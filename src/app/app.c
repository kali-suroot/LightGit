/*
 * app.c — 全局应用状态实现
 */
#include "lightgit/app/app.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

lg_app *lg_app_get(void)
{
    static lg_app s_app;
    return &s_app;
}

int lg_app_get_data_dir(wchar_t *out, size_t cap)
{
    wchar_t exe[MAX_PATH];
    wchar_t *slash;

    if (out == NULL || cap == 0) {
        return -1;
    }

    if (GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) {
        return -1;
    }
    slash = wcsrchr(exe, L'\\');
    if (slash != NULL) {
        *slash = L'\0';
    }

    swprintf(out, cap, L"%s\\data", exe);
    CreateDirectoryW(out, NULL);   /* 不存在则创建 */
    return 0;
}
