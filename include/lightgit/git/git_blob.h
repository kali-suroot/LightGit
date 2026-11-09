/*
 * git_blob.h — Git Blob 对象
 *
 * Blob 即文件内容本身；Blob ID = SHA1("blob <len>\0" + 内容)。
 * 提供内存与文件两种输入方式。
 */
#ifndef LIGHTGIT_GIT_GIT_BLOB_H
#define LIGHTGIT_GIT_GIT_BLOB_H

#include <stddef.h>
#include "lightgit/git/git_hash.h"

/*
 * 计算内存中数据的 Blob ID（40 字符小写十六进制，含结尾 NUL）。
 */
int lg_blob_hash_memory(const void *data, size_t len, char out_hex[LG_GIT_HASH_HEX_SIZE + 1]);

/*
 * 计算文件的 Blob ID。path 为 UTF-16 路径（Windows 宽字符）。
 * 采用流式读取，支持大文件。成功返回 0，失败返回 -1。
 */
int lg_blob_hash_file(const wchar_t *path, char out_hex[LG_GIT_HASH_HEX_SIZE + 1]);

#endif /* LIGHTGIT_GIT_GIT_BLOB_H */
