// port_forward.c - Port Forwarding 功能實現
#include "port_forward.h"
#include "logger.h"

// 全局變數
static int g_logger_initialized = 0;

// 前向聲明所有內部函數
static void *handle_forward_connection(void *arg);
static void *pipe_data(void *arg);
static void *forward_listener_thread(void *arg);
static int create_target_connection(const char *host, int port);
static void ensure_logger_init(void);

// 確保 logger 已初始化
static void ensure_logger_init(void)
{
    if (!g_logger_initialized)
    {
        init_logger("portforward.log");
        g_logger_initialized = 1;
    }
}

// 初始化 port forwarding 管理器
ForwardManager *forward_manager_init(void)
{
    ensure_logger_init();

    ForwardManager *manager = (ForwardManager *)malloc(sizeof(ForwardManager));
    if (!manager)
    {
        log_error("Failed to allocate memory for ForwardManager");
        return NULL;
    }

    memset(manager, 0, sizeof(ForwardManager));
    manager->rule_count = 0;
    manager->running = false;
    pthread_mutex_init(&manager->rules_mutex, NULL);

    log_info("Port forwarding manager initialized");
    return manager;
}

// 添加 port forwarding 規則
int forward_add_rule(ForwardManager *manager, int listen_port,
                     const char *target_host, int target_port,
                     const char *description)
{
    if (!manager || manager->rule_count >= MAX_FORWARD_RULES)
    {
        log_error("Cannot add rule: manager full or invalid");
        return -1;
    }

    pthread_mutex_lock(&manager->rules_mutex);

    ForwardRule *rule = &manager->rules[manager->rule_count];
    rule->id = manager->rule_count + 1;
    rule->listen_port = listen_port;
    strncpy(rule->target_host, target_host, sizeof(rule->target_host) - 1);
    rule->target_host[sizeof(rule->target_host) - 1] = '\0';
    rule->target_port = target_port;
    rule->active = true;

    if (description)
    {
        strncpy(rule->description, description, sizeof(rule->description) - 1);
        rule->description[sizeof(rule->description) - 1] = '\0';
    }
    else
    {
        snprintf(rule->description, sizeof(rule->description),
                 "Forward %d -> %s:%d", listen_port, target_host, target_port);
    }

    manager->rule_count++;

    pthread_mutex_unlock(&manager->rules_mutex);

    log_info("Added forwarding rule: %s", rule->description);
    return rule->id;
}

// 刪除 port forwarding 規則
int forward_remove_rule(ForwardManager *manager, int rule_id)
{
    if (!manager || rule_id <= 0 || rule_id > manager->rule_count)
    {
        log_error("Invalid rule ID: %d", rule_id);
        return -1;
    }

    pthread_mutex_lock(&manager->rules_mutex);

    // 將後面的規則向前移動
    for (int i = rule_id - 1; i < manager->rule_count - 1; i++)
    {
        manager->rules[i] = manager->rules[i + 1];
        manager->rules[i].id = i + 1;
    }

    manager->rule_count--;

    pthread_mutex_unlock(&manager->rules_mutex);

    log_info("Removed forwarding rule ID: %d", rule_id);
    return 0;
}

// 啟用/禁用規則
int forward_toggle_rule(ForwardManager *manager, int rule_id, bool enable)
{
    if (!manager || rule_id <= 0 || rule_id > manager->rule_count)
    {
        return -1;
    }

    pthread_mutex_lock(&manager->rules_mutex);
    manager->rules[rule_id - 1].active = enable;
    pthread_mutex_unlock(&manager->rules_mutex);

    log_info("Rule %d %s", rule_id, enable ? "enabled" : "disabled");
    return 0;
}

// 列出所有規則
void forward_list_rules(ForwardManager *manager)
{
    if (!manager)
        return;

    pthread_mutex_lock(&manager->rules_mutex);

    printf("\n=== Port Forwarding Rules ===\n");
    printf("ID | Status  | Listen Port | Target              | Description\n");
    printf("---|---------|-------------|---------------------|-------------\n");

    for (int i = 0; i < manager->rule_count; i++)
    {
        ForwardRule *rule = &manager->rules[i];
        printf("%2d | %-7s | %11d | %s:%d | %s\n",
               rule->id,
               rule->active ? "Active" : "Inactive",
               rule->listen_port,
               rule->target_host,
               rule->target_port,
               rule->description);
    }

    if (manager->rule_count == 0)
    {
        printf("No forwarding rules configured.\n");
    }

    printf("\n");
    pthread_mutex_unlock(&manager->rules_mutex);
}

