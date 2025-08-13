// http_forward.c - 整合 port forwarding 到現有的 HTTP server
#include "server.h"
#include "port_forward.h"
#include "router.h"
#include "json.h"

// 全局 port forwarding 管理器
static ForwardManager *g_forward_manager = NULL;

// API: 獲取所有轉發規則
void api_get_forward_rules(Request *req, Response *res)
{
    if (!g_forward_manager)
    {
        set_json_response(res, 500, "{\"error\":\"Forward manager not initialized\"}");
        return;
    }

    JsonBuilder *json = json_create();
    json_start_array(json, "rules");

    pthread_mutex_lock(&g_forward_manager->rules_mutex);

    for (int i = 0; i < g_forward_manager->rule_count; i++)
    {
        ForwardRule *rule = &g_forward_manager->rules[i];

        json_start_object(json, NULL);
        json_add_number(json, "id", rule->id);
        json_add_number(json, "listen_port", rule->listen_port);
        json_add_string(json, "target_host", rule->target_host);
        json_add_number(json, "target_port", rule->target_port);
        json_add_bool(json, "active", rule->active);
        json_add_string(json, "description", rule->description);
        json_end_object(json);
    }

    pthread_mutex_unlock(&g_forward_manager->rules_mutex);

    json_end_array(json);
    set_json_response(res, 200, json_get_string(json));
    json_destroy(json);
}

// API: 添加新的轉發規則
void api_add_forward_rule(Request *req, Response *res)
{
    if (!g_forward_manager)
    {
        set_json_response(res, 500, "{\"error\":\"Forward manager not initialized\"}");
        return;
    }

    // 解析 JSON 請求
    int listen_port = 0;
    int target_port = 0;
    char target_host[256] = {0};
    char description[256] = {0};

    // 簡單的 JSON 解析（你可以使用現有的 json.c 模組）
    char *listen_str = strstr(req->body, "\"listen_port\":");
    char *target_host_str = strstr(req->body, "\"target_host\":");
    char *target_port_str = strstr(req->body, "\"target_port\":");
    char *desc_str = strstr(req->body, "\"description\":");

    if (listen_str)
    {
        sscanf(listen_str, "\"listen_port\":%d", &listen_port);
    }
    if (target_port_str)
    {
        sscanf(target_port_str, "\"target_port\":%d", &target_port);
    }
    if (target_host_str)
    {
        char *start = strchr(target_host_str, '"');
        if (start)
        {
            start++;
            start = strchr(start, '"');
            if (start)
            {
                start++;
                char *end = strchr(start, '"');
                if (end)
                {
                    int len = end - start;
                    if (len > 0 && len < sizeof(target_host))
                    {
                        strncpy(target_host, start, len);
                        target_host[len] = '\0';
                    }
                }
            }
        }
    }
    if (desc_str)
    {
        char *start = strchr(desc_str, '"');
        if (start)
        {
            start++;
            start = strchr(start, '"');
            if (start)
            {
                start++;
                char *end = strchr(start, '"');
                if (end)
                {
                    int len = end - start;
                    if (len > 0 && len < sizeof(description))
                    {
                        strncpy(description, start, len);
                        description[len] = '\0';
                    }
                }
            }
        }
    }

    // 驗證參數
    if (listen_port <= 0 || target_port <= 0 || strlen(target_host) == 0)
    {
        set_json_response(res, 400, "{\"error\":\"Invalid parameters\"}");
        return;
    }

    // 添加規則
    int rule_id = forward_add_rule(g_forward_manager, listen_port,
                                   target_host, target_port,
                                   strlen(description) > 0 ? description : NULL);

    if (rule_id > 0)
    {
        JsonBuilder *json = json_create();
        json_add_number(json, "id", rule_id);
        json_add_string(json, "status", "success");
        json_add_string(json, "message", "Rule added successfully");
        set_json_response(res, 201, json_get_string(json));
        json_destroy(json);

        // 如果服務正在運行，啟動新規則的監聽
        if (g_forward_manager->running)
        {
            pthread_t thread;
            pthread_create(&thread, NULL, forward_listener_thread,
                           &g_forward_manager->rules[rule_id - 1]);
            pthread_detach(thread);
        }
    }
    else
    {
        set_json_response(res, 500, "{\"error\":\"Failed to add rule\"}");
    }
}

