/*
 * test_scanner.c — 目录扫描单元测试
 *
 * 在临时目录创建文件/子目录/.git 目录，验证扫描结果与内置排除规则。
 */
#include "test_framework.h"
#include "lightgit/file/file_scanner.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

/* 创建文件并写入内容 */
static int LgWriteFile(const wchar_t *path, const char *content)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    WriteFile(h, content, (DWORD)strlen(content), &written, NULL);
    CloseHandle(h);
    return 0;
}

static int LgFindPath(const lg_scan_result *r, const char *path)
{
    size_t i;
    for (i = 0; i < r->count; i++) {
        if (strcmp(r->files[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int LgTestScanner(void)
{
    wchar_t tmp[MAX_PATH], root[MAX_PATH], sub[MAX_PATH], gitdir[MAX_PATH], fpath[MAX_PATH];
    lg_scan_result res;

    LG_TEST_ASSERT(GetTempPathW(MAX_PATH, tmp) > 0);
    swprintf(root, MAX_PATH, L"%s\\lg_scan_test", tmp);
    swprintf(sub, MAX_PATH, L"%s\\sub", root);
    swprintf(gitdir, MAX_PATH, L"%s\\.git", root);

    CreateDirectoryW(root, NULL);
    CreateDirectoryW(sub, NULL);
    CreateDirectoryW(gitdir, NULL);

    swprintf(fpath, MAX_PATH, L"%s\\a.txt", root);
    LG_TEST_ASSERT(LgWriteFile(fpath, "aaa") == 0);
    swprintf(fpath, MAX_PATH, L"%s\\b.txt", root);
    LG_TEST_ASSERT(LgWriteFile(fpath, "bbb") == 0);
    swprintf(fpath, MAX_PATH, L"%s\\c.txt", sub);
    LG_TEST_ASSERT(LgWriteFile(fpath, "ccc") == 0);
    swprintf(fpath, MAX_PATH, L"%s\\config", gitdir);
    LG_TEST_ASSERT(LgWriteFile(fpath, "d") == 0);

    memset(&res, 0, sizeof(res));
    LG_TEST_ASSERT(lg_scan_directory(root, &res) == 0);
    LG_TEST_ASSERT(res.count == 3);   /* a.txt, b.txt, sub/c.txt（.git 被排除） */
    LG_TEST_ASSERT(LgFindPath(&res, "a.txt") == 1);
    LG_TEST_ASSERT(LgFindPath(&res, "b.txt") == 1);
    LG_TEST_ASSERT(LgFindPath(&res, "sub/c.txt") == 1);
    LG_TEST_ASSERT(LgFindPath(&res, ".git/config") == 0);

    lg_scan_result_free(&res);

    /* 清理临时目录 */
    swprintf(fpath, MAX_PATH, L"%s\\config", gitdir);
    DeleteFileW(fpath);
    RemoveDirectoryW(gitdir);
    swprintf(fpath, MAX_PATH, L"%s\\a.txt", root);
    DeleteFileW(fpath);
    swprintf(fpath, MAX_PATH, L"%s\\b.txt", root);
    DeleteFileW(fpath);
    swprintf(fpath, MAX_PATH, L"%s\\c.txt", sub);
    DeleteFileW(fpath);
    RemoveDirectoryW(sub);
    RemoveDirectoryW(root);
    return 0;
}

int lg_run_scanner_tests(void)
{
    LG_TEST_ASSERT(LgTestScanner() == 0);
    return 0;
}
