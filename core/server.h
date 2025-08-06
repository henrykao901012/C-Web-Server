#ifndef SERVER_H
#define SERVER_H

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100

int start_server(int port);
void run_server(int server_socket);

#endif