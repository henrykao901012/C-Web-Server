// tunnel_common.c - 隧道協議通用實現
#include "tunnel_common.h"

#ifdef _WIN32
#include <winsock2.h>
#define close closesocket
#else
#include <arpa/inet.h> // for htonl, ntohl
#endif

// 發送隧道消息
int tunnel_send_msg(int socket, TunnelMsgType type, uint32_t session_id,
                    const void *data, size_t data_len)
{
    TunnelHeader header;
    header.magic = htonl(TUNNEL_MAGIC);
    header.type = htonl(type);
    header.session_id = htonl(session_id);
    header.data_len = htonl(data_len);

    // 發送頭部
    if (send(socket, (char *)&header, sizeof(header), 0) != sizeof(header))
    {
        return -1;
    }

    // 發送數據（如果有）
    if (data && data_len > 0)
    {
        size_t total_sent = 0;
        while (total_sent < data_len)
        {
            int sent = send(socket, (char *)data + total_sent,
                            data_len - total_sent, 0);
            if (sent <= 0)
            {
                return -1;
            }
            total_sent += sent;
        }
    }

    return 0;
}

// 接收隧道消息
int tunnel_recv_msg(int socket, TunnelHeader *header, void *data, size_t max_len)
{
    // 接收頭部
    char header_buf[sizeof(TunnelHeader)];
    size_t total_recv = 0;

    while (total_recv < sizeof(TunnelHeader))
    {
        int recv_len = recv(socket, header_buf + total_recv,
                            sizeof(TunnelHeader) - total_recv, 0);
        if (recv_len <= 0)
        {
            return -1;
        }
        total_recv += recv_len;
    }

    memcpy(header, header_buf, sizeof(TunnelHeader));

    // 轉換字節序
    header->magic = ntohl(header->magic);
    header->type = ntohl(header->type);
    header->session_id = ntohl(header->session_id);
    header->data_len = ntohl(header->data_len);

    // 驗證魔術數字
    if (header->magic != TUNNEL_MAGIC)
    {
        return -1;
    }

    // 接收數據
    if (header->data_len > 0)
    {
        if (header->data_len > max_len)
        {
            return -1; // 數據太大
        }

        total_recv = 0;
        while (total_recv < header->data_len)
        {
            int recv_len = recv(socket, (char *)data + total_recv,
                                header->data_len - total_recv, 0);
            if (recv_len <= 0)
            {
                return -1;
            }
            total_recv += recv_len;
        }
    }

    return header->data_len;
}

// 創建控制連接
int tunnel_connect_server(const char *server_host, int server_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return -1;
    }

    // 解析服務器地址
    struct hostent *server = gethostbyname(server_host);
    if (!server)
    {
        close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    addr.sin_port = htons(server_port);

    // 連接
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}