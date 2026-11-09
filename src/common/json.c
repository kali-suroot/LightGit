/*
 * json.c — 轻量 JSON 解析器实现（递归下降）
 *
 * 仅实现项目所需的 JSON 子集，足够解析 GitHub REST API 响应。
 * 字符串支持 \uXXXX 转义（按 UTF-8 输出）；数字以 double 存储，
 * 对 GitHub 的 ID（< 2^53）可精确取整。
 */
#include "lightgit/common/json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * 解析器状态
 * -------------------------------------------------------------------------- */
typedef struct LgJsonParser {
    const char *p;
    const char *end;
} LgJsonParser;

static lg_json_value *LgJsonParseValue(LgJsonParser *ps);

static void LgSkipWs(LgJsonParser *ps)
{
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' ||
                               *ps->p == '\n' || *ps->p == '\r')) {
        ps->p++;
    }
}

static lg_json_value *LgJsonNew(lg_json_type t)
{
    lg_json_value *v = (lg_json_value *)calloc(1, sizeof(*v));
    if (v != NULL) {
        v->type = t;
    }
    return v;
}

static int LgJsonAppendItem(lg_json_value *container, lg_json_value *item, const char *key)
{
    lg_json_value **ni;
    char **nk = NULL;
    size_t n = container->count;

    if (container->type == LG_JSON_OBJECT) {
        nk = (char **)realloc(container->keys, (n + 1) * sizeof(char *));
        if (nk == NULL) {
            return -1;
        }
        container->keys = nk;
        container->keys[n] = key ? _strdup(key) : NULL;
    }

    ni = (lg_json_value **)realloc(container->items, (n + 1) * sizeof(lg_json_value *));
    if (ni == NULL) {
        return -1;
    }
    container->items = ni;
    container->items[n] = item;
    container->count = n + 1;
    return 0;
}

/* 解析字符串（调用前 *ps->p 应为 '"'），返回新节点或 NULL */
static lg_json_value *LgJsonParseString(LgJsonParser *ps)
{
    lg_json_value *v;
    char *buf;
    size_t cap = 32, len = 0;

    ps->p++;   /* 跳过起始引号 */

    buf = (char *)malloc(cap);
    if (buf == NULL) {
        return NULL;
    }

    while (ps->p < ps->end && *ps->p != '"') {
        char c = *ps->p++;

        if (c == '\\') {
            if (ps->p >= ps->end) {
                free(buf);
                return NULL;
            }
            c = *ps->p++;
            switch (c) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'u': {
                /* \uXXXX → UTF-8 */
                unsigned int cp = 0;
                int i;
                for (i = 0; i < 4; i++) {
                    char h = (ps->p < ps->end) ? *ps->p++ : 0;
                    int d;
                    if (h >= '0' && h <= '9') d = h - '0';
                    else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                    else { free(buf); return NULL; }
                    cp = (cp << 4) | (unsigned int)d;
                }
                /* 编码为 UTF-8 */
                if (cp < 0x80) {
                    c = (char)cp;
                    goto append;
                } else if (cp < 0x800) {
                    if (len + 2 >= cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
                    buf[len++] = (char)(0xC0 | (cp >> 6));
                    buf[len++] = (char)(0x80 | (cp & 0x3F));
                    continue;
                } else {
                    if (len + 3 >= cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
                    buf[len++] = (char)(0xE0 | (cp >> 12));
                    buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    buf[len++] = (char)(0x80 | (cp & 0x3F));
                    continue;
                }
            }
            default:
                free(buf);
                return NULL;
            }
        }

append:
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (nb == NULL) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        buf[len++] = c;
    }

    if (ps->p >= ps->end) {   /* 未闭合字符串 */
        free(buf);
        return NULL;
    }
    ps->p++;   /* 跳过结尾引号 */

    if (len + 1 >= cap) {
        char *nb = (char *)realloc(buf, len + 1);
        if (nb == NULL) { free(buf); return NULL; }
        buf = nb;
    }
    buf[len] = '\0';

    v = LgJsonNew(LG_JSON_STRING);
    if (v == NULL) {
        free(buf);
        return NULL;
    }
    v->string = buf;
    return v;
}

static lg_json_value *LgJsonParseNumber(LgJsonParser *ps)
{
    lg_json_value *v;
    const char *start = ps->p;

    while (ps->p < ps->end && (strchr("0123456789+-.eE", *ps->p) != NULL)) {
        ps->p++;
    }

    v = LgJsonNew(LG_JSON_NUMBER);
    if (v == NULL) {
        return NULL;
    }
    v->number = strtod(start, NULL);
    return v;
}

static lg_json_value *LgJsonParseArray(LgJsonParser *ps)
{
    lg_json_value *arr = LgJsonNew(LG_JSON_ARRAY);
    if (arr == NULL) {
        return NULL;
    }

    ps->p++;   /* 跳过 '[' */
    LgSkipWs(ps);
    if (ps->p < ps->end && *ps->p == ']') {
        ps->p++;
        return arr;   /* 空数组 */
    }

    for (;;) {
        lg_json_value *item = LgJsonParseValue(ps);
        if (item == NULL || LgJsonAppendItem(arr, item, NULL) != 0) {
            lg_json_free(item);
            lg_json_free(arr);
            return NULL;
        }
        LgSkipWs(ps);
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
            continue;
        }
        if (ps->p < ps->end && *ps->p == ']') {
            ps->p++;
            return arr;
        }
        lg_json_free(arr);
        return NULL;
    }
}

