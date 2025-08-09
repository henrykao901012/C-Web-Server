#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "logger.h"

static Route routes[MAX_ROUTES];
static int route_count = 0;

void router_init(void)
{
    route_count = 0;
    memset(routes, 0, sizeof(routes));
}

void router_add(HttpMethod method, const char *pattern, RouteHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        log_message(LOG_ERROR, "Maximum routes reached");
        return;
    }

    routes[route_count].method = method;
    routes[route_count].pattern = strdup(pattern);
    routes[route_count].handler = handler;
    route_count++;

    log_message(LOG_INFO, "Added route: %s %s", get_method_string(method), pattern);
}

// 解析路徑參數 (例如 /users/:id 匹配 /users/123)
static int match_route(const char *pattern, const char *path, Request *req)
{
    // 完全匹配的情況
    if (strcmp(pattern, path) == 0)
    {
        return 1;
    }

    // 帶參數的路由匹配
    char pattern_copy[256];
    char path_copy[256];
    strcpy(pattern_copy, pattern);
    strcpy(path_copy, path);

    // 如果路徑結尾有 /，移除它
    int plen = strlen(pattern_copy);
    int pathlen = strlen(path_copy);
    if (plen > 1 && pattern_copy[plen - 1] == '/')
        pattern_copy[plen - 1] = '\0';
    if (pathlen > 1 && path_copy[pathlen - 1] == '/')
        path_copy[pathlen - 1] = '\0';

    char *pattern_token = strtok(pattern_copy, "/");
    char *path_token = strtok(path_copy, "/");

    req->param_count = 0;

    while (pattern_token && path_token)
    {
        if (pattern_token[0] == ':')
        {
            // 參數
            strcpy(req->params[req->param_count], pattern_token + 1);
            strcpy(req->param_values[req->param_count], path_token);
            req->param_count++;
        }
        else if (strcmp(pattern_token, path_token) != 0)
        {
            // 不匹配
            return 0;
        }

        pattern_token = strtok(NULL, "/");
        path_token = strtok(NULL, "/");
    }

    // 兩者都應該結束
    return pattern_token == NULL && path_token == NULL;
}

void router_handle(Request *req, Response *res)
{
    log_message(LOG_DEBUG, "Router handling path: %s, method: %s", req->path, get_method_string(req->method));

    for (int i = 0; i < route_count; i++)
    {
        if (routes[i].method == req->method)
        {
            if (match_route(routes[i].pattern, req->path, req))
            {
                log_message(LOG_INFO, "Matched route: %s", routes[i].pattern);
                routes[i].handler(req, res);
                return;
            }
        }
    }

    // 沒有匹配的路由，返回 404
    log_message(LOG_WARNING, "No route matched for: %s", req->path);
    set_response(res, 404, "application/json", "{\"error\":\"Not Found\"}");
}

void router_cleanup(void)
{
    for (int i = 0; i < route_count; i++)
    {
        free(routes[i].pattern);
    }
    route_count = 0;
}

const char *get_method_string(HttpMethod method)
{
    switch (method)
    {
    case HTTP_GET:
        return "GET";
    case HTTP_POST:
        return "POST";
    case HTTP_PUT:
        return "PUT";
    case HTTP_DELETE:
        return "DELETE";
    case HTTP_PATCH:
        return "PATCH";
    default:
        return "UNKNOWN";
    }
}

HttpMethod parse_method(const char *method_str)
{
    if (strcmp(method_str, "GET") == 0)
        return HTTP_GET;
    if (strcmp(method_str, "POST") == 0)
        return HTTP_POST;
    if (strcmp(method_str, "PUT") == 0)
        return HTTP_PUT;
    if (strcmp(method_str, "DELETE") == 0)
        return HTTP_DELETE;
    if (strcmp(method_str, "PATCH") == 0)
        return HTTP_PATCH;
    return HTTP_GET;
}

void parse_query_string(Request *req, const char *query)
{
    if (!query || strlen(query) == 0)
        return;

    char query_copy[1024];
    strcpy(query_copy, query);

    char *token = strtok(query_copy, "&");
    while (token && req->param_count < MAX_PARAMS)
    {
        char *equals = strchr(token, '=');
        if (equals)
        {
            *equals = '\0';
            strcpy(req->params[req->param_count], token);
            strcpy(req->param_values[req->param_count], equals + 1);
            req->param_count++;
        }
        token = strtok(NULL, "&");
    }
}

char *get_query_param(Request *req, const char *name)
{
    for (int i = 0; i < req->param_count; i++)
    {
        if (strcmp(req->params[i], name) == 0)
        {
            return req->param_values[i];
        }
    }
    return NULL;
}

void set_response(Response *res, int status, const char *content_type, const char *body)
{
    res->status_code = status;
    res->content_type = strdup(content_type);
    res->body = strdup(body);
    res->body_length = strlen(body);
}

void set_json_response(Response *res, int status, const char *json)
{
    set_response(res, status, "application/json", json);
}
