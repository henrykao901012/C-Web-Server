// tunnel_server.c - 隧道服務器（運行在公網VPS）
#include "tunnel_common.h"
#include "logger.h"
#include <signal.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#endif

#define MAX_CLIENTS 100
#define HTTP_PORT 80
#define CONTROL_PORT 7000
#define DATA_PORT 7001

typedef struct
{
    int control_socket;       // 控制連接
    char subdomain[64];       // 子域名
    char token[64];           // 認證token
    time_t last_heartbeat;    // 最後心跳時間
    bool active;              // 是否活動
    uint32_t next_session_id; // 下一個會話ID
} TunnelClientInfo;

typedef struct
{
    int listen_port;                       // 監聽端口
    TunnelClientInfo clients[MAX_CLIENTS]; // 客戶端列表
    int client_count;                      // 客戶端數量
    pthread_mutex_t mutex;                 // 互斥鎖
    volatile bool running;                 // 運行狀態
} TunnelServer;

static TunnelServer *g_server = NULL;

// 前向宣告
static void *handle_http_request(void *arg);
static void *handle_control_connection(void *arg);
static void *http_server_thread(void *arg);
static void *control_server_thread(void *arg);

// 生成隨機子域名
static void generate_subdomain(char *subdomain, size_t len)
{
    const char *chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < len - 1; i++)
    {
        subdomain[i] = chars[rand() % 36];
    }
    subdomain[len - 1] = '\0';
}

// 查找客戶端（通過子域名）
static TunnelClientInfo *find_client_by_subdomain(const char *subdomain)
{
    for (int i = 0; i < g_server->client_count; i++)
    {
        if (strcmp(g_server->clients[i].subdomain, subdomain) == 0 &&
            g_server->clients[i].active)
        {
            return &g_server->clients[i];
        }
    }
    return NULL;
}

// 解析HTTP Host頭
static bool parse_host_header(const char *data, int len, char *subdomain, size_t subdomain_len)
{
    const char *host_start = strstr(data, "Host: ");
    if (!host_start)
    {
        return false;
    }

    host_start += 6; // Skip "Host: "
    const char *host_end = strstr(host_start, "\r\n");
    if (!host_end)
    {
        return false;
    }

    int host_len = host_end - host_start;
    char host[256];
    if (host_len >= sizeof(host))
    {
        return false;
    }

    strncpy(host, host_start, host_len);
    host[host_len] = '\0';

    // 提取子域名 (假設格式: subdomain.tunnel.example.com)
    char *dot = strchr(host, '.');
    if (!dot)
    {
        strncpy(subdomain, host, subdomain_len - 1);
    }
    else
    {
        int sub_len = dot - host;
        if (sub_len >= subdomain_len)
        {
            sub_len = subdomain_len - 1;
        }
        strncpy(subdomain, host, sub_len);
        subdomain[sub_len] = '\0';
    }

    return true;
}

// 處理HTTP請求
static void *handle_http_request(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[TUNNEL_BUFFER_SIZE];
    char subdomain[64];

    // 接收HTTP請求
    int len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0)
    {
        close(client_sock);
        return NULL;
    }
    buffer[len] = '\0';

    // 解析Host頭
    if (!parse_host_header(buffer, len, subdomain, sizeof(subdomain)))
    {
        const char *error_response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 22\r\n"
            "\r\n"
            "<h1>Bad Request</h1>\r\n";
        send(client_sock, error_response, strlen(error_response), 0);
        close(client_sock);
        return NULL;
    }

    // 查找對應的隧道客戶端
    pthread_mutex_lock(&g_server->mutex);
    TunnelClientInfo *client = find_client_by_subdomain(subdomain);

    if (!client)
    {
        pthread_mutex_unlock(&g_server->mutex);
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 45\r\n"
            "\r\n"
            "<h1>Tunnel Not Found</h1><p>No such tunnel</p>";
        send(client_sock, not_found, strlen(not_found), 0);
        close(client_sock);
        return NULL;
    }

    // 生成會話ID
    uint32_t session_id = client->next_session_id++;

    // 通知客戶端建立數據連接
    tunnel_send_msg(client->control_socket, TUNNEL_CONNECT, session_id, NULL, 0);
    pthread_mutex_unlock(&g_server->mutex);

    // 等待數據隧道連接
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    // 在數據端口監聽
    int data_listen = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DATA_PORT);

    int opt = 1;
