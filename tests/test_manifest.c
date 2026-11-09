/*
 * test_manifest.c — Manifest 与文件状态单元测试
 */
#include "test_framework.h"
#include "lightgit/storage/manifest.h"
#include "lightgit/file/file_status.h"

#include <stdlib.h>
#include <string.h>

static int LgTestManifestRoundTrip(void)
{
    const char *text =
        "3b18e512dba79e4c8300dd08aeb37f8e728b8dad hello.txt\n"
        "6ab0f81035d17d5bbdeef79f9ac198cfecdefba4 sub/世界.txt\n";
    lg_manifest m;
    char *out = NULL;

    memset(&m, 0, sizeof(m));
    LG_TEST_ASSERT(lg_manifest_parse(text, &m) == 0);
    LG_TEST_ASSERT(m.count == 2);
    LG_TEST_ASSERT_STREQ(lg_manifest_find(&m, "hello.txt"),
                         "3b18e512dba79e4c8300dd08aeb37f8e728b8dad");
    LG_TEST_ASSERT_STREQ(lg_manifest_find(&m, "sub/世界.txt"),
                         "6ab0f81035d17d5bbdeef79f9ac198cfecdefba4");
    LG_TEST_ASSERT(lg_manifest_find(&m, "missing.txt") == NULL);

    LG_TEST_ASSERT(lg_manifest_serialize(&m, &out) == 0);
    LG_TEST_ASSERT(out != NULL);
    /* 序列化后应包含所有条目 */
    LG_TEST_ASSERT(strstr(out, "hello.txt") != NULL);
    LG_TEST_ASSERT(strstr(out, "sub/世界.txt") != NULL);
    free(out);

    /* 更新与新增 */
    LG_TEST_ASSERT(lg_manifest_set(&m, "hello.txt", "0000000000000000000000000000000000000000") == 0);
    LG_TEST_ASSERT_STREQ(lg_manifest_find(&m, "hello.txt"),
                         "0000000000000000000000000000000000000000");
    LG_TEST_ASSERT(lg_manifest_set(&m, "new.txt", "1111111111111111111111111111111111111111") == 0);
    LG_TEST_ASSERT(m.count == 3);

    lg_manifest_clear(&m);
    return 0;
}

static int LgTestFileStatus(void)
{
    LG_TEST_ASSERT(lg_file_status_compute(NULL, "abc") == LG_FILE_ADDED);
    LG_TEST_ASSERT(lg_file_status_compute("abc", NULL) == LG_FILE_DELETED);
    LG_TEST_ASSERT(lg_file_status_compute("abc", "abc") == LG_FILE_UNCHANGED);
    LG_TEST_ASSERT(lg_file_status_compute("abc", "def") == LG_FILE_MODIFIED);
    return 0;
}

int lg_run_manifest_tests(void)
{
    LG_TEST_ASSERT(LgTestManifestRoundTrip() == 0);
    LG_TEST_ASSERT(LgTestFileStatus() == 0);
    return 0;
}
