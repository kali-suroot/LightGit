/*
 * json.h — 轻量 JSON 解析器（自行实现，不引入 cJSON）
 *
 * 支持 RFC 8259 的常用子集：对象/数组/字符串/数字/布尔/null，
 * 用于解析 GitHub REST API 响应。解析结果是一棵树，可按键/下标访问。
 */
#ifndef LIGHTGIT_COMMON_JSON_H
#define LIGHTGIT_COMMON_JSON_H

#include <stddef.h>

/* JSON 值类型 */
typedef enum lg_json_type {
    LG_JSON_NULL = 0,
    LG_JSON_BOOL,
    LG_JSON_NUMBER,
    LG_JSON_STRING,
    LG_JSON_ARRAY,
    LG_JSON_OBJECT
} lg_json_type;

/* JSON 值节点 */
typedef struct lg_json_value {
    lg_json_type      type;
    int               boolean;     /* LG_JSON_BOOL 时有效 */
    double            number;      /* LG_JSON_NUMBER 时有效 */
    char             *string;      /* LG_JSON_STRING 时有效（malloc） */
    struct lg_json_value **items;  /* 数组元素 / 对象值（malloc） */
    char            **keys;        /* 对象的键（数组为 NULL） */
    size_t            count;
} lg_json_value;

/*
 * 解析 JSON 文本，成功返回 0 并把根节点写入 *out（调用方 lg_json_free）。
 * 失败返回 -1。
 */
int lg_json_parse(const char *text, size_t len, lg_json_value **out);

/* 释放整棵树 */
void lg_json_free(lg_json_value *v);

/* 对象按键查找（找不到返回 NULL） */
lg_json_value *lg_json_get(const lg_json_value *obj, const char *key);

/* 数组按下标取元素（越界返回 NULL） */
lg_json_value *lg_json_at(const lg_json_value *arr, size_t i);

/* 取字符串值（非字符串类型返回 NULL） */
const char *lg_json_string(const lg_json_value *v);

/* 取整数值（数字类型，转为 long long；失败返回 -1） */
int lg_json_int64(const lg_json_value *v, long long *out);

/* 取布尔值（失败返回 -1） */
int lg_json_bool(const lg_json_value *v, int *out);

/* 便捷：从对象取字符串字段（不存在或非字符串返回 NULL） */
const char *lg_json_get_string(const lg_json_value *obj, const char *key);

#endif /* LIGHTGIT_COMMON_JSON_H */
