/*
 * test_gitignore.c — .gitignore 匹配单元测试
 */
#include "test_framework.h"
#include "lightgit/file/gitignore.h"

#include <string.h>

/* 单条规则匹配便捷断言 */
static int LgExpect(const char *text, const char *path, int is_dir, int want)
{
    LgGitignore *gi = lg_gitignore_parse(text);
    int got;

    LG_TEST_ASSERT(gi != NULL);
    got = lg_gitignore_match(gi, path, is_dir);
    lg_gitignore_free(gi);

    if (got != want) {
        fprintf(stderr, "    gitignore(%s) vs %s -> got %d, want %d\n", text, path, got, want);
        return 1;
    }
    return 0;
}

static int LgTestGitignoreBasic(void)
{
    LG_TEST_ASSERT(LgExpect("*.o\n", "foo.o", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("*.o\n", "foo.c", 0, 0) == 0);
    LG_TEST_ASSERT(LgExpect("*.o\n", "sub/bar.o", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("build/\n", "build", 1, 1) == 0);
    LG_TEST_ASSERT(LgExpect("build/\n", "sub/build", 1, 1) == 0);
    LG_TEST_ASSERT(LgExpect("build/\n", "build", 0, 0) == 0);   /* 文件不匹配目录规则 */
    LG_TEST_ASSERT(LgExpect("/root.txt\n", "root.txt", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("/root.txt\n", "sub/root.txt", 0, 0) == 0);
    return 0;
}

static int LgTestGitignoreNegation(void)
{
    LG_TEST_ASSERT(LgExpect("*.o\n!keep.o\n", "keep.o", 0, 0) == 0);
    LG_TEST_ASSERT(LgExpect("*.o\n!keep.o\n", "drop.o", 0, 1) == 0);
    return 0;
}

static int LgTestGitignoreDoubleStar(void)
{
    LG_TEST_ASSERT(LgExpect("**/*.tmp\n", "a.tmp", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("**/*.tmp\n", "sub/a.tmp", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("**/*.tmp\n", "sub/deep/a.tmp", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("doc/*.txt\n", "doc/a.txt", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("doc/*.txt\n", "doc/sub/a.txt", 0, 0) == 0);
    return 0;
}

static int LgTestGitignoreMisc(void)
{
    LG_TEST_ASSERT(LgExpect("# comment\n\n*.log\n", "x.log", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("?at\n", "cat", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("?at\n", "at", 0, 0) == 0);
    LG_TEST_ASSERT(LgExpect("[abc].txt\n", "a.txt", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("[abc].txt\n", "d.txt", 0, 0) == 0);
    LG_TEST_ASSERT(LgExpect("[!a].txt\n", "b.txt", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("[!a].txt\n", "a.txt", 0, 0) == 0);
    return 0;
}

/* 目录规则应匹配其下所有文件（祖先目录匹配） */
static int LgTestGitignoreDirAncestor(void)
{
    LG_TEST_ASSERT(LgExpect("/build/\n", "build/Release/x.exe", 0, 1) == 0);
    LG_TEST_ASSERT(LgExpect("/build/\n", "src/main.c", 0, 0) == 0);
    LG_TEST_ASSERT(LgExpect("/data/\n", "data/credentials/pat_vault.json", 0, 1) == 0);
    return 0;
}

int lg_run_gitignore_tests(void)
{
    LG_TEST_ASSERT(LgTestGitignoreBasic() == 0);
    LG_TEST_ASSERT(LgTestGitignoreNegation() == 0);
    LG_TEST_ASSERT(LgTestGitignoreDoubleStar() == 0);
    LG_TEST_ASSERT(LgTestGitignoreMisc() == 0);
    LG_TEST_ASSERT(LgTestGitignoreDirAncestor() == 0);
    return 0;
}
