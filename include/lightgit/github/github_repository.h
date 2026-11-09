/*
 * github_repository.h — GitHub 仓库管理
 *
 * 获取当前账号的仓库列表（分页）、读取仓库信息（默认分支等）、创建仓库。
 */
#ifndef LIGHTGIT_GITHUB_GITHUB_REPOSITORY_H
#define LIGHTGIT_GITHUB_GITHUB_REPOSITORY_H

#include <stddef.h>
#include "lightgit/github/github_api.h"

/* 仓库信息 */
typedef struct lg_github_repo {
    char *name;             /* 仓库名 */
    char *full_name;        /* "owner/name" */
    char *description;      /* 描述（可空） */
    char *default_branch;   /* 默认分支 */
    int   is_private;       /* 1 = 私有 */
} lg_github_repo;

/*
 * 获取当前账号所有仓库（自动分页，per_page=100）。
 * 成功返回 0 并写入 *out_list / *out_count（调用方 lg_github_repo_list_free）。
 */
int lg_github_list_repos(const char *token,
                         lg_github_repo **out_list, size_t *out_count,
                         lg_github_error *err);

void lg_github_repo_list_free(lg_github_repo *list, size_t count);

/*
 * 读取单个仓库信息（含默认分支）。owner/repo 为 UTF-8。
 */
int lg_github_get_repo(const char *token, const char *owner, const char *repo,
                       lg_github_repo *out, lg_github_error *err);

void lg_github_repo_free(lg_github_repo *r);

/*
 * 创建仓库。init_readme 为 1 时初始化 README（会产生一个初始提交）。
 */
int lg_github_create_repo(const char *token, const char *name, const char *description,
                          int is_private, int init_readme,
                          lg_github_repo *out, lg_github_error *err);

#endif /* LIGHTGIT_GITHUB_GITHUB_REPOSITORY_H */
