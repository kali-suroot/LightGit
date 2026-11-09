/*
 * test_blob.c — Git Blob 单元测试
 *
 * 期望向量由官方 Git（git hash-object --stdin）生成：
 *   空内容                → e69de29bb2d1d6434b8b29ae775ad8c2e48c5391
 *   "hello world\n"       → 3b18e512dba79e4c8300dd08aeb37f8e728b8dad
 *   "LightGit test content\n" → 6ab0f81035d17d5bbdeef79f9ac198cfecdefba4
 *   "中文测试\n"(UTF-8)   → eeee1f4efc67a372923b7b321712aa319c6ef93b
 */
#include "test_framework.h"
#include "lightgit/git/git_blob.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/* 内存输入向量的测试 */
static int LgTestBlobMemory(void)
{
    struct {
        const char *data;
        size_t      len;
        const char *expect;
    } vectors[] = {
        { "",                       0,                                "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391" },
        { "hello world\n",          strlen("hello world\n"),          "3b18e512dba79e4c8300dd08aeb37f8e728b8dad" },
        { "LightGit test content\n", strlen("LightGit test content\n"), "6ab0f81035d17d5bbdeef79f9ac198cfecdefba4" },
        /* UTF-8 编码的“中文测试\n”（共 13 字节） */
        { "\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95\n", 13, "eeee1f4efc67a372923b7b321712aa319c6ef93b" },
    };
    size_t i;

    for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};
        LG_TEST_ASSERT(lg_blob_hash_memory(vectors[i].data, vectors[i].len, out) == 0);
        LG_TEST_ASSERT_STREQ(out, vectors[i].expect);
    }
    return 0;
}

/* 文件输入：写入临时文件后用 lg_blob_hash_file 计算 */
static int LgTestBlobFile(void)
{
    const char *content = "hello world\n";
    const char *expect = "3b18e512dba79e4c8300dd08aeb37f8e728b8dad";
    wchar_t tmpDir[MAX_PATH];
    wchar_t tmpPath[MAX_PATH];
    HANDLE h;
    DWORD written = 0;
    char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};

    /* 取系统临时目录，构造 LightGit_test_blob_<pid>.tmp */
    LG_TEST_ASSERT(GetTempPathW(MAX_PATH, tmpDir) > 0);
    swprintf(tmpPath, MAX_PATH, L"%sLightGit_test_blob_%lu.tmp", tmpDir, GetCurrentProcessId());

    h = CreateFileW(tmpPath, GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LG_TEST_ASSERT(h != INVALID_HANDLE_VALUE);
    LG_TEST_ASSERT(WriteFile(h, content, (DWORD)strlen(content), &written, NULL) != 0);
    CloseHandle(h);

    LG_TEST_ASSERT(lg_blob_hash_file(tmpPath, out) == 0);
    LG_TEST_ASSERT_STREQ(out, expect);

    /* 清理临时文件 */
    DeleteFileW(tmpPath);
    return 0;
}

int lg_run_blob_tests(void)
{
    LG_TEST_ASSERT(LgTestBlobMemory() == 0);
    LG_TEST_ASSERT(LgTestBlobFile() == 0);
    return 0;
}
