/*
 * base64.h — Base64 编解码
 *
 * 用于把 DPAPI 加密后的二进制 blob 编码成文本存入 JSON。
 */
#ifndef LIGHTGIT_COMMON_BASE64_H
#define LIGHTGIT_COMMON_BASE64_H

#include <stddef.h>

/*
 * 编码：out 至少需 ((in_len+2)/3)*4 + 1 字节。
 * 返回编码后长度（不含结尾 NUL），失败返回 -1。
 */
int lg_base64_encode(const unsigned char *in, size_t in_len, char *out, size_t out_cap);

/*
 * 解码：in_len 应为 4 的倍数（标准 base64，含 '=' 填充）。
 * 返回解码后字节数，失败返回 -1。
 */
int lg_base64_decode(const char *in, size_t in_len, unsigned char *out, size_t out_cap);

#endif /* LIGHTGIT_COMMON_BASE64_H */
