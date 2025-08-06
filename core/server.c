#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#endif

#include "server.h"
#include "http_handler.h"
#include "logger.h"

#ifdef _WIN32
DWORD WINAPI handle_client_thread(LPVOID arg)
{
#else
void *handle_client_thread(void *arg)
{
#endif
    int client_socket = *(int *)arg;
    free(arg);

    handle_client(client_socket);

#ifdef _WIN32
    closesocket(client_socket);
    return 0;
#else
    close(client_socket);
    return NULL;
#endif
}

int start_server(int port)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        log_message(LOG_ERROR, "WSAStartup failed");
        return -1;
    }
#endif

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        log_message(LOG_ERROR, "Failed to create socket");
        return -1;
    }

    // 允許重用地址
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        log_message(LOG_WARNING, "Failed to set socket options");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        log_message(LOG_ERROR, "Failed to bind socket");
#ifdef _WIN32
        closesocket(server_socket);
#else
        close(server_socket);
#endif
        return -1;
    }

    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        log_message(LOG_ERROR, "Failed to listen on socket");
#ifdef _WIN32
        closesocket(server_socket);
#else
        close(server_socket);
#endif
        return -1;
    }

    return server_socket;
}

void run_server(int server_socket)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1)
    {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            log_message(LOG_ERROR, "Failed to accept connection");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        log_message(LOG_INFO, "New connection from %s", client_ip);

        // 建立新執行緒處理客戶端
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = client_socket;

#ifdef _WIN32
        HANDLE thread = CreateThread(NULL, 0, handle_client_thread, client_socket_ptr, 0, NULL);
        if (thread == NULL)
        {
            log_message(LOG_ERROR, "Failed to create thread");
            free(client_socket_ptr);
            closesocket(client_socket);
        }
        else
        {
            CloseHandle(thread);
        }
#else
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client_thread, client_socket_ptr) != 0)
        {
            log_message(LOG_ERROR, "Failed to create thread");
            free(client_socket_ptr);
            close(client_socket);
        }
        else
        {
            pthread_detach(thread);
        }
#endif
    }
}