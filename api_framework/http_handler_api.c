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
#include "router.h"

int router_enabled = 1; // 框架模式啟用路由

static void send_response_with_headers(int client_socket, Response *res)
{
    char header[2048];
    time_t now = time(NULL);
    char date[100];
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    const char *status_text = "OK";
    switch (res->status_code)
    {
    case 200:
        status_text = "OK";
        break;
    case 201:
        status_text = "Created";
        break;
    case 400:
        status_text = "Bad Request";
        break;
    case 404:
        status_text = "Not Found";
        break;
    case 500:
        status_text = "Internal Server Error";
        break;
    }

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Date: %s\r\n"
             "Server: Simple C Server\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
             "Access-Control-Allow-Headers: Content-Type\r\n"
             "Connection: close\r\n"
             "%s"
             "\r\n",
             res->status_code, status_text, date, res->content_type, res->body_length,
             res->headers ? res->headers : "");

    send(client_socket, header, strlen(header), 0);
    if (res->body_length > 0)
    {
        send(client_socket, res->body, res->body_length, 0);
    }
}

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
        return;
    }

    // 處理 OPTIONS 請求 (CORS)
    if (strcmp(method, "OPTIONS") == 0)
    {
        const char *cors_response = "HTTP/1.1 200 OK\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                                    "Access-Control-Allow-Headers: Content-Type\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n";
        send(client_socket, cors_response, strlen(cors_response), 0);
        return;
    }

    log_message(LOG_INFO, "%s %s", method, path);

    Request req = {0};
    Response res = {0};

    // 解析請求
    req.method = parse_method(method);

    // 分離路徑和查詢字串
    char *query = strchr(path, '?');
    if (query)
    {
        *query = '\0';
        query++;
        req.query_string = query;
        parse_query_string(&req, query);
    }

    req.path = path;
    req.headers = buffer;

    // 解析請求體（如果有）
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start)
    {
        body_start += 4;
        req.body = body_start;
        req.body_length = received - (body_start - buffer);
    }

    // 處理路由
    router_handle(&req, &res);

    // 發送回應
    send_response_with_headers(client_socket, &res);

    // 清理
    if (res.content_type)
        free(res.content_type);
    if (res.body)
        free(res.body);
    if (res.headers)
        free(res.headers);
}