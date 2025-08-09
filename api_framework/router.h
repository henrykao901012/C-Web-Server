#ifndef ROUTER_H
#define ROUTER_H

#include <stddef.h>

#define MAX_ROUTES 100
#define MAX_PARAMS 10

// HTTP 方法
typedef enum
{
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH
} HttpMethod;

// 請求結構
typedef struct
{
    HttpMethod method;
    char *path;
    char *body;
    int body_length;
    char *query_string;
    char *headers;
    char params[MAX_PARAMS][256];
    char param_values[MAX_PARAMS][256];
    int param_count;
} Request;

// 回應結構
typedef struct
{
    int status_code;
    char *content_type;
    char *body;
    int body_length;
    char *headers;
} Response;

// 路由處理函數類型
typedef void (*RouteHandler)(Request *req, Response *res);

// 路由結構
typedef struct
{
    HttpMethod method;
    char *pattern;
    RouteHandler handler;
} Route;

// 路由器函數
void router_init(void);
void router_add(HttpMethod method, const char *pattern, RouteHandler handler);
void router_handle(Request *req, Response *res);
void router_cleanup(void);

// 輔助函數
const char *get_method_string(HttpMethod method);
HttpMethod parse_method(const char *method_str);
void parse_query_string(Request *req, const char *query);
char *get_query_param(Request *req, const char *name);
void set_response(Response *res, int status, const char *content_type, const char *body);
void set_json_response(Response *res, int status, const char *json);

#endif