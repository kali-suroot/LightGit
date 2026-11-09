/*
 * git_tree.h — Git Tree 对象
 *
 * Tree 表示一个目录：由若干“条目”组成，每个条目指向一个 blob（文件）
 * 或子树（子目录）。条目必须按 Git 规则排序后序列化：
 *
 *   每个条目 = "<八进制 mode> <UTF-8 名称>\0<20 字节二进制 SHA-1>"
 *
 * 排序规则：按名称字节序比较，目录名视为在其后追加 '/' 参与比较。
 */
#ifndef LIGHTGIT_GIT_GIT_TREE_H
#define LIGHTGIT_GIT_GIT_TREE_H

#include <stddef.h>
#include "lightgit/git/git_hash.h"

/* 文件模式（Git 以八进制表示，这里用 C 八进制字面量定义） */
#define LG_TREE_MODE_FILE    0100644   /* 普通文件 */
#define LG_TREE_MODE_EXEC    0100755   /* 可执行文件 */
#define LG_TREE_MODE_DIR     0040000   /* 目录（子树） */
#define LG_TREE_MODE_SYMLINK 0120000   /* 符号链接 */
#define LG_TREE_MODE_GITLINK 0160000   /* submodule */

/* 一个 Tree 条目 */
typedef struct lg_tree_entry {
    unsigned int        mode;   /* LG_TREE_MODE_* 之一 */
    const char         *name;   /* UTF-8 文件名（相对当前 tree，不含 '/'） */
    const unsigned char *sha1;  /* 指向 20 字节二进制对象 ID */
} lg_tree_entry;

/*
 * 计算序列化后 tree 内容的总字节数（与顺序无关）。
 */
size_t lg_tree_content_size(const lg_tree_entry *entries, size_t count);

/*
 * 按 Git 规则排序 entries（就地排序，会重排调用方数组），
 * 并序列化为二进制 tree 内容写入 out。
 * 成功返回 0；out 容量不足时返回 -1 并通过 out_len 告知所需字节数。
 */
int lg_tree_build(lg_tree_entry *entries, size_t count,
                  unsigned char *out, size_t out_cap, size_t *out_len);

/*
 * 排序 + 序列化 + 计算 tree 对象 ID（40 字符小写十六进制）。
 * 注意：会就地排序 entries 数组。
 */
int lg_tree_hash(lg_tree_entry *entries, size_t count, char out_hex[LG_GIT_HASH_HEX_SIZE + 1]);

#endif /* LIGHTGIT_GIT_GIT_TREE_H */
