// tunnel_common.h - 隧道協議定義
#ifndef TUNNEL_COMMON_H
#define TUNNEL_COMMON_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h> // 加入 uint32_t 定義

#define TUNNEL_BUFFER_SIZE 65536
#define TUNNEL_MAGIC 0x54554E4C // "TUNL"
#define MAX_DOMAIN_LEN 256

// 隧道協議類型
typedef enum
{
    TUNNEL_CONNECT = 1,  // 客戶端連接請求
    TUNNEL_ACCEPT,       // 服務器接受連接
    TUNNEL_REJECT,       // 服務器拒絕連接
    TUNNEL_DATA,         // 數據傳輸
    TUNNEL_CLOSE,        // 關閉連接
    TUNNEL_HEARTBEAT,    // 心跳包
    TUNNEL_ASSIGN_DOMAIN // 分配域名
} TunnelMsgType;

// 隧道消息頭
typedef struct
{
    uint32_t magic;      // 魔術數字
    uint32_t type;       // 消息類型
    uint32_t session_id; // 會話ID
    uint32_t data_len;   // 數據長度
} TunnelHeader;

// 連接請求
typedef struct
{
    char token[64];     // 認證token
    int local_port;     // 本地端口
    char subdomain[64]; // 請求的子域名
} ConnectRequest;

// 域名分配響應
typedef struct
{
    char public_url[256]; // 分配的公網URL
    int public_port;      // 公網端口
} DomainAssignment;

// 隧道連接結構
typedef struct
{
    int control_socket;   // 控制連接
    int local_socket;     // 本地服務連接
    uint32_t session_id;  // 會話ID
    char public_url[256]; // 公網URL
    bool active;          // 是否活動
} TunnelConnection;

// 發送隧道消息
int tunnel_send_msg(int socket, TunnelMsgType type, uint32_t session_id,
                    const void *data, size_t data_len);

// 接收隧道消息
int tunnel_recv_msg(int socket, TunnelHeader *header, void *data, size_t max_len);

// 創建控制連接
int tunnel_connect_server(const char *server_host, int server_port);

// 數據轉發
void *tunnel_forward_data(void *arg);

#endif // TUNNEL_COMMON_H