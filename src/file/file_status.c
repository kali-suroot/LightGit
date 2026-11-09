/*
 * file_status.c — 文件变化状态判定实现
 */
#include "lightgit/file/file_status.h"

#include <string.h>

lg_file_status lg_file_status_compute(const char *old_sha, const char *new_sha)
{
    if (new_sha == NULL) {
        return LG_FILE_DELETED;   /* 本地不存在 */
    }
    if (old_sha == NULL) {
        return LG_FILE_ADDED;     /* Manifest 无记录 */
    }
    if (strcmp(old_sha, new_sha) == 0) {
        return LG_FILE_UNCHANGED;
    }
    return LG_FILE_MODIFIED;
}
