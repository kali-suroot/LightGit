/*
 * test_tree.c — Git Tree 单元测试
 *
 * 期望向量由官方 Git 生成（见 git_tree.h 注释）：
 *   - sub 树（单文件 c.txt）：d1035b8d0ae0fcda56dc6aeb8b73c456f05fcb3f
 *   - 根树（a.txt + b.txt + sub/）：07652b7ddf3d209ad88733fb43f615dab2738740
 *   - 排序边界（文件 foo 与目录 foodir/）：d1e4058d30d28f8efba3e3eb00b164473faef339
 */
#include "test_framework.h"
#include "lightgit/git/git_tree.h"
#include "lightgit/common/hex.h"

#include <string.h>

/* 解码 40 字符十六进制为 20 字节二进制，失败返回 0 */
static int LgDecodeHex(const char *hex, unsigned char out[LG_GIT_HASH_RAW_SIZE])
{
    return lg_hex_decode(hex, out, LG_GIT_HASH_RAW_SIZE) == LG_GIT_HASH_RAW_SIZE;
}

/* 单文件子树 */
static int LgTestTreeSingleFile(void)
{
    unsigned char c_txt[LG_GIT_HASH_RAW_SIZE];
    lg_tree_entry entries[1];
    char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};

    LG_TEST_ASSERT(LgDecodeHex("b2a7546679fdf79ca0eb7bfbee1e1bb342487380", c_txt));

    entries[0].mode = LG_TREE_MODE_FILE;
    entries[0].name = "c.txt";
    entries[0].sha1 = c_txt;

    LG_TEST_ASSERT(lg_tree_hash(entries, 1, out) == 0);
    LG_TEST_ASSERT_STREQ(out, "d1035b8d0ae0fcda56dc6aeb8b73c456f05fcb3f");
    return 0;
}

/* 根树：a.txt + b.txt + sub/（故意乱序传入，验证排序） */
static int LgTestTreeRootUnsorted(void)
{
    unsigned char a_txt[LG_GIT_HASH_RAW_SIZE];
    unsigned char b_txt[LG_GIT_HASH_RAW_SIZE];
    unsigned char sub[LG_GIT_HASH_RAW_SIZE];
    lg_tree_entry entries[3];
    char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};

    LG_TEST_ASSERT(LgDecodeHex("72943a16fb2c8f38f9dde202b7a70ccc19c52f34", a_txt));
    LG_TEST_ASSERT(LgDecodeHex("f761ec192d9f0dca3329044b96ebdb12839dbff6", b_txt));
    LG_TEST_ASSERT(LgDecodeHex("d1035b8d0ae0fcda56dc6aeb8b73c456f05fcb3f", sub));

    /* 乱序传入：sub / b.txt / a.txt，排序后应得到与 git 一致的 tree */
    entries[0].mode = LG_TREE_MODE_DIR;
    entries[0].name = "sub";
    entries[0].sha1 = sub;

    entries[1].mode = LG_TREE_MODE_FILE;
    entries[1].name = "b.txt";
    entries[1].sha1 = b_txt;

    entries[2].mode = LG_TREE_MODE_FILE;
    entries[2].name = "a.txt";
    entries[2].sha1 = a_txt;

    LG_TEST_ASSERT(lg_tree_hash(entries, 3, out) == 0);
    LG_TEST_ASSERT_STREQ(out, "07652b7ddf3d209ad88733fb43f615dab2738740");
    return 0;
}

/* 排序边界：文件 "foo" 与目录 "foodir"（目录名按 "foodir/" 比较） */
static int LgTestTreeSortDirPrefix(void)
{
    unsigned char foo_blob[LG_GIT_HASH_RAW_SIZE];
    unsigned char foodir_tree[LG_GIT_HASH_RAW_SIZE];
    lg_tree_entry entries[2];
    unsigned char buf[256];
    size_t len = 0;
    char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};

    LG_TEST_ASSERT(LgDecodeHex("25c5945565258f97a605a32733941c773ea77185", foo_blob));
    LG_TEST_ASSERT(LgDecodeHex("9359d67574440937df125205034f3c23bbef6af5", foodir_tree));

    entries[0].mode = LG_TREE_MODE_DIR;
    entries[0].name = "foodir";
    entries[0].sha1 = foodir_tree;

    entries[1].mode = LG_TREE_MODE_FILE;
    entries[1].name = "foo";
    entries[1].sha1 = foo_blob;

    LG_TEST_ASSERT(lg_tree_hash(entries, 2, out) == 0);
    LG_TEST_ASSERT_STREQ(out, "d1e4058d30d28f8efba3e3eb00b164473faef339");

    /* 同时校验序列化顺序：文件 "foo" 应排在目录 "foodir" 之前。
       tree 内容含二进制 SHA-1 与 NUL，不能用 strstr，改用 memcmp 逐项校验：
       第 1 项 = "100644 foo\0" + 20 字节，占 11 + 20 = 31 字节；
       第 2 项从 offset 31 开始。 */
    LG_TEST_ASSERT(lg_tree_build(entries, 2, buf, sizeof(buf), &len) == 0);
    LG_TEST_ASSERT(len == 31 + (12 + 1 + 20));
    LG_TEST_ASSERT(memcmp(buf, "100644 foo", 10) == 0);
    LG_TEST_ASSERT(memcmp(buf + 31, "40000 foodir", 12) == 0);
    return 0;
}

int lg_run_tree_tests(void)
{
    LG_TEST_ASSERT(LgTestTreeSingleFile() == 0);
    LG_TEST_ASSERT(LgTestTreeRootUnsorted() == 0);
    LG_TEST_ASSERT(LgTestTreeSortDirPrefix() == 0);
    return 0;
}
