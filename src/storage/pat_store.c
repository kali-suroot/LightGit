/*
 * pat_store.c — PAT 凭证库实现
 */
#include "lightgit/storage/pat_store.h"
#include "lightgit/storage/credential.h"
#include "lightgit/common/json.h"
#include "lightgit/common/buffer.h"
#include "lightgit/common/base64.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *LgReadFileUtf8(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    char *buf;
    DWORD read = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (16 << 20)) {
        CloseHandle(h); return NULL;
    }
    buf = (char *)malloc((size_t)sz.QuadPart + 1);
    if (!buf) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &read, NULL)) { free(buf); CloseHandle(h); return NULL; }
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

/* JSON 字符串转义（含首尾引号），写入 buffer */
static int LgJsonEscape(lg_buffer *b, const char *s)
{
    const char *p;
    if (lg_buffer_append(b, "\"", 1) != 0) return -1;
    for (p = s; p != NULL && *p; p++) {
        char c = *p;
        char esc[2] = { c, 0 };
        switch (c) {
        case '"':  lg_buffer_append(b, "\\\"", 2); break;
        case '\\': lg_buffer_append(b, "\\\\", 2); break;
        case '\n': lg_buffer_append(b, "\\n", 2); break;
        case '\r': lg_buffer_append(b, "\\r", 2); break;
        case '\t': lg_buffer_append(b, "\\t", 2); break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                lg_buffer_append(b, buf, 6);
            } else {
                lg_buffer_append(b, esc, 1);
            }
        }
    }
    return lg_buffer_append(b, "\"", 1);
}

void lg_pat_store_clear(lg_pat_store *store)
{
    size_t i;
    if (store == NULL) return;
    for (i = 0; i < store->count; i++) {
        free(store->entries[i].note);
        free(store->entries[i].username);
        free(store->entries[i].blob);
    }
    free(store->entries);
    memset(store, 0, sizeof(*store));
    store->active_index = -1;
}

int lg_pat_store_load(const wchar_t *data_dir, lg_pat_store *store)
{
    wchar_t path[MAX_PATH];
    char *text;
    lg_json_value *root = NULL;
    size_t i;

    if (store == NULL) return -1;
    memset(store, 0, sizeof(*store));
    store->active_index = -1;

    swprintf(path, MAX_PATH, L"%s\\credentials\\pat_vault.json", data_dir);
    text = LgReadFileUtf8(path);
    if (text == NULL) return 0;   /* 无文件 = 空库 */

    if (lg_json_parse(text, strlen(text), &root) != 0) {
        free(text);
        return -1;
    }
    free(text);

    {
        lg_json_value *pats = lg_json_get(root, "pats");
        lg_json_value *active = lg_json_get(root, "active");
        long long ai = -1;
        if (active) lg_json_int64(active, &ai);
        store->active_index = (int)ai;

        if (pats != NULL && pats->type == LG_JSON_ARRAY) {
            store->entries = (lg_pat_entry *)calloc(pats->count ? pats->count : 1,
                                                    sizeof(lg_pat_entry));
            if (store->entries == NULL) { lg_json_free(root); return -1; }

            for (i = 0; i < pats->count; i++) {
                lg_json_value *item = lg_json_at(pats, i);
                const char *note = lg_json_get_string(item, "note");
                const char *user = lg_json_get_string(item, "username");
                const char *blob = lg_json_get_string(item, "blob");
                lg_pat_entry *e = &store->entries[store->count];

                if (note == NULL || blob == NULL) continue;

                e->note = _strdup(note);
                e->username = user ? _strdup(user) : NULL;
                e->blob_len = strlen(blob) / 4 * 3 + 4;
                e->blob = (unsigned char *)malloc(e->blob_len);
                if (e->blob == NULL) { lg_json_free(root); return -1; }
                {
                    int n = lg_base64_decode(blob, strlen(blob), e->blob, e->blob_len);
                    if (n < 0) { free(e->blob); e->blob = NULL; continue; }
                    e->blob_len = (size_t)n;
                }
                store->count++;
            }
        }
    }

    lg_json_free(root);
    return 0;
}

