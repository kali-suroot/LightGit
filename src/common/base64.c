/*
 * base64.c — Base64 编解码实现
 */
#include "lightgit/common/base64.h"

static const char g_b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int LgB64Val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int lg_base64_encode(const unsigned char *in, size_t in_len, char *out, size_t out_cap)
{
    size_t i, o = 0;
    size_t need = ((in_len + 2) / 3) * 4 + 1;

    if (out == NULL || out_cap < need) {
        return -1;
    }

    for (i = 0; i < in_len; i += 3) {
        unsigned int n = (unsigned int)in[i] << 16;
        if (i + 1 < in_len) n |= (unsigned int)in[i + 1] << 8;
        if (i + 2 < in_len) n |= (unsigned int)in[i + 2];

        out[o++] = g_b64[(n >> 18) & 0x3F];
        out[o++] = g_b64[(n >> 12) & 0x3F];
        out[o++] = (i + 1 < in_len) ? g_b64[(n >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < in_len) ? g_b64[n & 0x3F] : '=';
    }
    out[o] = '\0';
    return (int)o;
}

int lg_base64_decode(const char *in, size_t in_len, unsigned char *out, size_t out_cap)
{
    size_t i, o = 0;
    size_t out_len;

    if (in == NULL || out == NULL || in_len % 4 != 0) {
        return -1;
    }

    /* 先计算输出长度并检查容量 */
    out_len = (in_len / 4) * 3;
    if (in_len > 0 && in[in_len - 1] == '=') out_len--;
    if (in_len > 1 && in[in_len - 2] == '=') out_len--;
    if (out_len > out_cap) {
        return -1;
    }

    for (i = 0; i < in_len; i += 4) {
        int v[4];
        int k;
        for (k = 0; k < 4; k++) {
            if (in[i + k] == '=') {
                v[k] = -1;
            } else {
                v[k] = LgB64Val(in[i + k]);
                if (v[k] < 0) return -1;
            }
        }

        out[o++] = (unsigned char)((v[0] << 2) | (v[1] >> 4));
        if (v[2] != -1) {
            out[o++] = (unsigned char)((v[1] << 4) | (v[2] >> 2));
        }
        if (v[3] != -1) {
            out[o++] = (unsigned char)((v[2] << 6) | v[3]);
        }
    }
    return (int)o;
}
