/*
 * hex.c — 十六进制编解码实现
 */
#include "lightgit/common/hex.h"

/* 十六进制数字表（小写，与 Git 输出一致） */
static const char g_hex_digits[] = "0123456789abcdef";

/* 解析单个十六进制字符为 0~15；非法返回 -1 */
static int LgHexNibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void lg_hex_encode(const unsigned char *bin, size_t len, char *out)
{
    size_t i;
    if (bin == NULL || out == NULL) {
        return;
    }
    for (i = 0; i < len; i++) {
        out[2 * i]     = g_hex_digits[bin[i] >> 4];
        out[2 * i + 1] = g_hex_digits[bin[i] & 0x0F];
    }
    out[2 * len] = '\0';
}

int lg_hex_decode(const char *hex, unsigned char *out, size_t out_len)
{
    size_t i;

    if (hex == NULL || out == NULL) {
        return -1;
    }

    for (i = 0; i < out_len; i++) {
        int hi = LgHexNibble(hex[2 * i]);
        int lo = LgHexNibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return (int)out_len;
}