static lg_json_value *LgJsonParseObject(LgJsonParser *ps)
{
    lg_json_value *obj = LgJsonNew(LG_JSON_OBJECT);
    if (obj == NULL) {
        return NULL;
    }

    ps->p++;   /* 跳过 '{' */
    LgSkipWs(ps);
    if (ps->p < ps->end && *ps->p == '}') {
        ps->p++;
        return obj;   /* 空对象 */
    }

    for (;;) {
        lg_json_value *keyv, *val;
        const char *key;

        LgSkipWs(ps);
        if (ps->p >= ps->end || *ps->p != '"') {
            lg_json_free(obj);
            return NULL;
        }
        keyv = LgJsonParseString(ps);
        if (keyv == NULL) {
            lg_json_free(obj);
            return NULL;
        }
        key = keyv->string;

        LgSkipWs(ps);
        if (ps->p >= ps->end || *ps->p != ':') {
            lg_json_free(keyv);
            lg_json_free(obj);
            return NULL;
        }
        ps->p++;

        val = LgJsonParseValue(ps);
        if (val == NULL || LgJsonAppendItem(obj, val, key) != 0) {
            lg_json_free(val);
            lg_json_free(keyv);
            lg_json_free(obj);
            return NULL;
        }
        /* keyv->string 已由 LgJsonAppendItem 拷贝（_strdup），此处释放 keyv */
        lg_json_free(keyv);

        LgSkipWs(ps);
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
            continue;
        }
        if (ps->p < ps->end && *ps->p == '}') {
            ps->p++;
            return obj;
        }
        lg_json_free(obj);
        return NULL;
    }
}

static lg_json_value *LgJsonParseValue(LgJsonParser *ps)
{
    lg_json_value *v;

    LgSkipWs(ps);
    if (ps->p >= ps->end) {
        return NULL;
    }

    switch (*ps->p) {
    case '{': return LgJsonParseObject(ps);
    case '[': return LgJsonParseArray(ps);
    case '"': return LgJsonParseString(ps);
    case 't':
        if (ps->end - ps->p >= 4 && memcmp(ps->p, "true", 4) == 0) {
            ps->p += 4;
            v = LgJsonNew(LG_JSON_BOOL);
            if (v) v->boolean = 1;
            return v;
        }
        return NULL;
    case 'f':
        if (ps->end - ps->p >= 5 && memcmp(ps->p, "false", 5) == 0) {
            ps->p += 5;
            v = LgJsonNew(LG_JSON_BOOL);
            if (v) v->boolean = 0;
            return v;
        }
        return NULL;
    case 'n':
        if (ps->end - ps->p >= 4 && memcmp(ps->p, "null", 4) == 0) {
            ps->p += 4;
            return LgJsonNew(LG_JSON_NULL);
        }
        return NULL;
    default:
        if (*ps->p == '-' || (*ps->p >= '0' && *ps->p <= '9')) {
            return LgJsonParseNumber(ps);
        }
        return NULL;
    }
}

int lg_json_parse(const char *text, size_t len, lg_json_value **out)
{
    LgJsonParser ps;
    lg_json_value *v;

    if (out == NULL) {
        return -1;
    }
    *out = NULL;
    if (text == NULL) {
        return -1;
    }

    ps.p = text;
    ps.end = text + len;

    /* 跳过 UTF-8 BOM（EF BB BF），兼容带 BOM 的 JSON 文件 */
    if (len >= 3 && (unsigned char)ps.p[0] == 0xEF &&
        (unsigned char)ps.p[1] == 0xBB && (unsigned char)ps.p[2] == 0xBF) {
        ps.p += 3;
    }

    v = LgJsonParseValue(&ps);
    if (v == NULL) {
        return -1;
    }
    *out = v;
    return 0;
}

void lg_json_free(lg_json_value *v)
{
    size_t i;
    if (v == NULL) {
        return;
    }
    for (i = 0; i < v->count; i++) {
        lg_json_free(v->items[i]);
    }
    free(v->items);
    if (v->keys != NULL) {
        for (i = 0; i < v->count; i++) {
            free(v->keys[i]);
        }
        free(v->keys);
    }
    free(v->string);
    free(v);
}

lg_json_value *lg_json_get(const lg_json_value *obj, const char *key)
{
    size_t i;
    if (obj == NULL || obj->type != LG_JSON_OBJECT || key == NULL) {
        return NULL;
    }
    for (i = 0; i < obj->count; i++) {
        if (obj->keys[i] != NULL && strcmp(obj->keys[i], key) == 0) {
            return obj->items[i];
        }
    }
    return NULL;
}

lg_json_value *lg_json_at(const lg_json_value *arr, size_t i)
{
    if (arr == NULL || arr->type != LG_JSON_ARRAY || i >= arr->count) {
        return NULL;
    }
    return arr->items[i];
}

const char *lg_json_string(const lg_json_value *v)
{
    if (v == NULL || v->type != LG_JSON_STRING) {
        return NULL;
    }
    return v->string;
}

int lg_json_int64(const lg_json_value *v, long long *out)
{
    if (v == NULL || v->type != LG_JSON_NUMBER || out == NULL) {
        return -1;
    }
    *out = (long long)v->number;
    return 0;
}

int lg_json_bool(const lg_json_value *v, int *out)
{
    if (v == NULL || v->type != LG_JSON_BOOL || out == NULL) {
        return -1;
    }
    *out = v->boolean;
    return 0;
}

const char *lg_json_get_string(const lg_json_value *obj, const char *key)
{
    return lg_json_string(lg_json_get(obj, key));
}
