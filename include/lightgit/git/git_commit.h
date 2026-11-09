/*
 * git_commit.h — Git Commit 对象
 *
 * Commit 对象格式（第一版只支持单父提交 / root 提交）：
 *
 *   tree <tree>\n
 *   [parent <parent>\n]
 *   author <name> <email> <timestamp> <tz>\n
 *   committer <name> <email> <timestamp> <tz>\n
 *   \n
 *   <message>\n
 */
#ifndef LIGHTGIT_GIT_GIT_COMMIT_H
#define LIGHTGIT_GIT_GIT_COMMIT_H

#include <stddef.h>
#include "lightgit/git/git_hash.h"

/* 提交作者/提交者信息 */
typedef struct lg_commit_author {
    const char *name;        /* 显示名（UTF-8） */
    const char *email;       /* 邮箱（UTF-8，不含尖括号，构建时自动包裹为 <email>） */
    long long   timestamp;   /* Unix 时间戳（秒） */
    const char *tz_offset;   /* 时区偏移字符串，如 "+0800" */
} lg_commit_author;

/*
 * 构建完整 commit 对象内容（不含类型头，即 "tree ..." 起的内容）。
 * parent_hex 传 NULL 表示 root commit；否则为 40 字符父提交 ID。
 * 成功返回 0，并通过 *out_content / *out_len 返回内容（调用方负责 free）。
 * 返回的内容以 NUL 结尾，但 *out_len 为不含 NUL 的真实字节数。
 */
int lg_commit_build(const char *tree_hex,
                    const char *parent_hex,
                    const lg_commit_author *author,
                    const lg_commit_author *committer,
                    const char *message,
                    char **out_content,
                    size_t *out_len);

/*
 * 构建并计算 commit 对象 ID（40 字符小写十六进制）。
 */
int lg_commit_hash(const char *tree_hex,
                   const char *parent_hex,
                   const lg_commit_author *author,
                   const lg_commit_author *committer,
                   const char *message,
                   char out_hex[LG_GIT_HASH_HEX_SIZE + 1]);

#endif /* LIGHTGIT_GIT_GIT_COMMIT_H */