#ifdef _WIN32
    setsockopt(data_listen, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(data_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // 等待客戶端數據連接（超時5秒）
    struct timeval tv = {5, 0};
#ifdef _WIN32
    int timeout_ms = 5000; // Windows 使用毫秒
    setsockopt(data_listen, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout_ms, sizeof(timeout_ms));
#else
    setsockopt(data_listen, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    int data_sock = accept(data_listen, (struct sockaddr *)&addr, &addr_len);
    close(data_listen);

    if (data_sock < 0)
    {
        log_error("Timeout waiting for data tunnel");
        close(client_sock);
        return NULL;
    }

    // 驗證會話ID
    TunnelHeader header;
    char temp[16];
    if (tunnel_recv_msg(data_sock, &header, temp, sizeof(temp)) < 0 ||
        header.type != TUNNEL_ACCEPT ||
        header.session_id != session_id)
    {
        log_error("Invalid data tunnel handshake");
        close(data_sock);
        close(client_sock);
        return NULL;
    }

    log_info("Data tunnel established for session %u", session_id);

    // 發送原始HTTP請求到隧道
    tunnel_send_msg(data_sock, TUNNEL_DATA, session_id, buffer, len);

    // 雙向數據轉發
    fd_set read_fds;
    int max_fd = (client_sock > data_sock) ? client_sock : data_sock;

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(client_sock, &read_fds);
        FD_SET(data_sock, &read_fds);

        struct timeval timeout = {30, 0}; // 30秒超時
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret <= 0)
        {
            break;
        }

        // HTTP客戶端 -> 隧道
        if (FD_ISSET(client_sock, &read_fds))
        {
            len = recv(client_sock, buffer, sizeof(buffer), 0);
            if (len <= 0)
                break;

            if (tunnel_send_msg(data_sock, TUNNEL_DATA, session_id, buffer, len) < 0)
            {
                break;
            }
        }

        // 隧道 -> HTTP客戶端
        if (FD_ISSET(data_sock, &read_fds))
        {
            if (tunnel_recv_msg(data_sock, &header, buffer, sizeof(buffer)) < 0)
            {
                break;
            }

            if (header.type == TUNNEL_CLOSE)
            {
                break;
            }

            if (header.type == TUNNEL_DATA)
            {
                send(client_sock, buffer, header.data_len, 0);
            }
        }
    }

    // 清理
    tunnel_send_msg(data_sock, TUNNEL_CLOSE, session_id, NULL, 0);
    close(data_sock);
    close(client_sock);

    log_info("HTTP request handled for session %u", session_id);
    return NULL;
}

