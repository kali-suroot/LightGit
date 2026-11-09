/*
 * git_hash.h — Git 对象哈希（SHA-1）
 *
 * Git 对象 ID = SHA1("<type> <len>\0<content>")。
 * 本模块封装 Windows CNG/BCrypt 提供的 SHA-1，提供流式（init/update/final）
 * 与一次性（lg_sha1_hex）两种接口。流式接口用于大文件，避免整文件载入内存。
 *
 * 说明：第一版仅支持 SHA-1 格式仓库；接口设计为后续 SHA-256 仓库预留扩展空间。
 */
#ifndef LIGHTGIT_GIT_GIT_HASH_H
#define LIGHTGIT_GIT_GIT_HASH_H

#include <stddef.h>

/* SHA-1 二进制 / 十六进制长度 */
#define LG_GIT_HASH_RAW_SIZE 20   /* 二进制字节数 */
#define LG_GIT_HASH_HEX_SIZE 40   /* 十六进制字符数（不含结尾 NUL） */

/* 不透明上下文，由 lg_sha1_init 分配 */
typedef struct lg_sha1_ctx lg_sha1_ctx;

/*
 * 初始化 SHA-1 上下文。
 * 成功返回 0 并将 *out_ctx 指向新上下文；失败返回 -1。
 */
int lg_sha1_init(lg_sha1_ctx **out_ctx);

/*
 * 向上下文追加数据。
 * data 为 NULL 且 len 为 0 视为空操作（成功）；其余非法参数返回 -1。
 */
int lg_sha1_update(lg_sha1_ctx *ctx, const void *data, size_t len);

/*
 * 完成计算，输出 20 字节二进制摘要，并释放上下文。
 * 无论成功失败，上下文都会被释放。
 */
int lg_sha1_final(lg_sha1_ctx *ctx, unsigned char out_raw[LG_GIT_HASH_RAW_SIZE]);

/*
 * 中途放弃：释放上下文但不输出摘要（用于出错清理）。
 */
void lg_sha1_abort(lg_sha1_ctx *ctx);

/*
 * 一次性计算 SHA-1，输出 40 字符小写十六进制字符串（含结尾 NUL，共 41 字节）。
 */
int lg_sha1_hex(const void *data, size_t len, char out_hex[LG_GIT_HASH_HEX_SIZE + 1]);

#endif /* LIGHTGIT_GIT_GIT_HASH_H */
