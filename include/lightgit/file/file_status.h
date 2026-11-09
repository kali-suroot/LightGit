/*
 * file_status.h — 文件变化状态判定
 *
 * 依据 Manifest 中的旧 blob 哈希与本地新 blob 哈希判断文件状态。
 */
#ifndef LIGHTGIT_FILE_FILE_STATUS_H
#define LIGHTGIT_FILE_FILE_STATUS_H

typedef enum lg_file_status {
    LG_FILE_UNCHANGED = 0,   /* 哈希一致 */
    LG_FILE_ADDED,           /* 本地新增 */
    LG_FILE_MODIFIED,        /* 哈希不同 */
    LG_FILE_DELETED          /* 本地已删除（Manifest 有、本地无） */
} lg_file_status;

/*
 * 计算文件状态。
 * old_sha 为 Manifest 中的旧 blob ID（本地无该记录时为 NULL）；
 * new_sha 为本地计算的 blob ID（本地文件不存在时为 NULL）。
 */
lg_file_status lg_file_status_compute(const char *old_sha, const char *new_sha);

#endif /* LIGHTGIT_FILE_FILE_STATUS_H */
