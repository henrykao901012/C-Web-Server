#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#include "../core/server.h"
#include "../core/http_handler.h"
#include "../core/logger.h"
#include "router.h"
#include "json.h"

// 模擬的資料庫
typedef struct
{
    int id;
    char name[100];
    char email[100];
} User;

static User users[100];
static int user_count = 0;
static int server_socket = -1;

void cleanup()
{
    if (server_socket != -1)
    {
#ifdef _WIN32
        closesocket(server_socket);
        WSACleanup();
#else
        close(server_socket);
#endif
    }
}

void signal_handler(int sig)
{
    log_message(LOG_INFO, "Shutting down server...");
    cleanup();
    exit(0);
}

// API 處理函數

// GET /api/users - 取得所有使用者
void get_users(Request *req, Response *res)
{
    char response[4096] = "{\"users\":[";

    for (int i = 0; i < user_count; i++)
    {
        if (i > 0)
            strcat(response, ",");

        char user_json[256];
        sprintf(user_json, "{\"id\":%d,\"name\":\"%s\",\"email\":\"%s\"}",
                users[i].id, users[i].name, users[i].email);
        strcat(response, user_json);
    }

    strcat(response, "]}");
    set_json_response(res, 200, response);
}

// GET /api/users/:id - 取得特定使用者
void get_user(Request *req, Response *res)
{
    char *id_str = get_query_param(req, "id");
    if (!id_str)
    {
        set_json_response(res, 400, "{\"error\":\"Missing user ID\"}");
        return;
    }

    int id = atoi(id_str);

    for (int i = 0; i < user_count; i++)
    {
        if (users[i].id == id)
        {
            JsonBuilder *json = json_create();
            json_add_number(json, "id", users[i].id);
            json_add_string(json, "name", users[i].name);
            json_add_string(json, "email", users[i].email);

            set_json_response(res, 200, json_get_string(json));
            json_destroy(json);
            return;
        }
    }

    set_json_response(res, 404, "{\"error\":\"User not found\"}");
}

// POST /api/users - 建立新使用者
void create_user(Request *req, Response *res)
{
    if (!req->body || req->body_length == 0)
    {
        set_json_response(res, 400, "{\"error\":\"Missing request body\"}");
        return;
    }

    // 解析 JSON
    JsonPair pairs[10];
    int pair_count = json_parse_simple(req->body, pairs, 10);

    char *name = json_get_value(pairs, pair_count, "name");
    char *email = json_get_value(pairs, pair_count, "email");

    if (!name || !email)
    {
        set_json_response(res, 400, "{\"error\":\"Missing name or email\"}");
        return;
    }

    // 建立新使用者
    User *new_user = &users[user_count];
    new_user->id = user_count + 1;
    strcpy(new_user->name, name);
    strcpy(new_user->email, email);
    user_count++;

    // 回傳新使用者
    JsonBuilder *json = json_create();
    json_add_number(json, "id", new_user->id);
    json_add_string(json, "name", new_user->name);
    json_add_string(json, "email", new_user->email);

    set_json_response(res, 201, json_get_string(json));
    json_destroy(json);

    // 清理
    for (int i = 0; i < pair_count; i++)
    {
        free(pairs[i].key);
        free(pairs[i].value);
    }
}

// GET /api/time - 取得伺服器時間
void get_time(Request *req, Response *res)
{
    time_t now = time(NULL);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    JsonBuilder *json = json_create();
    json_add_string(json, "time", time_str);
    json_add_number(json, "timestamp", (int)now);

    set_json_response(res, 200, json_get_string(json));
    json_destroy(json);
}

// 首頁
void home_page(Request *req, Response *res)
{
    const char *html =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>C Web Framework Demo</title>\n"
        "    <style>\n"
        "        body { font-family: Arial; margin: 40px; }\n"
        "        .endpoint { background: #f0f0f0; padding: 10px; margin: 10px 0; }\n"
        "        code { background: #e0e0e0; padding: 2px 5px; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>C Web Framework Demo</h1>\n"
        "    <h2>Available API Endpoints:</h2>\n"
        "    <div class='endpoint'>\n"
        "        <strong>GET /api/time</strong> - Get current server time\n"
        "    </div>\n"
        "    <div class='endpoint'>\n"
        "        <strong>GET /api/users</strong> - Get all users\n"
        "    </div>\n"
        "    <div class='endpoint'>\n"
        "        <strong>GET /api/users/:id</strong> - Get specific user\n"
        "    </div>\n"
        "    <div class='endpoint'>\n"
        "        <strong>POST /api/users</strong> - Create new user<br>\n"
        "        Body: <code>{\"name\": \"John\", \"email\": \"john@example.com\"}</code>\n"
        "    </div>\n"
        "    <script>\n"
        "        // Demo: Fetch current time\n"
        "        fetch('/api/time')\n"
        "            .then(r => r.json())\n"
        "            .then(data => console.log('Server time:', data));\n"
        "    </script>\n"
        "</body>\n"
        "</html>";

    set_response(res, 200, "text/html", html);
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;

    if (argc > 1)
    {
        port = atoi(argv[1]);
    }

    // 設置信號處理
    signal(SIGINT, signal_handler);
#ifndef _WIN32
    signal(SIGTERM, signal_handler);
#endif

    // 初始化
    init_logger("server.log");
    router_init();

    // 設定路由
    router_add(HTTP_GET, "/", home_page);
    router_add(HTTP_GET, "/api/time", get_time);
    router_add(HTTP_GET, "/api/users", get_users);
    router_add(HTTP_GET, "/api/users/:id", get_user);
    router_add(HTTP_POST, "/api/users", create_user);

    // 加入一些預設使用者
    strcpy(users[0].name, "Alice");
    strcpy(users[0].email, "alice@example.com");
    users[0].id = 1;

    strcpy(users[1].name, "Bob");
    strcpy(users[1].email, "bob@example.com");
    users[1].id = 2;

    user_count = 2;

    // 啟動伺服器
    server_socket = start_server(port);
    if (server_socket < 0)
    {
        log_message(LOG_ERROR, "Failed to start server");
        return 1;
    }

    log_message(LOG_INFO, "API Server started on port %d", port);
    printf("API Server running on http://localhost:%d\n", port);
    printf("Visit http://localhost:%d for API documentation\n", port);
    printf("Press Ctrl+C to stop\n");

    // 執行伺服器
    run_server(server_socket);

    // 清理
    router_cleanup();
    close_logger();
    return 0;
}