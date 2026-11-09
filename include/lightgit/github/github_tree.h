/*
 * github_tree.h — 读取远程 Git Tree（用于发布合并）
 *
 * 通过 REST API GET /repos/{owner}/{repo}/git/trees/{branch}?recursive=1
 * 获取远程分支整棵树（扁平列表），作为本地发布合并的基线。
 * 这样无需实现 fetch/upload-pack 协议即可拿到远端文件清单与对象 ID。
 */
#ifndef LIGHTGIT_GITHUB_GITHUB_TREE_H
#define LIGHTGIT_GITHUB_GITHUB_TREE_H

#include <stddef.h>
#include "lightgit/github/github_api.h"

/* 远程树中的一条记录 */
typedef struct lg_github_tree_entry {
    char *path;   /* 相对路径（'/' 分隔） */
    char *mode;   /* "100644" / "100755" / "040000" / "120000" / "160000" */
    char *type;   /* "blob" / "tree" / "commit" */
    char *sha;    /* 40 字符对象 ID */
} lg_github_tree_entry;

typedef struct lg_github_tree {
    lg_github_tree_entry *entries;
    size_t                count;
    int                   truncated;   /* 1 = 树过大被截断（暂不支持） */
} lg_github_tree;

/*
 * 递归读取远程分支树。成功返回 0 并填充 out（调用方 lg_github_tree_free）。
 * 注意：空仓库（无提交）返回 0 且 count==0。
 */
int lg_github_get_tree_recursive(const char *token, const char *owner, const char *repo,
                                 const char *branch,
                                 lg_github_tree *out, lg_github_error *err);

/* 在树中按路径查找（返回 NULL 表示不存在） */
const lg_github_tree_entry *lg_github_tree_find(const lg_github_tree *t, const char *path);

void lg_github_tree_free(lg_github_tree *t);

#endif /* LIGHTGIT_GITHUB_GITHUB_TREE_H */
