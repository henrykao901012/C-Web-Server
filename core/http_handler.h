#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

// 全域變數，決定是否使用路由器
extern int router_enabled;

// 處理客戶端連線
void handle_client(int client_socket);

// 發送 HTTP 回應（舊版相容）
void send_response(int client_socket, const char *status, const char *content_type, const char *body, int body_len);

#endif