// 處理控制連接
static void *handle_control_connection(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);

    TunnelHeader header;
    char buffer[4096];

    // 接收連接請求
    if (tunnel_recv_msg(client_sock, &header, buffer, sizeof(buffer)) < 0 ||
        header.type != TUNNEL_CONNECT)
    {
        close(client_sock);
        return NULL;
    }

    ConnectRequest *req = (ConnectRequest *)buffer;

    // 分配客戶端槽位
    pthread_mutex_lock(&g_server->mutex);

    if (g_server->client_count >= MAX_CLIENTS)
    {
        pthread_mutex_unlock(&g_server->mutex);
        tunnel_send_msg(client_sock, TUNNEL_REJECT, 0, NULL, 0);
        close(client_sock);
        return NULL;
    }

    TunnelClientInfo *client = &g_server->clients[g_server->client_count++];
    client->control_socket = client_sock;
    strncpy(client->token, req->token, sizeof(client->token) - 1);
    client->active = true;
    client->last_heartbeat = time(NULL);
    client->next_session_id = 1;

    // 生成子域名
    if (strlen(req->subdomain) > 0)
    {
        strncpy(client->subdomain, req->subdomain, sizeof(client->subdomain) - 1);
    }
    else
    {
        generate_subdomain(client->subdomain, 8);
    }

    pthread_mutex_unlock(&g_server->mutex);

    // 發送域名分配
    DomainAssignment assign;
    snprintf(assign.public_url, sizeof(assign.public_url),
             "http://%s.tunnel.example.com", client->subdomain);
    assign.public_port = HTTP_PORT;

    tunnel_send_msg(client_sock, TUNNEL_ASSIGN_DOMAIN, 0, &assign, sizeof(assign));

    log_info("Client connected: %s -> %s", client->token, assign.public_url);

    // 處理控制消息
    while (client->active)
    {
#ifdef _WIN32
        int timeout_ms = 5000;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv = {5, 0};
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        int ret = tunnel_recv_msg(client_sock, &header, buffer, sizeof(buffer));

        if (ret < 0)
        {
            // 檢查是否超時
            if (time(NULL) - client->last_heartbeat > 30)
            {
                log_warning("Client timeout: %s", client->subdomain);
                break;
            }
            continue;
        }

        client->last_heartbeat = time(NULL);

        switch (header.type)
        {
        case TUNNEL_HEARTBEAT:
            // 心跳響應已在接收時更新
            break;

        case TUNNEL_CLOSE:
            client->active = false;
            break;

        default:
            log_warning("Unexpected message type: %d", header.type);
        }
    }

    // 清理
    pthread_mutex_lock(&g_server->mutex);
    client->active = false;
    // 移除客戶端（簡化處理，實際應該調整數組）
    pthread_mutex_unlock(&g_server->mutex);

    close(client_sock);
    log_info("Client disconnected: %s", client->subdomain);

    return NULL;
}

// HTTP服務器線程
static void *http_server_thread(void *arg)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(HTTP_PORT);

    int opt = 1;
#ifdef _WIN32
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_error("Failed to bind HTTP port %d", HTTP_PORT);
        return NULL;
    }

    listen(listen_sock, 100);
    log_info("HTTP server listening on port %d", HTTP_PORT);

    while (g_server->running)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);

        if (*client_sock < 0)
        {
            free(client_sock);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_http_request, client_sock);
        pthread_detach(thread);
    }

    close(listen_sock);
    return NULL;
}

// 控制服務器線程
static void *control_server_thread(void *arg)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(CONTROL_PORT);

    int opt = 1;
#ifdef _WIN32
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_error("Failed to bind control port %d", CONTROL_PORT);
        return NULL;
    }

    listen(listen_sock, 100);
    log_info("Control server listening on port %d", CONTROL_PORT);

    while (g_server->running)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);

        if (*client_sock < 0)
        {
            free(client_sock);
            continue;
        }

        log_info("Control connection from %s", inet_ntoa(client_addr.sin_addr));

        pthread_t thread;
        pthread_create(&thread, NULL, handle_control_connection, client_sock);
        pthread_detach(thread);
    }

    close(listen_sock);
    return NULL;
}

// 信號處理
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nShutting down tunnel server...\n");
        if (g_server)
        {
            g_server->running = false;
        }
    }
}

// 主函數
int main(int argc, char *argv[])
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    init_logger("tunnel_server.log");
    signal(SIGINT, signal_handler);
    srand(time(NULL));

    // 創建服務器
    TunnelServer server;
    memset(&server, 0, sizeof(server));
    server.running = true;
    pthread_mutex_init(&server.mutex, NULL);
    g_server = &server;

    printf("========================================\n");
    printf("  Tunnel Server Starting...\n");
    printf("  HTTP Port: %d\n", HTTP_PORT);
    printf("  Control Port: %d\n", CONTROL_PORT);
    printf("  Data Port: %d\n", DATA_PORT);
    printf("========================================\n\n");

    // 啟動服務線程
    pthread_t http_thread, control_thread;
    pthread_create(&http_thread, NULL, http_server_thread, NULL);
    pthread_create(&control_thread, NULL, control_server_thread, NULL);

    // 等待線程結束
    pthread_join(http_thread, NULL);
    pthread_join(control_thread, NULL);

    // 清理
    pthread_mutex_destroy(&server.mutex);
    close_logger();

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}