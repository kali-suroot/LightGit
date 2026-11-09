/*
 * config.c — 应用配置实现（JSON 读写）
 *
 * 使用项目自带的轻量 JSON 解析器解析，序列化时手动拼 JSON（仅需转义字符串）。
 */
#include "lightgit/storage/config.h"
#include "lightgit/common/json.h"
#include "lightgit/common/buffer.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * 小工具：读取/写入文本文件
 * -------------------------------------------------------------------------- */
static char *LgReadFileUtf8(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    char *buf;
    DWORD read = 0;

    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (16 << 20)) {
        CloseHandle(h);
        return NULL;
    }
    buf = (char *)malloc((size_t)sz.QuadPart + 1);
    if (buf == NULL) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &read, NULL)) {
        free(buf); CloseHandle(h); return NULL;
    }
    buf[read] = '\0';
    CloseHandle(h);
    return buf;
}

static int LgWriteFileUtf8(const wchar_t *path, const char *text)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    static const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    if (h == INVALID_HANDLE_VALUE) return -1;
    WriteFile(h, bom, 3, &written, NULL);   /* UTF-8 BOM，便于记事本识别 */
    WriteFile(h, text, (DWORD)strlen(text), &written, NULL);
    CloseHandle(h);
    return 0;
}

/* --------------------------------------------------------------------------
 * JSON 字符串转义（把 s 转义后写入 out 缓冲区，含首尾引号）
 * -------------------------------------------------------------------------- */
static int LgJsonEscapeInto(lg_buffer *b, const char *s)
{
    static const char hex[] = "0123456789abcdef";
    const char *p;

    if (lg_buffer_append(b, "\"", 1) != 0) return -1;
    for (p = s; p != NULL && *p; p++) {
        char c = *p;
        char esc[2];
        switch (c) {
        case '"':  lg_buffer_append(b, "\\\"", 2); break;
        case '\\': lg_buffer_append(b, "\\\\", 2); break;
        case '\n': lg_buffer_append(b, "\\n", 2); break;
        case '\r': lg_buffer_append(b, "\\r", 2); break;
        case '\t': lg_buffer_append(b, "\\t", 2); break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[6];
                buf[0] = '\\'; buf[1] = 'u'; buf[2] = '0'; buf[3] = '0';
                buf[4] = hex[(unsigned char)c >> 4];
                buf[5] = hex[(unsigned char)c & 0xF];
                lg_buffer_append(b, buf, 6);
            } else {
                esc[0] = c;
                lg_buffer_append(b, esc, 1);
            }
        }
    }
    return lg_buffer_append(b, "\"", 1);
}

int lg_config_load(const wchar_t *data_dir, lg_config *cfg)
{
    wchar_t path[MAX_PATH];
    char *text;
    lg_json_value *root = NULL;

    if (cfg == NULL) return -1;
    memset(cfg, 0, sizeof(*cfg));

    swprintf(path, MAX_PATH, L"%s\\config.json", data_dir);
    text = LgReadFileUtf8(path);
    if (text == NULL) {
        return 0;   /* 无配置文件 = 空配置 */
    }

    if (lg_json_parse(text, strlen(text), &root) != 0) {
        free(text);
        return -1;
    }
    free(text);

    {
        const char *u = lg_json_get_string(root, "username");
        const char *b = lg_json_get_string(root, "default_branch");
        const char *l = lg_json_get_string(root, "language");
        lg_json_value *projects = lg_json_get(root, "projects");

        if (u) cfg->username = _strdup(u);
        cfg->default_branch = _strdup(b ? b : "main");
        cfg->language = _strdup(l ? l : "zh");

        if (projects != NULL && projects->type == LG_JSON_ARRAY) {
            size_t i;
            cfg->projects = (lg_project *)calloc(projects->count, sizeof(lg_project));
            if (cfg->projects == NULL) {
                lg_json_free(root);
                return -1;
            }
            for (i = 0; i < projects->count; i++) {
                lg_json_value *item = lg_json_at(projects, i);
                const char *o = lg_json_get_string(item, "owner");
                const char *r = lg_json_get_string(item, "repo");
                const char *f = lg_json_get_string(item, "folder");
                const char *br = lg_json_get_string(item, "branch");
                lg_project *p = &cfg->projects[cfg->project_count++];
                memset(p, 0, sizeof(*p));
                if (o) p->owner = _strdup(o);
                if (r) p->repo = _strdup(r);
                if (f) p->folder = _strdup(f);
                if (br) p->branch = _strdup(br);
            }
        }
    }

    lg_json_free(root);
    return 0;
}

