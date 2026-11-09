/*
 * lg_github.c — GitHub API 命令行验证工具（开发/集成测试用）
 *
 * 验证 Phase 5 的 REST API 封装：
 *   1) GET /user 验证 PAT 并打印用户名/ID
 *   2) GET /user/repos 分页列出仓库
 *   3) GET /repos/{owner}/{repo} 打印默认分支
 *
 * 用法：lg_github [owner repo]
 * 环境变量：LG_TOKEN=GitHub PAT
 */
#include "lightgit/github/github_auth.h"
#include "lightgit/github/github_repository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char token[512];
    size_t need = 0;
    lg_github_error err;
    lg_github_user user;
    lg_github_repo *list = NULL;
    size_t count = 0, i;

    if (getenv_s(&need, token, sizeof(token), "LG_TOKEN") != 0 || need == 0) {
        fprintf(stderr, "缺少 LG_TOKEN 环境变量\n");
        return 2;
    }

    printf("[1] GET /user 验证 Token...\n");
    if (lg_github_get_current_user(token, &user, &err) != 0) {
        fprintf(stderr, "验证失败 [%d]: %s\n", (int)err.code, err.message);
        return 1;
    }
    printf("  登录名: %s\n  ID: %lld\n", user.login, user.id);
    lg_github_user_free(&user);

    printf("[2] GET /user/repos 列出仓库...\n");
    if (lg_github_list_repos(token, &list, &count, &err) != 0) {
        fprintf(stderr, "获取仓库列表失败 [%d]: %s\n", (int)err.code, err.message);
        return 1;
    }
    printf("  共 %zu 个仓库\n", count);
    for (i = 0; i < count && i < 10; i++) {
        printf("    %s  (默认分支 %s, %s)\n",
               list[i].full_name ? list[i].full_name : list[i].name,
               list[i].default_branch ? list[i].default_branch : "(空)",
               list[i].is_private ? "private" : "public");
    }
    lg_github_repo_list_free(list, count);

    if (argc >= 3) {
        lg_github_repo repo;
        printf("[3] GET /repos/%s/%s ...\n", argv[1], argv[2]);
        if (lg_github_get_repo(token, argv[1], argv[2], &repo, &err) != 0) {
            fprintf(stderr, "获取仓库失败 [%d]: %s\n", (int)err.code, err.message);
            return 1;
        }
        printf("  name=%s\n  default_branch=%s\n  private=%d\n",
               repo.name, repo.default_branch ? repo.default_branch : "(空)", repo.is_private);
        lg_github_repo_free(&repo);
    }

    return 0;
}
