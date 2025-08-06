#ifndef JSON_H
#define JSON_H

// 簡單的 JSON 建構器
typedef struct
{
    char *buffer;
    size_t size;
    size_t capacity;
} JsonBuilder;

JsonBuilder *json_create(void);
void json_destroy(JsonBuilder *json);
void json_add_string(JsonBuilder *json, const char *key, const char *value);
void json_add_number(JsonBuilder *json, const char *key, int value);
void json_add_bool(JsonBuilder *json, const char *key, int value);
void json_start_object(JsonBuilder *json, const char *key);
void json_end_object(JsonBuilder *json);
void json_start_array(JsonBuilder *json, const char *key);
void json_end_array(JsonBuilder *json);
char *json_get_string(JsonBuilder *json);

// JSON 解析器 (簡單版本)
typedef struct
{
    char *key;
    char *value;
} JsonPair;

int json_parse_simple(const char *json_str, JsonPair *pairs, int max_pairs);
char *json_get_value(JsonPair *pairs, int count, const char *key);

#endif