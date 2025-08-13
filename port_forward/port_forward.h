// port_forward.h - Port Forwarding 功能頭文件
#ifndef PORT_FORWARD_H
#define PORT_FORWARD_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
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

#define FORWARD_BUFFER_SIZE 8192
#define MAX_FORWARD_RULES 10

// Port forwarding 規則結構
typedef struct
{
    int id;
    int listen_port;       // 監聽端口
    char target_host[256]; // 目標主機
    int target_port;       // 目標端口
    bool active;           // 是否啟用
    char description[256]; // 規則描述
} ForwardRule;

// Port forwarding 連線處理結構
typedef struct
{
    int client_socket;
    int target_socket;
    ForwardRule *rule;
} ForwardConnection;

// 雙向資料傳輸結構
typedef struct
{
    int from_socket;
    int to_socket;
    char *direction; // "client->target" 或 "target->client"
} PipeData;

// Port forwarding 管理器
typedef struct
{
    ForwardRule rules[MAX_FORWARD_RULES];
    int rule_count;
    pthread_mutex_t rules_mutex;
    bool running;
} ForwardManager;

// 初始化 port forwarding 管理器
ForwardManager *forward_manager_init(void);

// 添加 port forwarding 規則
int forward_add_rule(ForwardManager *manager, int listen_port,
                     const char *target_host, int target_port,
                     const char *description);

// 刪除 port forwarding 規則
int forward_remove_rule(ForwardManager *manager, int rule_id);

// 啟用/禁用規則
int forward_toggle_rule(ForwardManager *manager, int rule_id, bool enable);

// 列出所有規則
void forward_list_rules(ForwardManager *manager);

// 啟動 port forwarding 服務
int forward_start_service(ForwardManager *manager);

// 停止 port forwarding 服務
void forward_stop_service(ForwardManager *manager);

// 清理資源
void forward_manager_cleanup(ForwardManager *manager);

// 從配置文件載入規則
int forward_load_config(ForwardManager *manager, const char *config_file);

// 保存規則到配置文件
int forward_save_config(ForwardManager *manager, const char *config_file);

#endif // PORT_FORWARD_H