/*
 * github_tree.c — 远程 Git Tree 读取实现
 */
#include "lightgit/github/github_tree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int lg_github_get_tree_recursive(const char *token, const char *owner, const char *repo,
                                 const char *branch,
                                 lg_github_tree *out, lg_github_error *err)
{
    char path[512];
    lg_json_value *root = NULL;
    lg_json_value *tree = NULL;
    size_t i;

    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));

    snprintf(path, sizeof(path), "/repos/%s/%s/git/trees/%s?recursive=1",
             owner, repo, branch);

    if (lg_github_get_json(path, token, &root, err) != 0) {
        return -1;
    }

    {
        int truncated = 0;
        if (lg_json_bool(lg_json_get(root, "truncated"), &truncated) == 0) {
            out->truncated = truncated;
        }
    }

    tree = lg_json_get(root, "tree");
    if (tree == NULL || tree->type != LG_JSON_ARRAY) {
        lg_json_free(root);
        if (err != NULL) {
            err->code = LG_GITHUB_ERR_OTHER;
            snprintf(err->message, sizeof(err->message), "Unexpected tree response.");
        }
        return -1;
    }

    out->count = tree->count;
    out->entries = (lg_github_tree_entry *)calloc(out->count ? out->count : 1,
                                                  sizeof(*out->entries));
    if (out->entries == NULL && out->count > 0) {
        lg_json_free(root);
        return -1;
    }

    for (i = 0; i < tree->count; i++) {
        lg_json_value *item = lg_json_at(tree, i);
        const char *p = lg_json_get_string(item, "path");
        const char *m = lg_json_get_string(item, "mode");
        const char *t = lg_json_get_string(item, "type");
        const char *s = lg_json_get_string(item, "sha");
        lg_github_tree_entry *e = &out->entries[i];

        if (p) e->path = _strdup(p);
        if (m) e->mode = _strdup(m);
        if (t) e->type = _strdup(t);
        if (s) e->sha = _strdup(s);

        if ((p && e->path == NULL) || (s && e->sha == NULL)) {
            lg_github_tree_free(out);
            lg_json_free(root);
            return -1;
        }
    }

    lg_json_free(root);
    return 0;
}

const lg_github_tree_entry *lg_github_tree_find(const lg_github_tree *t, const char *path)
{
    size_t i;
    if (t == NULL || path == NULL) {
        return NULL;
    }
    for (i = 0; i < t->count; i++) {
        if (t->entries[i].path != NULL && strcmp(t->entries[i].path, path) == 0) {
            return &t->entries[i];
        }
    }
    return NULL;
}

void lg_github_tree_free(lg_github_tree *t)
{
    size_t i;
    if (t == NULL) {
        return;
    }
    for (i = 0; i < t->count; i++) {
        free(t->entries[i].path);
        free(t->entries[i].mode);
        free(t->entries[i].type);
        free(t->entries[i].sha);
    }
    free(t->entries);
    memset(t, 0, sizeof(*t));
}
