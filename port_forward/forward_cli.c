// forward_cli.c - Port Forwarding CLI 界面
#include "port_forward.h"
#include <signal.h>
#include <ctype.h>

static ForwardManager *g_manager = NULL;
static volatile int g_running = 1;

// 信號處理
void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nShutting down...\n");
        g_running = 0;
    }
}

// 顯示幫助信息
void show_help()
{
    printf("\n=== Port Forwarding Commands ===\n");
    printf("  add <listen_port> <target_host> <target_port> [description]\n");
    printf("      - Add a new forwarding rule\n");
    printf("  remove <rule_id>\n");
    printf("      - Remove a forwarding rule\n");
    printf("  enable <rule_id>\n");
    printf("      - Enable a forwarding rule\n");
    printf("  disable <rule_id>\n");
    printf("      - Disable a forwarding rule\n");
    printf("  list\n");
    printf("      - List all forwarding rules\n");
    printf("  start\n");
    printf("      - Start forwarding service\n");
    printf("  stop\n");
    printf("      - Stop forwarding service\n");
    printf("  save [filename]\n");
    printf("      - Save rules to config file\n");
    printf("  load [filename]\n");
    printf("      - Load rules from config file\n");
    printf("  help\n");
    printf("      - Show this help message\n");
    printf("  quit/exit\n");
    printf("      - Exit the program\n\n");
}

// 解析並執行命令
void process_command(char *input)
{
    char command[50];
    char arg1[256], arg2[256], arg3[256], arg4[512];

    // 清理輸入
    char *newline = strchr(input, '\n');
    if (newline)
        *newline = '\0';

    // 解析命令
    int args = sscanf(input, "%s %s %s %s %[^\n]",
                      command, arg1, arg2, arg3, arg4);

    if (args < 1)
        return;

    // 轉換命令為小寫
    for (char *p = command; *p; p++)
    {
        *p = tolower(*p);
    }

    // 執行命令
    if (strcmp(command, "add") == 0)
    {
        if (args >= 4)
        {
            int listen_port = atoi(arg1);
            int target_port = atoi(arg3);
            char *desc = (args >= 5) ? arg4 : NULL;

            int id = forward_add_rule(g_manager, listen_port,
                                      arg2, target_port, desc);
            if (id > 0)
            {
                printf("Added rule with ID: %d\n", id);
            }
            else
            {
                printf("Failed to add rule\n");
            }
        }
        else
        {
            printf("Usage: add <listen_port> <target_host> <target_port> [description]\n");
        }
    }
    else if (strcmp(command, "remove") == 0)
    {
        if (args >= 2)
        {
            int id = atoi(arg1);
            if (forward_remove_rule(g_manager, id) == 0)
            {
                printf("Removed rule %d\n", id);
            }
            else
            {
                printf("Failed to remove rule %d\n", id);
            }
        }
        else
        {
            printf("Usage: remove <rule_id>\n");
        }
    }
    else if (strcmp(command, "enable") == 0)
    {
        if (args >= 2)
        {
            int id = atoi(arg1);
            if (forward_toggle_rule(g_manager, id, true) == 0)
            {
                printf("Enabled rule %d\n", id);
            }
            else
            {
                printf("Failed to enable rule %d\n", id);
            }
        }
        else
        {
            printf("Usage: enable <rule_id>\n");
        }
    }
    else if (strcmp(command, "disable") == 0)
    {
        if (args >= 2)
        {
            int id = atoi(arg1);
            if (forward_toggle_rule(g_manager, id, false) == 0)
            {
                printf("Disabled rule %d\n", id);
            }
            else
            {
                printf("Failed to disable rule %d\n", id);
            }
        }
        else
        {
            printf("Usage: disable <rule_id>\n");
        }
    }
    else if (strcmp(command, "list") == 0)
    {
        forward_list_rules(g_manager);
    }
    else if (strcmp(command, "start") == 0)
    {
        if (forward_start_service(g_manager) == 0)
        {
            printf("Port forwarding service started\n");
        }
        else
        {
            printf("Failed to start service (already running?)\n");
        }
    }
    else if (strcmp(command, "stop") == 0)
    {
        forward_stop_service(g_manager);
        printf("Port forwarding service stopped\n");
    }
    else if (strcmp(command, "save") == 0)
    {
        const char *filename = (args >= 2) ? arg1 : "forward.conf";
        int count = forward_save_config(g_manager, filename);
        if (count >= 0)
        {
            printf("Saved %d rules to %s\n", count, filename);
        }
        else
        {
            printf("Failed to save configuration\n");
        }
    }
    else if (strcmp(command, "load") == 0)
    {
        const char *filename = (args >= 2) ? arg1 : "forward.conf";
        int count = forward_load_config(g_manager, filename);
        if (count >= 0)
        {
            printf("Loaded %d rules from %s\n", count, filename);
        }
        else
        {
            printf("Failed to load configuration\n");
        }
    }
    else if (strcmp(command, "help") == 0)
    {
        show_help();
    }
    else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0)
    {
        g_running = 0;
    }
    else
    {
        printf("Unknown command: %s (type 'help' for commands)\n", command);
    }
}

int main(int argc, char *argv[])
{
    printf("\n=== C Web Server - Port Forwarding Module ===\n");
    printf("Type 'help' for available commands\n\n");

// 初始化 Winsock (Windows)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed\n");
        return 1;
    }
#endif

    // 設置信號處理
    signal(SIGINT, signal_handler);

    // 初始化管理器
    g_manager = forward_manager_init();
    if (!g_manager)
    {
        printf("Failed to initialize port forwarding manager\n");
        return 1;
    }

    // 嘗試載入默認配置
    if (argc > 1)
    {
        int count = forward_load_config(g_manager, argv[1]);
        if (count > 0)
        {
            printf("Loaded %d rules from %s\n", count, argv[1]);
            forward_list_rules(g_manager);
        }
    }

    // 主循環
    char input[1024];
    while (g_running)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        process_command(input);
    }

    // 清理
    forward_stop_service(g_manager);
    forward_manager_cleanup(g_manager);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("\nGoodbye!\n");
    return 0;
}