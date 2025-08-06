#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "../core/server.h"
#include "../core/http_handler.h"
#include "../core/logger.h"

int server_socket = -1;
int router_enabled = 0; // 不使用路由

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

    // 初始化日誌
    init_logger("server.log");

    // 啟動伺服器
    server_socket = start_server(port);
    if (server_socket < 0)
    {
        log_message(LOG_ERROR, "Failed to start server");
        return 1;
    }

    log_message(LOG_INFO, "Server started on port %d", port);
    printf("Web server running on http://localhost:%d\n", port);
    printf("Press Ctrl+C to stop\n");

    // 主循環
    run_server(server_socket);

    cleanup();
    close_logger();
    return 0;
}