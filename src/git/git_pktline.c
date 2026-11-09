/*
 * git_pktline.c — pkt-line 编码实现
 */
#include "lightgit/git/git_pktline.h"

#include <string.h>

/* 十六进制数字表 */
static const char g_pkt_hex[] = "0123456789abcdef";

size_t lg_pktline_encode(unsigned char *out, size_t out_cap,
                         const void *payload, size_t payload_len)
{
    size_t total;

    if (payload_len > LG_PKTLINE_MAX_PAYLOAD) {
        return 0;   /* 负载超长，无法用 pkt-line 表达 */
    }

    total = 4 + payload_len;   /* 长度字段本身计入前缀 */
    if (out == NULL || out_cap < total) {
        return 0;
    }

    /* 写 4 字节十六进制长度（含前缀自身） */
    out[0] = (unsigned char)g_pkt_hex[(total >> 12) & 0xF];
    out[1] = (unsigned char)g_pkt_hex[(total >> 8) & 0xF];
    out[2] = (unsigned char)g_pkt_hex[(total >> 4) & 0xF];
    out[3] = (unsigned char)g_pkt_hex[total & 0xF];

    if (payload_len > 0 && payload != NULL) {
        memcpy(out + 4, payload, payload_len);
    }
    return total;
}

size_t lg_pktline_encode_flush(unsigned char out[4])
{
    if (out != NULL) {
        out[0] = '0'; out[1] = '0'; out[2] = '0'; out[3] = '0';
    }
    return 4;
}

int lg_pktline_decode(const unsigned char *buf, size_t len, size_t offset,
                      const unsigned char **out_payload, size_t *out_payload_len)
{
    size_t total = 0;
    int i;

    if (out_payload != NULL) *out_payload = NULL;
    if (out_payload_len != NULL) *out_payload_len = 0;

    if (buf == NULL || offset + 4 > len) {
        return -1;
    }

    /* 解析 4 字节十六进制长度字段 */
    for (i = 0; i < 4; i++) {
        char c = (char)buf[offset + i];
        int v;
        if (c >= '0' && c <= '9')      v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return -1;
        total = (total << 4) | (unsigned int)v;
    }

    /* flush / delim / response-end：无负载 */
    if (total < 4) {
        return 4;
    }

    if (offset + total > len) {
        return -1;   /* 数据被截断 */
    }

    if (out_payload != NULL) {
        *out_payload = buf + offset + 4;
    }
    if (out_payload_len != NULL) {
        *out_payload_len = total - 4;
    }
    return (int)total;
}
