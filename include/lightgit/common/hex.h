/*
 * hex.h — 十六进制编解码
 *
 * Git 对象 ID 一律用 40 字符小写十六进制表示，此模块提供二进制与十六进制
 * 字符串之间的转换，供 git_hash / git_object 等模块复用。
 */
#ifndef LIGHTGIT_COMMON_HEX_H
#define LIGHTGIT_COMMON_HEX_H

#include <stddef.h>

/*
 * 将二进制数据编码为小写十六进制字符串。
 * out 至少需要 2*len + 1 字节（含结尾 NUL）。
 */
void lg_hex_encode(const unsigned char *bin, size_t len, char *out);

/*
 * 将十六进制字符串解码为二进制。
 * hex 必须恰好包含 2*out_len 个十六进制字符（大小写均可）。
 * 返回：成功返回解码出的字节数（即 out_len）；非法字符或长度不匹配返回 -1。
 */
int lg_hex_decode(const char *hex, unsigned char *out, size_t out_len);

#endif /* LIGHTGIT_COMMON_HEX_H */
