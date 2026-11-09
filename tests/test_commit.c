/*
 * test_commit.c — Git Commit 单元测试
 *
 * 期望向量由官方 Git 生成（固定时间戳 1700000000 +0800，保证确定性）：
 *   tree 07652b7ddf3d209ad88733fb43f615dab2738740
 *   author/committer Test <test@example.com>
 *   message "init commit"
 *   → commit 6f6c2bcf6c7693471e798cf651c11e989b23fda0
 */
#include "test_framework.h"
#include "lightgit/git/git_commit.h"

#include <stdlib.h>
#include <string.h>

static const lg_commit_author g_author = {
    "Test", "test@example.com", 1700000000LL, "+0800"
};

/* Root commit 的完整内容与哈希 */
static int LgTestCommitRoot(void)
{
    const char *tree = "07652b7ddf3d209ad88733fb43f615dab2738740";
    const char *expect_content =
        "tree 07652b7ddf3d209ad88733fb43f615dab2738740\n"
        "author Test <test@example.com> 1700000000 +0800\n"
        "committer Test <test@example.com> 1700000000 +0800\n"
        "\n"
        "init commit\n";
    char *content = NULL;
    size_t len = 0;
    char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};

    LG_TEST_ASSERT(lg_commit_build(tree, NULL, &g_author, &g_author,
                                   "init commit", &content, &len) == 0);
    LG_TEST_ASSERT(content != NULL);
    LG_TEST_ASSERT(len == strlen(expect_content));
    LG_TEST_ASSERT_STREQ(content, expect_content);
    free(content);

    LG_TEST_ASSERT(lg_commit_hash(tree, NULL, &g_author, &g_author,
                                  "init commit", out) == 0);
    LG_TEST_ASSERT_STREQ(out, "6f6c2bcf6c7693471e798cf651c11e989b23fda0");
    return 0;
}

/* 带父提交：内容中应包含 "parent <hex>\n" 且位置在 tree 行之后 */
static int LgTestCommitWithParent(void)
{
    const char *tree = "07652b7ddf3d209ad88733fb43f615dab2738740";
    const char *parent = "6f6c2bcf6c7693471e798cf651c11e989b23fda0";
    char *content = NULL;
    size_t len = 0;

    LG_TEST_ASSERT(lg_commit_build(tree, parent, &g_author, &g_author,
                                   "second commit", &content, &len) == 0);
    LG_TEST_ASSERT(content != NULL);

    /* 校验内容以 "tree ...\nparent ...\n" 开头 */
    LG_TEST_ASSERT(strncmp(content,
                           "tree 07652b7ddf3d209ad88733fb43f615dab2738740\n"
                           "parent 6f6c2bcf6c7693471e798cf651c11e989b23fda0\n",
                           strlen("tree 07652b7ddf3d209ad88733fb43f615dab2738740\n"
                                  "parent 6f6c2bcf6c7693471e798cf651c11e989b23fda0\n")) == 0);
    free(content);
    return 0;
}

int lg_run_commit_tests(void)
{
    LG_TEST_ASSERT(LgTestCommitRoot() == 0);
    LG_TEST_ASSERT(LgTestCommitWithParent() == 0);
    return 0;
}
