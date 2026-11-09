/*
 * github_auth.c — GitHub 账号认证实现
 */
#include "lightgit/github/github_auth.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int lg_github_get_current_user(const char *token, lg_github_user *out_user, lg_github_error *err)
{
    lg_json_value *root = NULL;
    const char *login;
    long long id = 0;

    if (out_user == NULL) {
        return -1;
    }
    memset(out_user, 0, sizeof(*out_user));

    if (lg_github_get_json("/user", token, &root, err) != 0) {
        return -1;
    }

    login = lg_json_get_string(root, "login");
    if (login == NULL) {
        if (err != NULL) {
            err->code = LG_GITHUB_ERR_OTHER;
            snprintf(err->message, sizeof(err->message), "Unexpected /user response.");
        }
        lg_json_free(root);
        return -1;
    }

    lg_json_int64(lg_json_get(root, "id"), &id);

    out_user->login = _strdup(login);
    out_user->id = id;
    if (out_user->login == NULL) {
        lg_json_free(root);
        return -1;
    }

    lg_json_free(root);
    return 0;
}

void lg_github_user_free(lg_github_user *u)
{
    if (u == NULL) {
        return;
    }
    free(u->login);
    u->login = NULL;
    u->id = 0;
}
