/*
 * file_scanner.c — 本地目录递归扫描实现
 */
#include "lightgit/file/file_scanner.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 宽字符串 → UTF-8（malloc） */
static char *LgWToUtf8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *u;
    if (n <= 0) {
        return NULL;
    }
    u = (char *)malloc((size_t)n);
    if (u == NULL) {
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, w, -1, u, n, NULL, NULL);
    return u;
}

/* 拼接相对路径：prefix + "/" + name（prefix 为空则直接 name），malloc 返回 */
static char *LgJoinRel(const char *prefix, const char *name)
{
    size_t plen = prefix ? strlen(prefix) : 0;
    size_t nlen = strlen(name);
    char *out = (char *)malloc(plen + (plen ? 1 : 0) + nlen + 1);

    if (out == NULL) {
        return NULL;
    }
    if (plen) {
        memcpy(out, prefix, plen);
        out[plen] = '/';
        memcpy(out + plen + 1, name, nlen + 1);
    } else {
        memcpy(out, name, nlen + 1);
    }
    return out;
}

static int LgScanRecurse(lg_scan_result *out, const wchar_t *dir_path, const char *rel_prefix)
{
    wchar_t pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;

    swprintf(pattern, MAX_PATH * 2, L"%s\\*", dir_path);
    hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return -1;
    }

    do {
        char *name_u8;
        char *rel;

        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        name_u8 = LgWToUtf8(fd.cFileName);
        if (name_u8 == NULL) {
            FindClose(hFind);
            return -1;
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* 内置排除 .git 目录 */
            if (strcmp(name_u8, ".git") == 0) {
                free(name_u8);
                continue;
            }
            rel = LgJoinRel(rel_prefix, name_u8);
            if (rel == NULL) {
                free(name_u8);
                FindClose(hFind);
                return -1;
            }
            {
                wchar_t sub[MAX_PATH * 2];
                swprintf(sub, MAX_PATH * 2, L"%s\\%s", dir_path, fd.cFileName);
                if (LgScanRecurse(out, sub, rel) != 0) {
                    free(rel);
                    free(name_u8);
                    FindClose(hFind);
                    return -1;
                }
            }
            free(rel);
        } else {
            lg_scanned_file *nf;

            rel = LgJoinRel(rel_prefix, name_u8);
            if (rel == NULL) {
                free(name_u8);
                FindClose(hFind);
                return -1;
            }

            nf = (lg_scanned_file *)realloc(out->files,
                                            (out->count + 1) * sizeof(*nf));
            if (nf == NULL) {
                free(rel);
                free(name_u8);
                FindClose(hFind);
                return -1;
            }
            out->files = nf;
            out->files[out->count].path = rel;
            out->files[out->count].size =
                ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            out->count++;
        }
        free(name_u8);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return 0;
}

int lg_scan_directory(const wchar_t *root_dir, lg_scan_result *out)
{
    if (out == NULL) {
        return -1;
    }
    out->files = NULL;
    out->count = 0;
    return LgScanRecurse(out, root_dir, NULL);
}

void lg_scan_result_free(lg_scan_result *r)
{
    size_t i;
    if (r == NULL) {
        return;
    }
    for (i = 0; i < r->count; i++) {
        free(r->files[i].path);
    }
    free(r->files);
    r->files = NULL;
    r->count = 0;
}
