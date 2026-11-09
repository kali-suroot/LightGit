/*
 * git_object.c — Git 对象通用逻辑实现
 */
#include "lightgit/git/git_object.h"
#include "lightgit/common/hex.h"

#include <stdio.h>

/* 对象类型名称表（与 lg_object_type 枚举顺序一致） */
static const char *g_object_type_names[] = {
    "",         /* 占位（无类型码 0） */
    "commit",   /* LG_OBJECT_COMMIT = 1 */
    "tree",     /* LG_OBJECT_TREE   = 2 */
    "blob",     /* LG_OBJECT_BLOB   = 3 */
    "tag"       /* LG_OBJECT_TAG    = 4 */
};

const char *lg_object_type_name(lg_object_type type)
{
    if (type < LG_OBJECT_COMMIT || type > LG_OBJECT_TAG) {
        return "blob";   /* 越界兜底 */
    }
    return g_object_type_names[(int)type];
}

size_t lg_object_header_build(lg_object_type type, size_t content_len, char *out, size_t out_cap)
{
    /* snprintf 返回“不含结尾 NUL 的字符数”；对象头要求以 NUL 结尾，
       因此总字节数 = 返回字符数 + 1。 */
    int n = snprintf(out, out_cap, "%s %llu",
                     lg_object_type_name(type),
                     (unsigned long long)content_len);
    return (n < 0) ? 0 : (size_t)n + 1;
}

int lg_object_id_compute(lg_object_type type, const void *content, size_t content_len,
                         char out_hex[LG_GIT_HASH_HEX_SIZE + 1])
{
    char header[64];
    size_t hlen;
    lg_sha1_ctx *ctx = NULL;
    unsigned char raw[LG_GIT_HASH_RAW_SIZE];

    if (out_hex == NULL) {
        return -1;
    }

    /* 对象头最大约 28 字节（"commit " + 20 位长度 + NUL），64 字节足够 */
    hlen = lg_object_header_build(type, content_len, header, sizeof(header));

    if (lg_sha1_init(&ctx) != 0) {
        return -1;
    }
    if (lg_sha1_update(ctx, header, hlen) != 0) {
        lg_sha1_abort(ctx);
        return -1;
    }
    if (lg_sha1_update(ctx, content, content_len) != 0) {
        lg_sha1_abort(ctx);
        return -1;
    }
    if (lg_sha1_final(ctx, raw) != 0) {
        return -1;   /* final 已释放上下文 */
    }

    lg_hex_encode(raw, LG_GIT_HASH_RAW_SIZE, out_hex);
    return 0;
}
