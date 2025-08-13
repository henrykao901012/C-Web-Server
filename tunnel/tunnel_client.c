// tunnel_client.c - 隧道客戶端（運行在本地）
#include "tunnel_common.h"
#include "logger.h"
#include <signal.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#endif

typedef struct
{
    int control_socket;    // 控制連接
    int local_port;        // 本地服務端口
    char server_host[256]; // 隧道服務器地址
    int server_port;       // 隧道服務器端口
    char public_url[256];  // 分配的公網URL
    char token[64];        // 認證token
    volatile bool running; // 運行狀態
} TunnelClient;

static TunnelClient *g_client = NULL;

// 連接到本地服務
static int connect_local_service(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}

// 處理數據隧道
static void *handle_data_tunnel(void *arg)
{
    TunnelConnection *conn = (TunnelConnection *)arg;
    char buffer[TUNNEL_BUFFER_SIZE];

    log_info("Data tunnel established for session %u", conn->session_id);

    // 創建兩個線程分別處理雙向數據
    // 1. 從隧道讀取，寫入本地
    // 2. 從本地讀取，通過隧道發送

    fd_set read_fds;
    int max_fd;

    while (conn->active)
    {
        FD_ZERO(&read_fds);
        FD_SET(conn->control_socket, &read_fds);
        FD_SET(conn->local_socket, &read_fds);

        max_fd = (conn->control_socket > conn->local_socket) ? conn->control_socket : conn->local_socket;

        struct timeval tv = {1, 0}; // 1秒超時
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (ret < 0)
        {
            break;
        }

        // 從隧道接收數據，轉發到本地
        if (FD_ISSET(conn->control_socket, &read_fds))
        {
            TunnelHeader header;
            int len = tunnel_recv_msg(conn->control_socket, &header, buffer, sizeof(buffer));

            if (len <= 0 || header.type == TUNNEL_CLOSE)
            {
                break;
            }

            if (header.type == TUNNEL_DATA)
            {
                send(conn->local_socket, buffer, len, 0);
            }
        }

        // 從本地接收數據，通過隧道發送
        if (FD_ISSET(conn->local_socket, &read_fds))
        {
            int len = recv(conn->local_socket, buffer, sizeof(buffer), 0);

            if (len <= 0)
            {
                break;
            }

            tunnel_send_msg(conn->control_socket, TUNNEL_DATA,
                            conn->session_id, buffer, len);
        }
    }

    // 清理
    tunnel_send_msg(conn->control_socket, TUNNEL_CLOSE, conn->session_id, NULL, 0);
    close(conn->local_socket);
    close(conn->control_socket);
    free(conn);

    log_info("Data tunnel closed for session %u", conn->session_id);
    return NULL;
}

// 處理控制連接
static void *control_thread(void *arg)
{
    TunnelClient *client = (TunnelClient *)arg;
    TunnelHeader header;
    char buffer[4096];

    // 發送連接請求
    ConnectRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.token, client->token, sizeof(req.token) - 1);
    req.token[sizeof(req.token) - 1] = '\0'; // 確保字符串結尾
    req.local_port = client->local_port;

    if (tunnel_send_msg(client->control_socket, TUNNEL_CONNECT, 0, &req, sizeof(req)) < 0)
    {
        log_error("Failed to send connect request");
        return NULL;
    }

    // 等待服務器響應
    if (tunnel_recv_msg(client->control_socket, &header, buffer, sizeof(buffer)) < 0)
    {
        log_error("Failed to receive server response");
        return NULL;
    }

    if (header.type == TUNNEL_REJECT)
    {
        log_error("Connection rejected by server");
        return NULL;
    }

    if (header.type == TUNNEL_ASSIGN_DOMAIN)
    {
        DomainAssignment *assign = (DomainAssignment *)buffer;
        strcpy(client->public_url, assign->public_url);

        printf("\n");
        printf("========================================\n");
        printf("  Tunnel established successfully!\n");
        printf("  Public URL: %s\n", client->public_url);
        printf("  Forwarding to: localhost:%d\n", client->local_port);
        printf("========================================\n\n");

        log_info("Tunnel established: %s -> localhost:%d",
                 client->public_url, client->local_port);
    }

    // 主循環：處理來自服務器的請求
    while (client->running)
    {
        int ret = tunnel_recv_msg(client->control_socket, &header, buffer, sizeof(buffer));

        if (ret < 0)
        {
            log_error("Control connection lost");
            break;
        }

        switch (header.type)
        {
        case TUNNEL_CONNECT:
        {
            // 服務器請求建立新的數據連接
            log_info("New connection request for session %u", header.session_id);

            // 連接到本地服務
            int local_sock = connect_local_service(client->local_port);
            if (local_sock < 0)
            {
                log_error("Failed to connect to local service on port %d",
                          client->local_port);
                tunnel_send_msg(client->control_socket, TUNNEL_CLOSE,
                                header.session_id, NULL, 0);
                break;
            }

            // 創建新的數據隧道連接
            int data_sock = tunnel_connect_server(client->server_host,
                                                  client->server_port + 1);
            if (data_sock < 0)
            {
                log_error("Failed to create data tunnel");
                close(local_sock);
                break;
            }

            // 發送會話ID
            tunnel_send_msg(data_sock, TUNNEL_ACCEPT, header.session_id, NULL, 0);

            // 創建連接結構
            TunnelConnection *conn = malloc(sizeof(TunnelConnection));
            conn->control_socket = data_sock;
            conn->local_socket = local_sock;
            conn->session_id = header.session_id;
            conn->active = true;

            // 啟動數據轉發線程
            pthread_t thread;
            pthread_create(&thread, NULL, handle_data_tunnel, conn);
            pthread_detach(thread);
            break;
        }

        case TUNNEL_HEARTBEAT:
            // 回應心跳
            tunnel_send_msg(client->control_socket, TUNNEL_HEARTBEAT, 0, NULL, 0);
            break;

        default:
            log_warning("Unknown message type: %d", header.type);
        }
    }

    return NULL;
}

// 信號處理
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nShutting down tunnel client...\n");
        if (g_client)
        {
            g_client->running = false;
        }
    }
}

// 主函數
int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: %s <server_host> <server_port> <local_port> [token]\n", argv[0]);
        printf("Example: %s tunnel.example.com 7000 8080\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    init_logger("tunnel_client.log");
    signal(SIGINT, signal_handler);

    // 創建客戶端
    TunnelClient client;
    memset(&client, 0, sizeof(client));
    strncpy(client.server_host, argv[1], sizeof(client.server_host) - 1);
    client.server_port = atoi(argv[2]);
    client.local_port = atoi(argv[3]);

    if (argc > 4)
    {
        strncpy(client.token, argv[4], sizeof(client.token) - 1);
    }
    else
    {
        strcpy(client.token, "default");
    }

    client.running = true;
    g_client = &client;

    printf("Connecting to tunnel server %s:%d...\n",
           client.server_host, client.server_port);

    // 連接到隧道服務器
    client.control_socket = tunnel_connect_server(client.server_host, client.server_port);
    if (client.control_socket < 0)
    {
        fprintf(stderr, "Failed to connect to tunnel server\n");
        return 1;
    }

    log_info("Connected to tunnel server");

    // 啟動控制線程
    pthread_t control_tid;
    pthread_create(&control_tid, NULL, control_thread, &client);

    // 等待退出
    pthread_join(control_tid, NULL);

    // 清理
    close(client.control_socket);
    close_logger();

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}