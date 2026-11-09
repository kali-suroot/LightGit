/*
 * test_hash.c — SHA-1 单元测试
 *
 * 使用 FIPS 180-4 标准 SHA-1 测试向量验证 BCrypt 封装是否正确。
 */
#include "test_framework.h"
#include "lightgit/git/git_hash.h"
#include "lightgit/common/hex.h"

/* 单条向量：输入字符串 + 期望的 40 字符十六进制 SHA-1 */
static int LgTestSha1OneShot(void)
{
    struct {
        const char *input;
        const char *expect;
    } vectors[] = {
        { "",                                             "da39a3ee5e6b4b0d3255bfef95601890afd80709" },
        { "abc",                                          "a9993e364706816aba3e25717850c26c9cd0d89d" },
        { "The quick brown fox jumps over the lazy dog",  "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                                                          "84983e441c3bd26ebaae4aa1f95129e5e54670f1" },
    };
    size_t i;

    for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        char out[LG_GIT_HASH_HEX_SIZE + 1] = {0};
        LG_TEST_ASSERT(lg_sha1_hex(vectors[i].input, strlen(vectors[i].input), out) == 0);
        LG_TEST_ASSERT_STREQ(out, vectors[i].expect);
    }
    return 0;
}

/* 流式接口：分多次 update，结果应与一次性计算一致 */
static int LgTestSha1Streaming(void)
{
    const char *part1 = "The quick brown fox ";
    const char *part2 = "jumps over ";
    const char *part3 = "the lazy dog";
    const char *expect = "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12";
    lg_sha1_ctx *ctx = NULL;
    unsigned char raw[LG_GIT_HASH_RAW_SIZE];
    char hex[LG_GIT_HASH_HEX_SIZE + 1];

    LG_TEST_ASSERT(lg_sha1_init(&ctx) == 0);
    LG_TEST_ASSERT(lg_sha1_update(ctx, part1, strlen(part1)) == 0);
    LG_TEST_ASSERT(lg_sha1_update(ctx, part2, strlen(part2)) == 0);
    LG_TEST_ASSERT(lg_sha1_update(ctx, part3, strlen(part3)) == 0);
    LG_TEST_ASSERT(lg_sha1_final(ctx, raw) == 0);

    lg_hex_encode(raw, LG_GIT_HASH_RAW_SIZE, hex);
    LG_TEST_ASSERT_STREQ(hex, expect);
    return 0;
}

/* 空输入与空 update 的边界 */
static int LgTestSha1Edge(void)
{
    lg_sha1_ctx *ctx = NULL;
    unsigned char raw[LG_GIT_HASH_RAW_SIZE];
    char hex[LG_GIT_HASH_HEX_SIZE + 1];

    LG_TEST_ASSERT(lg_sha1_init(&ctx) == 0);
    LG_TEST_ASSERT(lg_sha1_update(ctx, NULL, 0) == 0);   /* 空数据应成功 */
    LG_TEST_ASSERT(lg_sha1_final(ctx, raw) == 0);
    lg_hex_encode(raw, LG_GIT_HASH_RAW_SIZE, hex);
    LG_TEST_ASSERT_STREQ(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    return 0;
}

int lg_run_hash_tests(void)
{
    LG_TEST_ASSERT(LgTestSha1OneShot() == 0);
    LG_TEST_ASSERT(LgTestSha1Streaming() == 0);
    LG_TEST_ASSERT(LgTestSha1Edge() == 0);
    return 0;
}