int lg_config_save(const wchar_t *data_dir, const lg_config *cfg)
{
    wchar_t path[MAX_PATH];
    lg_buffer b;
    size_t i;
    int rc = -1;

    if (cfg == NULL) return -1;

    lg_buffer_init(&b);
    lg_buffer_append(&b, "{", 1);

    lg_buffer_append(&b, "\"username\":", 11);
    LgJsonEscapeInto(&b, cfg->username ? cfg->username : "");

    lg_buffer_append(&b, ",\"default_branch\":", 18);
    LgJsonEscapeInto(&b, cfg->default_branch ? cfg->default_branch : "main");

    lg_buffer_append(&b, ",\"language\":", 12);
    LgJsonEscapeInto(&b, cfg->language ? cfg->language : "zh");

    lg_buffer_append(&b, ",\"projects\":[", 13);
    for (i = 0; i < cfg->project_count; i++) {
        lg_project *p = &cfg->projects[i];
        if (i > 0) lg_buffer_append(&b, ",", 1);
        lg_buffer_append(&b, "{\"owner\":", 9);
        LgJsonEscapeInto(&b, p->owner ? p->owner : "");
        lg_buffer_append(&b, ",\"repo\":", 8);
        LgJsonEscapeInto(&b, p->repo ? p->repo : "");
        lg_buffer_append(&b, ",\"folder\":", 10);
        LgJsonEscapeInto(&b, p->folder ? p->folder : "");
        lg_buffer_append(&b, ",\"branch\":", 10);
        LgJsonEscapeInto(&b, p->branch ? p->branch : "main");
        lg_buffer_append(&b, "}", 1);
    }
    lg_buffer_append(&b, "]}", 2);
    if (lg_buffer_append(&b, "", 1) != 0) {   /* 结尾 NUL */
        lg_buffer_free(&b);
        return -1;
    }

    swprintf(path, MAX_PATH, L"%s\\config.json", data_dir);
    if (LgWriteFileUtf8(path, (const char *)b.data) == 0) {
        rc = 0;
    }

    lg_buffer_free(&b);
    return rc;
}

int lg_config_upsert_project(lg_config *cfg, const char *owner, const char *repo,
                             const char *folder, const char *branch)
{
    size_t i;

    if (cfg == NULL || owner == NULL || repo == NULL) return -1;

    for (i = 0; i < cfg->project_count; i++) {
        lg_project *p = &cfg->projects[i];
        if (strcmp(p->owner, owner) == 0 && strcmp(p->repo, repo) == 0) {
            if (folder) { free(p->folder); p->folder = _strdup(folder); }
            if (branch) { free(p->branch); p->branch = _strdup(branch); }
            return 0;
        }
    }

    {
        lg_project *np = (lg_project *)realloc(cfg->projects,
                                               (cfg->project_count + 1) * sizeof(*np));
        lg_project *p;
        if (np == NULL) return -1;
        cfg->projects = np;
        p = &cfg->projects[cfg->project_count++];
        memset(p, 0, sizeof(*p));
        p->owner = _strdup(owner);
        p->repo = _strdup(repo);
        if (folder) p->folder = _strdup(folder);
        if (branch) p->branch = _strdup(branch);
        return 0;
    }
}

void lg_config_clear(lg_config *cfg)
{
    size_t i;
    if (cfg == NULL) return;
    free(cfg->username);
    free(cfg->default_branch);
    free(cfg->language);
    for (i = 0; i < cfg->project_count; i++) {
        free(cfg->projects[i].owner);
        free(cfg->projects[i].repo);
        free(cfg->projects[i].folder);
        free(cfg->projects[i].branch);
    }
    free(cfg->projects);
    memset(cfg, 0, sizeof(*cfg));
}