int lg_pat_store_save(const wchar_t *data_dir, const lg_pat_store *store)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    lg_buffer b;
    size_t i;
    int rc = -1;

    if (store == NULL) return -1;

    swprintf(dir, MAX_PATH, L"%s\\credentials", data_dir);
    CreateDirectoryW(dir, NULL);
    swprintf(path, MAX_PATH, L"%s\\pat_vault.json", dir);

    lg_buffer_init(&b);
    lg_buffer_append(&b, "{\"active\":", 10);
    {
        char ab[32];
        snprintf(ab, sizeof(ab), "%d", store->active_index);
        lg_buffer_append(&b, ab, strlen(ab));
    }
    lg_buffer_append(&b, ",\"pats\":[", 9);
    for (i = 0; i < store->count; i++) {
        lg_pat_entry *e = &store->entries[i];
        char *b64 = NULL;
        int bl;

        if (i > 0) lg_buffer_append(&b, ",", 1);
        lg_buffer_append(&b, "{\"note\":", 8);
        LgJsonEscape(&b, e->note ? e->note : "");
        lg_buffer_append(&b, ",\"username\":", 12);
        LgJsonEscape(&b, e->username ? e->username : "");
        lg_buffer_append(&b, ",\"blob\":", 8);
        b64 = (char *)malloc(e->blob_len * 2 + 8);
        if (b64 == NULL) { lg_buffer_free(&b); return -1; }
        bl = lg_base64_encode(e->blob, e->blob_len, b64, e->blob_len * 2 + 8);
        if (bl < 0) { free(b64); lg_buffer_free(&b); return -1; }
        LgJsonEscape(&b, b64);
        free(b64);
        lg_buffer_append(&b, "}", 1);
    }
    lg_buffer_append(&b, "]}", 2);
    if (lg_buffer_append(&b, "", 1) != 0) { lg_buffer_free(&b); return -1; }

    if (LgWriteFileUtf8(path, (const char *)b.data) == 0) rc = 0;
    lg_buffer_free(&b);
    return rc;
}

int lg_pat_store_add(lg_pat_store *store, const char *note,
                     const char *username, const char *token)
{
    size_t i;
    unsigned char *blob = NULL;
    size_t blob_len = 0;

    if (store == NULL || note == NULL || token == NULL) return -1;
    if (note[0] == '\0') note = "PAT";

    if (lg_credential_protect(token, &blob, &blob_len) != 0) return -1;

    /* 按 note 去重更新 */
    for (i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].note, note) == 0) {
            free(store->entries[i].blob);
            store->entries[i].blob = blob;
            store->entries[i].blob_len = blob_len;
            if (username) {
                free(store->entries[i].username);
                store->entries[i].username = _strdup(username);
            }
            store->active_index = (int)i;
            return 0;
        }
    }

    {
        lg_pat_entry *ne = (lg_pat_entry *)realloc(store->entries,
                                                   (store->count + 1) * sizeof(*ne));
        if (ne == NULL) { free(blob); return -1; }
        store->entries = ne;
        store->entries[store->count].note = _strdup(note);
        store->entries[store->count].username = username ? _strdup(username) : NULL;
        store->entries[store->count].blob = blob;
        store->entries[store->count].blob_len = blob_len;
        store->active_index = (int)store->count;
        store->count++;
    }
    return 0;
}

int lg_pat_store_remove(lg_pat_store *store, int index)
{
    size_t i;
    if (store == NULL || index < 0 || (size_t)index >= store->count) return -1;

    free(store->entries[index].note);
    free(store->entries[index].username);
    free(store->entries[index].blob);

    for (i = (size_t)index; i + 1 < store->count; i++) {
        store->entries[i] = store->entries[i + 1];
    }
    store->count--;

    if (store->count == 0) store->active_index = -1;
    else if (store->active_index == index) store->active_index = index < (int)store->count ? index : (int)store->count - 1;
    else if (store->active_index > index) store->active_index--;
    return 0;
}

int lg_pat_store_get_token(const lg_pat_store *store, int index, char *out, size_t cap)
{
    char *plain = NULL;
    if (store == NULL || index < 0 || (size_t)index >= store->count || out == NULL) return -1;
    if (lg_credential_unprotect(store->entries[index].blob,
                                store->entries[index].blob_len, &plain) != 0) {
        return -1;
    }
    snprintf(out, cap, "%s", plain);
    free(plain);
    return 0;
}
