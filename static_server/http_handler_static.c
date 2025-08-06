#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#include <sys/socket.h>
#endif

#include "../core/http_handler.h"
#include "../core/server.h"
#include "../core/logger.h"
#include "../core/file_utils.h"

void send_response(int client_socket, const char *status, const char *content_type, const char *body, int body_len)
{
    char header[1024];
    time_t now = time(NULL);
    char date[100];
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Date: %s\r\n"
             "Server: Simple C Server\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, date, content_type, body_len);

    send(client_socket, header, strlen(header), 0);
    if (body_len > 0)
    {
        send(client_socket, body, body_len, 0);
    }
}

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

    if (received <= 0)
    {
        return;
    }

    buffer[received] = '\0';

    // 解析 HTTP 請求
    char method[16], path[256], version[16];
    if (sscanf(buffer, "%s %s %s", method, path, version) != 3)
    {
        send_response(client_socket, "400 Bad Request", "text/plain", "Bad Request", 11);
        return;
    }

    log_message(LOG_INFO, "%s %s", method, path);

    // 只支援 GET 方法
    if (strcmp(method, "GET") != 0)
    {
        send_response(client_socket, "405 Method Not Allowed", "text/plain", "Method Not Allowed", 18);
        return;
    }

    // 處理路徑
    if (strcmp(path, "/") == 0)
    {
        strcpy(path, "/index.html");
    }

    // 安全檢查：防止路徑遍歷
    if (strstr(path, "..") != NULL)
    {
        send_response(client_socket, "403 Forbidden", "text/plain", "Forbidden", 9);
        return;
    }

    // 建構完整檔案路徑
    char full_path[512];
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    snprintf(full_path, sizeof(full_path), "%s/www%s", cwd, path);

    // 讀取檔案
    char *file_content;
    int file_size = read_file(full_path, &file_content);

    if (file_size < 0)
    {
        // 檔案不存在，返回 404 頁面
        const char *not_found = "<html><body><h1>404 Not Found</h1></body></html>";
        send_response(client_socket, "404 Not Found", "text/html", not_found, strlen(not_found));
        log_message(LOG_WARNING, "File not found: %s", full_path);
    }
    else
    {
        // 根據副檔名決定 Content-Type
        const char *content_type = get_content_type(path);
        send_response(client_socket, "200 OK", content_type, file_content, file_size);
        free(file_content);
    }
}