// 創建到目標服務器的連接
static int create_target_connection(const char *host, int port)
{
#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
#endif
        log_error("Failed to create socket");
        return -1;
    }

    struct hostent *server = gethostbyname(host);
    if (!server)
    {
        log_error("Failed to resolve host: %s", host);
        close(sock);
        return -1;
    }

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    memcpy(&target_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    target_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0)
    {
        log_error("Failed to connect to %s:%d", host, port);
        close(sock);
        return -1;
    }

    log_debug("Connected to target %s:%d", host, port);
    return sock;
}

// 資料管道傳輸線程
static void *pipe_data(void *arg)
{
    PipeData *pipe = (PipeData *)arg;
    char buffer[FORWARD_BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(pipe->from_socket, buffer, sizeof(buffer), 0)) > 0)
    {
        int total_sent = 0;
        while (total_sent < bytes_read)
        {
            int sent = send(pipe->to_socket, buffer + total_sent,
                            bytes_read - total_sent, 0);
            if (sent <= 0)
            {
                log_debug("Connection closed in %s", pipe->direction);
                goto cleanup;
            }
            total_sent += sent;
        }
        log_debug("Forwarded %d bytes %s", bytes_read, pipe->direction);
    }

cleanup:
#ifdef _WIN32
    shutdown(pipe->from_socket, SD_BOTH);
    shutdown(pipe->to_socket, SD_BOTH);
#else
    shutdown(pipe->from_socket, SHUT_RDWR);
    shutdown(pipe->to_socket, SHUT_RDWR);
#endif
    free(pipe);
    return NULL;
}

// 處理單個轉發連接
static void *handle_forward_connection(void *arg)
{
    ForwardConnection *conn = (ForwardConnection *)arg;

    // 連接到目標服務器
    conn->target_socket = create_target_connection(
        conn->rule->target_host,
        conn->rule->target_port);

    if (conn->target_socket < 0)
    {
        log_error("Failed to connect to target for rule: %s",
                  conn->rule->description);
        close(conn->client_socket);
        free(conn);
        return NULL;
    }

    log_info("Established forward connection: %s", conn->rule->description);

    // 創建雙向數據管道
    PipeData *client_to_target = (PipeData *)malloc(sizeof(PipeData));
    client_to_target->from_socket = conn->client_socket;
    client_to_target->to_socket = conn->target_socket;
    client_to_target->direction = "client->target";

    PipeData *target_to_client = (PipeData *)malloc(sizeof(PipeData));
    target_to_client->from_socket = conn->target_socket;
    target_to_client->to_socket = conn->client_socket;
    target_to_client->direction = "target->client";

    // 啟動雙向傳輸線程
    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, pipe_data, client_to_target);
    pthread_create(&thread2, NULL, pipe_data, target_to_client);

    // 等待任一方向結束
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // 清理連接
    close(conn->client_socket);
    close(conn->target_socket);
    free(conn);

    log_info("Forward connection closed");
    return NULL;
}

// 監聽器線程
static void *forward_listener_thread(void *arg)
{
    ForwardRule *rule = (ForwardRule *)arg;

#ifdef _WIN32
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
    {
#else
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
#endif
        log_error("Failed to create listen socket for port %d", rule->listen_port);
        return NULL;
    }

    // 設置 socket 選項
    int opt = 1;
#ifdef _WIN32
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // 綁定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(rule->listen_port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_error("Failed to bind to port %d", rule->listen_port);
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 10) < 0)
    {
        log_error("Failed to listen on port %d", rule->listen_port);
        close(listen_sock);
        return NULL;
    }

    log_info("Listening on port %d for rule: %s",
             rule->listen_port, rule->description);

    // 接受連接
    while (rule->active)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

