/*
 * github_repository.c — GitHub 仓库管理实现
 */
#include "lightgit/github/github_repository.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 从 JSON 对象解析仓库信息（字段缺失时用空值/默认值兜底） */
static int LgParseRepo(const lg_json_value *obj, lg_github_repo *out)
{
    const char *name, *full_name, *desc, *branch;
    int is_private = 0;

    memset(out, 0, sizeof(*out));

    name = lg_json_get_string(obj, "name");
    full_name = lg_json_get_string(obj, "full_name");
    desc = lg_json_get_string(obj, "description");
    branch = lg_json_get_string(obj, "default_branch");
    lg_json_bool(lg_json_get(obj, "private"), &is_private);

    if (name == NULL) {
        return -1;
    }

    out->name = _strdup(name);
    out->full_name = full_name ? _strdup(full_name) : NULL;
    out->description = (desc && desc[0]) ? _strdup(desc) : NULL;
    out->default_branch = branch ? _strdup(branch) : NULL;
    out->is_private = is_private;

    if (out->name == NULL) {
        lg_github_repo_free(out);
        return -1;
    }
    return 0;
}

void lg_github_repo_free(lg_github_repo *r)
{
    if (r == NULL) {
        return;
    }
    free(r->name);
    free(r->full_name);
    free(r->description);
    free(r->default_branch);
    memset(r, 0, sizeof(*r));
}

void lg_github_repo_list_free(lg_github_repo *list, size_t count)
{
    size_t i;
    if (list == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        lg_github_repo_free(&list[i]);
    }
    free(list);
}

int lg_github_list_repos(const char *token,
                         lg_github_repo **out_list, size_t *out_count,
                         lg_github_error *err)
{
    lg_github_repo *list = NULL;
    size_t count = 0, cap = 0;
    int page;

    if (out_list == NULL || out_count == NULL) {
        return -1;
    }
    *out_list = NULL;
    *out_count = 0;

    for (page = 1; ; page++) {
        char path[128];
        lg_json_value *root = NULL;
        size_t i, n;

        snprintf(path, sizeof(path), "/user/repos?per_page=100&page=%d", page);
        if (lg_github_get_json(path, token, &root, err) != 0) {
            lg_github_repo_list_free(list, count);
            return -1;
        }
        if (root == NULL || root->type != LG_JSON_ARRAY) {
            lg_json_free(root);
            lg_github_repo_list_free(list, count);
            if (err != NULL) {
                err->code = LG_GITHUB_ERR_OTHER;
                snprintf(err->message, sizeof(err->message), "Unexpected repo list response.");
            }
            return -1;
        }

        n = root->count;
        if (n == 0) {
            lg_json_free(root);
            break;   /* 已取完所有页 */
        }

        for (i = 0; i < n; i++) {
            lg_github_repo item;
            if (LgParseRepo(lg_json_at(root, i), &item) != 0) {
                continue;   /* 跳过无法解析的条目 */
            }
            if (count == cap) {
                size_t ncap = cap ? cap * 2 : 16;
                lg_github_repo *nl = (lg_github_repo *)realloc(list, ncap * sizeof(*nl));
                if (nl == NULL) {
                    lg_github_repo_free(&item);
                    lg_json_free(root);
                    lg_github_repo_list_free(list, count);
                    return -1;
                }
                list = nl;
                cap = ncap;
            }
            list[count++] = item;
        }
        lg_json_free(root);

        if (n < 100) {
            break;   /* 不足一页，说明是最后一页 */
        }
    }

    *out_list = list;
    *out_count = count;
    return 0;
}

int lg_github_get_repo(const char *token, const char *owner, const char *repo,
                       lg_github_repo *out, lg_github_error *err)
{
    char path[256];
    lg_json_value *root = NULL;

    if (owner == NULL || repo == NULL || out == NULL) {
        return -1;
    }
    snprintf(path, sizeof(path), "/repos/%s/%s", owner, repo);

    if (lg_github_get_json(path, token, &root, err) != 0) {
        return -1;
    }
    if (LgParseRepo(root, out) != 0) {
        lg_json_free(root);
        return -1;
    }
    lg_json_free(root);
    return 0;
}

int lg_github_create_repo(const char *token, const char *name, const char *description,
                          int is_private, int init_readme,
                          lg_github_repo *out, lg_github_error *err)
{
    char body[1024];
    lg_json_value *root = NULL;
    int n;

    if (name == NULL) {
        return -1;
    }

    /* 组装 JSON 请求体（name 只含合法字符；description 做基本转义） */
    {
        char desc[512];
        size_t i, j = 0;
        for (i = 0; description != NULL && description[i] != '\0' && j < sizeof(desc) - 1; i++) {
            char c = description[i];
            if (c == '\\' || c == '"') {
                desc[j++] = '\\';
            }
            desc[j++] = c;
        }
        desc[j] = '\0';
        n = snprintf(body, sizeof(body),
                     "{\"name\":\"%s\",\"description\":\"%s\",\"private\":%s,\"auto_init\":%s}",
                     name, desc, is_private ? "true" : "false", init_readme ? "true" : "false");
    }
    if (n < 0 || (size_t)n >= sizeof(body)) {
        return -1;
    }

    if (lg_github_post_json("/user/repos", token, body, &root, err) != 0) {
        return -1;
    }

    if (out != NULL) {
        if (LgParseRepo(root, out) != 0) {
            lg_json_free(root);
            return -1;
        }
    }
    lg_json_free(root);
    return 0;
}