// API: 刪除轉發規則
void api_delete_forward_rule(Request *req, Response *res)
{
    if (!g_forward_manager)
    {
        set_json_response(res, 500, "{\"error\":\"Forward manager not initialized\"}");
        return;
    }

    // 從 URL 路徑提取規則 ID (例如: /api/forward/:id)
    int rule_id = 0;
    char *id_str = strrchr(req->path, '/');
    if (id_str)
    {
        rule_id = atoi(id_str + 1);
    }

    if (rule_id <= 0)
    {
        set_json_response(res, 400, "{\"error\":\"Invalid rule ID\"}");
        return;
    }

    // 先禁用規則
    forward_toggle_rule(g_forward_manager, rule_id, false);

    // 刪除規則
    if (forward_remove_rule(g_forward_manager, rule_id) == 0)
    {
        JsonBuilder *json = json_create();
        json_add_string(json, "status", "success");
        json_add_string(json, "message", "Rule deleted successfully");
        set_json_response(res, 200, json_get_string(json));
        json_destroy(json);
    }
    else
    {
        set_json_response(res, 404, "{\"error\":\"Rule not found\"}");
    }
}

// API: 啟用/禁用規則
void api_toggle_forward_rule(Request *req, Response *res)
{
    if (!g_forward_manager)
    {
        set_json_response(res, 500, "{\"error\":\"Forward manager not initialized\"}");
        return;
    }

    // 從 URL 路徑提取規則 ID
    int rule_id = 0;
    char *id_str = strstr(req->path, "/forward/");
    if (id_str)
    {
        id_str += strlen("/forward/");
        char *end = strchr(id_str, '/');
        if (end)
        {
            *end = '\0';
            rule_id = atoi(id_str);
            *end = '/';
        }
    }

    if (rule_id <= 0)
    {
        set_json_response(res, 400, "{\"error\":\"Invalid rule ID\"}");
        return;
    }

    // 檢查是啟用還是禁用
    bool enable = strstr(req->path, "/enable") != NULL;

    if (forward_toggle_rule(g_forward_manager, rule_id, enable) == 0)
    {
        JsonBuilder *json = json_create();
        json_add_string(json, "status", "success");
        json_add_string(json, "action", enable ? "enabled" : "disabled");
        json_add_number(json, "rule_id", rule_id);
        set_json_response(res, 200, json_get_string(json));
        json_destroy(json);

        // 如果啟用規則且服務正在運行，啟動監聽
        if (enable && g_forward_manager->running)
        {
            pthread_t thread;
            pthread_create(&thread, NULL, forward_listener_thread,
                           &g_forward_manager->rules[rule_id - 1]);
            pthread_detach(thread);
        }
    }
    else
    {
        set_json_response(res, 404, "{\"error\":\"Rule not found\"}");
    }
}

// API: 啟動/停止轉發服務
void api_control_forward_service(Request *req, Response *res)
{
    if (!g_forward_manager)
    {
        set_json_response(res, 500, "{\"error\":\"Forward manager not initialized\"}");
        return;
    }

    bool start = strstr(req->path, "/start") != NULL;

    if (start)
    {
        if (forward_start_service(g_forward_manager) == 0)
        {
            set_json_response(res, 200, "{\"status\":\"success\",\"message\":\"Service started\"}");
        }
        else
        {
            set_json_response(res, 400, "{\"error\":\"Service already running\"}");
        }
    }
    else
    {
        forward_stop_service(g_forward_manager);
        set_json_response(res, 200, "{\"status\":\"success\",\"message\":\"Service stopped\"}");
    }
}

// 初始化 port forwarding HTTP API
void init_forward_api(void)
{
    // 初始化 forward manager
    g_forward_manager = forward_manager_init();

    if (!g_forward_manager)
    {
        log_error("Failed to initialize port forwarding manager");
        return;
    }

    // 註冊 API 路由
    router_add(HTTP_GET, "/api/forward/rules", api_get_forward_rules);
    router_add(HTTP_POST, "/api/forward/rules", api_add_forward_rule);
    router_add(HTTP_DELETE, "/api/forward/:id", api_delete_forward_rule);
    router_add(HTTP_PUT, "/api/forward/:id/enable", api_toggle_forward_rule);
    router_add(HTTP_PUT, "/api/forward/:id/disable", api_toggle_forward_rule);
    router_add(HTTP_POST, "/api/forward/service/start", api_control_forward_service);
    router_add(HTTP_POST, "/api/forward/service/stop", api_control_forward_service);

    log_info("Port forwarding API initialized");
}

// 清理 port forwarding API
void cleanup_forward_api(void)
{
    if (g_forward_manager)
    {
        forward_stop_service(g_forward_manager);
        forward_manager_cleanup(g_forward_manager);
        g_forward_manager = NULL;
    }
}