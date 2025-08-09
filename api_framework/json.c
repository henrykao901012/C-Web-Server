#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

JsonBuilder *json_create(void)
{
    JsonBuilder *json = malloc(sizeof(JsonBuilder));
    json->capacity = 1024;
    json->buffer = malloc(json->capacity);
    json->buffer[0] = '{';
    json->buffer[1] = '\0';
    json->size = 1;
    return json;
}

void json_destroy(JsonBuilder *json)
{
    free(json->buffer);
    free(json);
}

static void json_ensure_capacity(JsonBuilder *json, size_t needed)
{
    if (json->size + needed > json->capacity)
    {
        json->capacity *= 2;
        json->buffer = realloc(json->buffer, json->capacity);
    }
}

static void json_add_comma(JsonBuilder *json)
{
    if (json->size > 1 && json->buffer[json->size - 1] != '{' && json->buffer[json->size - 1] != '[')
    {
        json_ensure_capacity(json, 2);
        strcat(json->buffer, ",");
        json->size++;
    }
}

void json_add_string(JsonBuilder *json, const char *key, const char *value)
{
    json_add_comma(json);
    size_t needed = strlen(key) + strlen(value) + 10;
    json_ensure_capacity(json, needed);

    char temp[1024];
    sprintf(temp, "\"%s\":\"%s\"", key, value);
    strcat(json->buffer, temp);
    json->size += strlen(temp);
}

void json_add_number(JsonBuilder *json, const char *key, int value)
{
    json_add_comma(json);
    size_t needed = strlen(key) + 20;
    json_ensure_capacity(json, needed);

    char temp[256];
    sprintf(temp, "\"%s\":%d", key, value);
    strcat(json->buffer, temp);
    json->size += strlen(temp);
}

void json_add_bool(JsonBuilder *json, const char *key, int value)
{
    json_add_comma(json);
    size_t needed = strlen(key) + 20;
    json_ensure_capacity(json, needed);

    char temp[256];
    sprintf(temp, "\"%s\":%s", key, value ? "true" : "false");
    strcat(json->buffer, temp);
    json->size += strlen(temp);
}

void json_start_array(JsonBuilder *json, const char *key)
{
    json_add_comma(json);
    size_t needed = strlen(key) + 10;
    json_ensure_capacity(json, needed);

    char temp[256];
    sprintf(temp, "\"%s\":[", key);
    strcat(json->buffer, temp);
    json->size += strlen(temp);
}

void json_end_array(JsonBuilder *json)
{
    json_ensure_capacity(json, 2);
    strcat(json->buffer, "]");
    json->size++;
}

char *json_get_string(JsonBuilder *json)
{
    json_ensure_capacity(json, 2);
    strcat(json->buffer, "}");
    return json->buffer;
}

// 簡單的 JSON 解析（只支援一層）
int json_parse_simple(const char *json_str, JsonPair *pairs, int max_pairs)
{
    char buffer[4096];
    strcpy(buffer, json_str);

    // 移除 { }
    char *start = strchr(buffer, '{');
    char *end = strrchr(buffer, '}');
    if (!start || !end)
        return 0;

    *end = '\0';
    start++;

    int count = 0;
    char *token = strtok(start, ",");

    while (token && count < max_pairs)
    {
        // 找到鍵值對
        char *colon = strchr(token, ':');
        if (colon)
        {
            *colon = '\0';
            char *key = token;
            char *value = colon + 1;

            // 移除引號和空格
            while (*key == ' ' || *key == '"')
                key++;
            while (*value == ' ' || *value == '"')
                value++;

            char *key_end = key + strlen(key) - 1;
            while (*key_end == ' ' || *key_end == '"')
                *key_end-- = '\0';

            char *value_end = value + strlen(value) - 1;
            while (*value_end == ' ' || *value_end == '"')
                *value_end-- = '\0';

            pairs[count].key = strdup(key);
            pairs[count].value = strdup(value);
            count++;
        }

        token = strtok(NULL, ",");
    }

    return count;
}

char *json_get_value(JsonPair *pairs, int count, const char *key)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(pairs[i].key, key) == 0)
        {
            return pairs[i].value;
        }
    }
    return NULL;
}