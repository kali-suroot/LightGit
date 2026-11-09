/*
 * manifest.h — 发布 Manifest（本地已发布状态）
 *
 * 记录“上次成功发布”时每个文件的 blob 对象 ID，用于变化检测：
 *   - 本地新文件且不在 manifest → Added
 *   - blob 哈希不同           → Modified
 *   - 哈希相同               → Unchanged
 *   - 在 manifest 但本地已删 → Deleted（不自动删远端）
 *
 * 存储格式（纯文本，UTF-8，每行）："<40hex-blob-id> <相对路径>\n"
 */
#ifndef LIGHTGIT_STORAGE_MANIFEST_H
#define LIGHTGIT_STORAGE_MANIFEST_H

#include <stddef.h>

typedef struct lg_manifest_entry {
    char *path;      /* 相对路径（UTF-8，'/' 分隔） */
    char *blob_sha;  /* 40 字符 blob 对象 ID */
} lg_manifest_entry;

typedef struct lg_manifest {
    lg_manifest_entry *entries;
    size_t             count;
} lg_manifest;

/* 从文本解析 manifest；m 需已清零。成功返回 0。 */
int lg_manifest_parse(const char *text, lg_manifest *m);

/* 序列化 manifest 为文本（malloc，调用方 free），成功返回 0 */
int lg_manifest_serialize(const lg_manifest *m, char **out_text);

/* 查找 path 对应的 blob ID，找不到返回 NULL */
const char *lg_manifest_find(const lg_manifest *m, const char *path);

/* 设置/更新 path 的 blob ID（path 会被拷贝） */
int lg_manifest_set(lg_manifest *m, const char *path, const char *blob_sha);

/* 清空并释放 */
void lg_manifest_clear(lg_manifest *m);

#endif /* LIGHTGIT_STORAGE_MANIFEST_H */
