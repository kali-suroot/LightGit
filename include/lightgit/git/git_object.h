/*
 * git_object.h — Git 对象通用定义
 *
 * Git 有四种对象：blob / tree / commit / tag。LightGit 第一版只需要
 * blob / tree / commit（tag 保留枚举位但不实现）。
 */
#ifndef LIGHTGIT_GIT_GIT_OBJECT_H
#define LIGHTGIT_GIT_GIT_OBJECT_H

#include <stddef.h>
#include "lightgit/git/git_hash.h"

/* Git 对象类型（数值与 Git packfile 的对象类型码保持一致：1=commit 2=tree 3=blob 4=tag） */
typedef enum lg_object_type {
    LG_OBJECT_COMMIT = 1,
    LG_OBJECT_TREE   = 2,
    LG_OBJECT_BLOB   = 3,
    LG_OBJECT_TAG    = 4
} lg_object_type;

/*
 * 返回对象类型的小写名称："blob" / "tree" / "commit" / "tag"。
 */
const char *lg_object_type_name(lg_object_type type);

/*
 * 构建对象头 "<type> <len>\0"。
 * 返回写入的总字节数（含结尾 NUL）；out_cap 不足时返回所需字节数但不保证写入完整。
 */
size_t lg_object_header_build(lg_object_type type, size_t content_len, char *out, size_t out_cap);

/*
 * 计算对象 ID：SHA1("<type> <len>\0" + content)，输出 40 字符小写十六进制。
 * 成功返回 0，失败返回 -1。
 */
int lg_object_id_compute(lg_object_type type, const void *content, size_t content_len,
                         char out_hex[LG_GIT_HASH_HEX_SIZE + 1]);

#endif /* LIGHTGIT_GIT_GIT_OBJECT_H */
