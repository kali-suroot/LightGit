/*
 * git_pktline.h — Git pkt-line 数据格式
 *
 * pkt-line 是 Git Smart HTTP 协议的基本帧格式：
 *   每个数据包 = 4 字节十六进制长度前缀（含前缀自身）+ 负载
 *   特殊包：0000 = flush-pkt（段结束），0001 = delim-pkt，0002 = response-end-pkt
 */
#ifndef LIGHTGIT_GIT_GIT_PKTLINE_H
#define LIGHTGIT_GIT_GIT_PKTLINE_H

#include <stddef.h>

/* 单条 pkt-line 的长度上限（含 4 字节前缀）：0xffff */
#define LG_PKTLINE_MAX_TOTAL  0xffffu
#define LG_PKTLINE_MAX_PAYLOAD (LG_PKTLINE_MAX_TOTAL - 4)

/*
 * 编码一条数据包：写入 "4 字节十六进制长度 + 负载"。
 * 返回写入的字节数；out_cap 不足时返回 0。
 * 注意：负载长度不得超过 LG_PKTLINE_MAX_PAYLOAD。
 */
size_t lg_pktline_encode(unsigned char *out, size_t out_cap,
                         const void *payload, size_t payload_len);

/*
 * 写入 flush-pkt（"0000"）。返回 4。
 */
size_t lg_pktline_encode_flush(unsigned char out[4]);

/*
 * 解码 buffer 中 offset 处的一条 pkt-line。
 * 返回消耗的总字节数（含 4 字节前缀）；flush-pkt（0000）返回 4 且 *out_payload_len=0；
 * delim-pkt（0001）/ response-end（0002）同样返回 4 且无负载；
 * 数据截断或长度字段非法返回 -1。
 */
int lg_pktline_decode(const unsigned char *buf, size_t len, size_t offset,
                      const unsigned char **out_payload, size_t *out_payload_len);

#endif /* LIGHTGIT_GIT_GIT_PKTLINE_H */
