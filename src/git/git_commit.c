/*
 * git_commit.c — Git Commit 对象实现
 *
 * 采用“两遍”构造：第一遍用 vsnprintf(NULL,0,...) 精确计算总长度，
 * 第二遍写入实际内容，从而避免手工累加长度出错导致的缓冲区溢出。
 */
#include "lightgit/git/git_commit.h"
#include "lightgit/git/git_object.h"
#include "lightgit/common/hex.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 追加一行：返回不含结尾 NUL 的字符数（dst 为 NULL 时仅计算长度） */
static int LgLineLen(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return n < 0 ? 0 : n;
}

int lg_commit_build(const char *tree_hex,
                    const char *parent_hex,
                    const lg_commit_author *author,
                    const lg_commit_author *committer,
                    const char *message,
                    char **out_content,
                    size_t *out_len)
{
    char tsbuf[32], ctsbuf[32];
    size_t total = 0;
    char *buf, *p;

    if (tree_hex == NULL || author == NULL || committer == NULL ||
        message == NULL || out_content == NULL) {
        return -1;
    }

    snprintf(tsbuf, sizeof(tsbuf), "%lld", author->timestamp);
    snprintf(ctsbuf, sizeof(ctsbuf), "%lld", committer->timestamp);

    /* ---- 第一遍：精确计算总长度 ---- */
    total += (size_t)LgLineLen(NULL, 0, "tree %s\n", tree_hex);
    if (parent_hex != NULL) {
        total += (size_t)LgLineLen(NULL, 0, "parent %s\n", parent_hex);
    }
    total += (size_t)LgLineLen(NULL, 0, "author %s <%s> %s %s\n",
                               author->name, author->email, tsbuf, author->tz_offset);
    total += (size_t)LgLineLen(NULL, 0, "committer %s <%s> %s %s\n",
                               committer->name, committer->email, ctsbuf, committer->tz_offset);
    total += (size_t)LgLineLen(NULL, 0, "\n%s\n", message);

    /* 多分配 1 字节用于结尾 NUL（便于调用方按字符串处理） */
    buf = (char *)malloc(total + 1);
    if (buf == NULL) {
        return -1;
    }

    /* ---- 第二遍：写入实际内容 ---- */
    p = buf;
    p += LgLineLen(p, total + 1 - (size_t)(p - buf), "tree %s\n", tree_hex);
    if (parent_hex != NULL) {
        p += LgLineLen(p, total + 1 - (size_t)(p - buf), "parent %s\n", parent_hex);
    }
    p += LgLineLen(p, total + 1 - (size_t)(p - buf), "author %s <%s> %s %s\n",
                   author->name, author->email, tsbuf, author->tz_offset);
    p += LgLineLen(p, total + 1 - (size_t)(p - buf), "committer %s <%s> %s %s\n",
                   committer->name, committer->email, ctsbuf, committer->tz_offset);
    p += LgLineLen(p, total + 1 - (size_t)(p - buf), "\n%s\n", message);

    buf[total] = '\0';

    *out_content = buf;
    if (out_len != NULL) {
        *out_len = total;
    }
    return 0;
}

int lg_commit_hash(const char *tree_hex,
                   const char *parent_hex,
                   const lg_commit_author *author,
                   const lg_commit_author *committer,
                   const char *message,
                   char out_hex[LG_GIT_HASH_HEX_SIZE + 1])
{
    char *content = NULL;
    size_t len = 0;
    int result;

    if (lg_commit_build(tree_hex, parent_hex, author, committer, message,
                        &content, &len) != 0) {
        return -1;
    }

    result = lg_object_id_compute(LG_OBJECT_COMMIT, content, len, out_hex);
    free(content);
    return result;
}
