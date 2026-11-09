/*
 * manifest.c — 发布 Manifest 实现
 */
#include "lightgit/storage/manifest.h"
#include "lightgit/common/buffer.h"
#include "lightgit/git/git_hash.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int lg_manifest_parse(const char *text, lg_manifest *m)
{
    const char *line = text;

    if (m == NULL) {
        return -1;
    }

    while (line != NULL && *line) {
        const char *eol = strchr(line, '\n');
        size_t linelen = eol ? (size_t)(eol - line) : strlen(line);

        /* 去掉行尾 \r 与空白 */
        while (linelen > 0 && (line[linelen - 1] == '\r' || line[linelen - 1] == ' ')) {
            linelen--;
        }

        /* 格式："<40hex> <path>"，空格位置固定在第 41 字节 */
        if (linelen >= 41 && line[40] == ' ') {
            char sha[LG_GIT_HASH_HEX_SIZE + 1];
            char *path;
            size_t pathlen = linelen - 41;

            memcpy(sha, line, LG_GIT_HASH_HEX_SIZE);
            sha[LG_GIT_HASH_HEX_SIZE] = '\0';

            path = (char *)malloc(pathlen + 1);
            if (path == NULL) {
                return -1;
            }
            memcpy(path, line + 41, pathlen);
            path[pathlen] = '\0';

            if (lg_manifest_set(m, path, sha) != 0) {
                free(path);
                return -1;
            }
            free(path);
        }

        line = eol ? eol + 1 : NULL;
    }
    return 0;
}

int lg_manifest_serialize(const lg_manifest *m, char **out_text)
{
    lg_buffer buf;
    size_t i;

    if (m == NULL || out_text == NULL) {
        return -1;
    }
    *out_text = NULL;
    lg_buffer_init(&buf);

    for (i = 0; i < m->count; i++) {
        char line[4096];
        int n = snprintf(line, sizeof(line), "%s %s\n",
                         m->entries[i].blob_sha, m->entries[i].path);
        if (n < 0 || (size_t)n >= sizeof(line)) {
            lg_buffer_free(&buf);
            return -1;
        }
        if (lg_buffer_append(&buf, line, (size_t)n) != 0) {
            lg_buffer_free(&buf);
            return -1;
        }
    }

    /* 结尾补 NUL，便于按字符串处理 */
    if (lg_buffer_append(&buf, "", 1) != 0) {
        lg_buffer_free(&buf);
        return -1;
    }
    *out_text = (char *)buf.data;
    return 0;
}

const char *lg_manifest_find(const lg_manifest *m, const char *path)
{
    size_t i;
    if (m == NULL || path == NULL) {
        return NULL;
    }
    for (i = 0; i < m->count; i++) {
        if (strcmp(m->entries[i].path, path) == 0) {
            return m->entries[i].blob_sha;
        }
    }
    return NULL;
}

int lg_manifest_set(lg_manifest *m, const char *path, const char *blob_sha)
{
    size_t i;

    if (m == NULL || path == NULL || blob_sha == NULL) {
        return -1;
    }

    /* 已存在则更新 */
    for (i = 0; i < m->count; i++) {
        if (strcmp(m->entries[i].path, path) == 0) {
            char *ns = _strdup(blob_sha);
            if (ns == NULL) {
                return -1;
            }
            free(m->entries[i].blob_sha);
            m->entries[i].blob_sha = ns;
            return 0;
        }
    }

    /* 新增 */
    {
        lg_manifest_entry *ne = (lg_manifest_entry *)realloc(m->entries,
                                                             (m->count + 1) * sizeof(*ne));
        char *p = _strdup(path);
        char *s = _strdup(blob_sha);
        if (ne == NULL || p == NULL || s == NULL) {
            free(p);
            free(s);
            return -1;
        }
        m->entries = ne;
        m->entries[m->count].path = p;
        m->entries[m->count].blob_sha = s;
        m->count++;
    }
    return 0;
}

void lg_manifest_clear(lg_manifest *m)
{
    size_t i;
    if (m == NULL) {
        return;
    }
    for (i = 0; i < m->count; i++) {
        free(m->entries[i].path);
        free(m->entries[i].blob_sha);
    }
    free(m->entries);
    m->entries = NULL;
    m->count = 0;
}
