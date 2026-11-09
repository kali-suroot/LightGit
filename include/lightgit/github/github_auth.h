/*
 * github_auth.h — GitHub 账号认证（PAT）
 *
 * 通过 GET /user 验证 Personal Access Token 并获取当前用户信息。
 */
#ifndef LIGHTGIT_GITHUB_GITHUB_AUTH_H
#define LIGHTGIT_GITHUB_GITHUB_AUTH_H

#include "lightgit/github/github_api.h"

/* 当前用户信息 */
typedef struct lg_github_user {
    char      *login;   /* 用户名 */
    long long  id;      /* 用户 ID */
} lg_github_user;

/*
 * 验证 Token 并获取当前用户。
 * 成功返回 0（填充 out_user）；失败返回 -1 并通过 err 描述原因
 * （含 Token 无效 / 网络断开 / 限流等分类）。
 */
int lg_github_get_current_user(const char *token, lg_github_user *out_user, lg_github_error *err);

void lg_github_user_free(lg_github_user *u);

#endif /* LIGHTGIT_GITHUB_GITHUB_AUTH_H */
