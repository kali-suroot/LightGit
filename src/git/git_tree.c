/*
 * git_tree.c — Git Tree 对象实现
 */
#include "lightgit/git/git_tree.h"
#include "lightgit/git/git_object.h"
#include "lightgit/common/hex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * 排序：按名称字节序比较，目录名视为追加 '/' 后比较
 * -------------------------------------------------------------------------- */
static int LgTreeEntryCompare(const void *a, const void *b)
{
    const lg_tree_entry *ea = (const lg_tree_entry *)a;
    const lg_tree_entry *eb = (const lg_tree_entry *)b;
    int a_is_dir = (ea->mode == LG_TREE_MODE_DIR);
    int b_is_dir = (eb->mode == LG_TREE_MODE_DIR);
    size_t la = strlen(ea->name);
    size_t lb = strlen(eb->name);
    size_t i;

    for (i = 0; ; i++) {
        /* 取出双方“排序名”第 i 字节：目录名后追加 '/'，越界返回 0 */
        unsigned char ca = (i < la) ? (unsigned char)ea->name[i]
                                    : ((i == la && a_is_dir) ? '/' : 0);
        unsigned char cb = (i < lb) ? (unsigned char)eb->name[i]
                                    : ((i == lb && b_is_dir) ? '/' : 0);
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        if (ca == 0) {
            break;   /* 双方排序名完全一致 */
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * 尺寸计算与序列化
 * -------------------------------------------------------------------------- */
size_t lg_tree_content_size(const lg_tree_entry *entries, size_t count)
{
    size_t total = 0;
    size_t i;

    if (entries == NULL) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        char modebuf[16];
        int n = snprintf(modebuf, sizeof(modebuf), "%o", entries[i].mode);
        /* "<mode> " + "<name>\0" + 20 字节 SHA-1 */
        total += (size_t)n + 1 + strlen(entries[i].name) + 1 + LG_GIT_HASH_RAW_SIZE;
    }
    return total;
}

int lg_tree_build(lg_tree_entry *entries, size_t count,
                  unsigned char *out, size_t out_cap, size_t *out_len)
{
    unsigned char *p;
    size_t need, i;

    if (out_len != NULL) {
        *out_len = 0;
    }

    /* 按 Git 规则就地排序 */
    if (count > 1 && entries != NULL) {
        qsort(entries, count, sizeof(entries[0]), LgTreeEntryCompare);
    }

    need = lg_tree_content_size(entries, count);

    if (out == NULL || out_cap < need) {
        if (out_len != NULL) {
            *out_len = need;
        }
        return -1;
    }

    p = out;
    for (i = 0; i < count; i++) {
        char modebuf[16];
        int n = snprintf(modebuf, sizeof(modebuf), "%o", entries[i].mode);
        size_t namelen = strlen(entries[i].name);

        /* 写入 "<mode> " */
        memcpy(p, modebuf, (size_t)n);
        p += n;
        *p++ = ' ';

        /* 写入名称与结尾 NUL */
        memcpy(p, entries[i].name, namelen);
        p += namelen;
        *p++ = '\0';

        /* 写入 20 字节二进制对象 ID */
        memcpy(p, entries[i].sha1, LG_GIT_HASH_RAW_SIZE);
        p += LG_GIT_HASH_RAW_SIZE;
    }

    if (out_len != NULL) {
        *out_len = need;
    }
    return 0;
}

int lg_tree_hash(lg_tree_entry *entries, size_t count, char out_hex[LG_GIT_HASH_HEX_SIZE + 1])
{
    size_t need;
    size_t len = 0;
    unsigned char *buf = NULL;
    int result;

    if (out_hex == NULL) {
        return -1;
    }

    need = lg_tree_content_size(entries, count);

    if (need > 0) {
        buf = (unsigned char *)malloc(need);
        if (buf == NULL) {
            return -1;
        }
    }

    if (lg_tree_build(entries, count, buf, need, &len) != 0) {
        free(buf);
        return -1;
    }

    result = lg_object_id_compute(LG_OBJECT_TREE, buf, len, out_hex);
    free(buf);
    return result;
}