#ifdef _WIN32
        SOCKET client_sock = accept(listen_sock,
                                    (struct sockaddr *)&client_addr,
                                    &client_len);
        if (client_sock == INVALID_SOCKET)
        {
#else
        int client_sock = accept(listen_sock,
                                 (struct sockaddr *)&client_addr,
                                 &client_len);
        if (client_sock < 0)
        {
#endif
            if (rule->active)
            {
                log_error("Accept failed on port %d", rule->listen_port);
            }
            continue;
        }

        log_info("Accepted connection on port %d from %s",
                 rule->listen_port,
                 inet_ntoa(client_addr.sin_addr));

        // 創建連接處理結構
        ForwardConnection *conn = (ForwardConnection *)malloc(sizeof(ForwardConnection));
        conn->client_socket = client_sock;
        conn->rule = rule;

        // 啟動處理線程
        pthread_t thread;
        pthread_create(&thread, NULL, handle_forward_connection, conn);
        pthread_detach(thread);
    }

    close(listen_sock);
    log_info("Stopped listening on port %d", rule->listen_port);
    return NULL;
}

// 啟動 port forwarding 服務
int forward_start_service(ForwardManager *manager)
{
    if (!manager || manager->running)
    {
        return -1;
    }

    manager->running = true;

    pthread_mutex_lock(&manager->rules_mutex);

    // 為每個活動規則啟動監聽線程
    for (int i = 0; i < manager->rule_count; i++)
    {
        if (manager->rules[i].active)
        {
            pthread_t thread;
            pthread_create(&thread, NULL, forward_listener_thread,
                           &manager->rules[i]);
            pthread_detach(thread);
        }
    }

    pthread_mutex_unlock(&manager->rules_mutex);

    log_info("Port forwarding service started");
    return 0;
}

// 停止 port forwarding 服務
void forward_stop_service(ForwardManager *manager)
{
    if (!manager || !manager->running)
    {
        return;
    }

    manager->running = false;

    // 停用所有規則
    pthread_mutex_lock(&manager->rules_mutex);
    for (int i = 0; i < manager->rule_count; i++)
    {
        manager->rules[i].active = false;
    }
    pthread_mutex_unlock(&manager->rules_mutex);

    log_info("Port forwarding service stopped");
}

// 從配置文件載入規則
int forward_load_config(ForwardManager *manager, const char *config_file)
{
    FILE *file = fopen(config_file, "r");
    if (!file)
    {
        log_error("Cannot open config file: %s", config_file);
        return -1;
    }

    char line[512];
    int rules_loaded = 0;

    while (fgets(line, sizeof(line), file))
    {
        // 跳過註釋和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        int listen_port, target_port;
        char target_host[256] = {0};
        char description[256] = {0};

        // 嘗試解析有描述的格式
        if (sscanf(line, "%d %255s %d %255[^\n\r]",
                   &listen_port, target_host, &target_port, description) >= 3)
        {
            // 如果沒有描述，description會是空字符串
            forward_add_rule(manager, listen_port, target_host,
                             target_port,
                             description[0] ? description : NULL);
            rules_loaded++;
        }
    }

    fclose(file);
    log_info("Loaded %d rules from %s", rules_loaded, config_file);
    return rules_loaded;
}

// 保存規則到配置文件
int forward_save_config(ForwardManager *manager, const char *config_file)
{
    FILE *file = fopen(config_file, "w");
    if (!file)
    {
        log_error("Cannot create config file: %s", config_file);
        return -1;
    }

    fprintf(file, "# Port Forwarding Configuration\n");
    fprintf(file, "# Format: listen_port target_host target_port description\n\n");

    pthread_mutex_lock(&manager->rules_mutex);

    for (int i = 0; i < manager->rule_count; i++)
    {
        ForwardRule *rule = &manager->rules[i];
        fprintf(file, "%d %s %d %s\n",
                rule->listen_port, rule->target_host,
                rule->target_port, rule->description);
    }

    pthread_mutex_unlock(&manager->rules_mutex);

    fclose(file);
    log_info("Saved %d rules to %s", manager->rule_count, config_file);
    return manager->rule_count;
}

// 清理資源
void forward_manager_cleanup(ForwardManager *manager)
{
    if (!manager)
        return;

    forward_stop_service(manager);
    pthread_mutex_destroy(&manager->rules_mutex);
    free(manager);

    log_info("Port forwarding manager cleaned up");

    // 關閉 logger
    if (g_logger_initialized)
    {
        close_logger();
        g_logger_initialized = 0;
    }
}