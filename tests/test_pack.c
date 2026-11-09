/*
 * test_pack.c — Git Packfile 单元测试
 *
 * 测试 packfile 生成器：构造 blob/tree/commit 三个对象生成 packfile，
 * 然后逐项解包（解码变长对象头 + miniz 解压）验证内容，并校验尾部 SHA-1。
 * 官方 Git 兼容性（git index-pack）在构建流程中另行验证。
 */
#include "test_framework.h"
#include "lightgit/git/git_pack.h"
#include "lightgit/git/git_tree.h"
#include "lightgit/git/git_commit.h"
#include "lightgit/common/hex.h"

#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 解码变长对象头，返回消耗的字节数 */
static size_t LgReadVarint(const unsigned char *p, unsigned int *type, size_t *size)
{
    size_t pos = 0;
    unsigned char c = p[pos++];
    unsigned int shift = 4;

    *type = (unsigned int)((c >> 4) & 0x07);
    *size = (size_t)(c & 0x0F);
    while (c & 0x80) {
        c = p[pos++];
        *size |= ((size_t)(c & 0x7F)) << shift;
        shift += 7;
    }
    return pos;
}

static int LgTestPackRoundTrip(void)
{
    const char *blob_content = "hello world\n";
    size_t blob_len = strlen(blob_content);

    unsigned char c_txt[LG_GIT_HASH_RAW_SIZE];
    lg_tree_entry entry;
    unsigned char tree_buf[64];
    size_t tree_len = 0;

    const lg_commit_author author = { "Test", "test@example.com", 1700000000LL, "+0800" };
    char *commit_content = NULL;
    size_t commit_len = 0;

    lg_pack_object objects[3];
    unsigned char *pack = NULL;
    size_t pack_len = 0;
    size_t pos = 12;   /* 跳过 12 字节头 */
    int i;

    /* ---- 准备三个对象：blob / tree / commit ---- */
    LG_TEST_ASSERT(lg_hex_decode("b2a7546679fdf79ca0eb7bfbee1e1bb342487380",
                                 c_txt, LG_GIT_HASH_RAW_SIZE) == LG_GIT_HASH_RAW_SIZE);

    entry.mode = LG_TREE_MODE_FILE;
    entry.name = "c.txt";
    entry.sha1 = c_txt;
    LG_TEST_ASSERT(lg_tree_build(&entry, 1, tree_buf, sizeof(tree_buf), &tree_len) == 0);

    LG_TEST_ASSERT(lg_commit_build("07652b7ddf3d209ad88733fb43f615dab2738740", NULL,
                                   &author, &author, "init commit",
                                   &commit_content, &commit_len) == 0);

    objects[0].type = LG_OBJECT_BLOB;
    objects[0].content = (const unsigned char *)blob_content;
    objects[0].content_len = blob_len;

    objects[1].type = LG_OBJECT_TREE;
    objects[1].content = tree_buf;
    objects[1].content_len = tree_len;

    objects[2].type = LG_OBJECT_COMMIT;
    objects[2].content = (const unsigned char *)commit_content;
    objects[2].content_len = commit_len;

    /* ---- 生成 packfile ---- */
    LG_TEST_ASSERT(lg_pack_generate(objects, 3, &pack, &pack_len) == 0);
    LG_TEST_ASSERT(pack != NULL);
    LG_TEST_ASSERT(pack_len >= LG_PACK_HEADER_SIZE + LG_PACK_TRAILER_SIZE);

    /* 可选：通过环境变量 LG_PACK_DUMP 将生成的 packfile 写出，
       供官方 git（index-pack/verify-pack）做兼容性验证 */
    {
        char dump_path[512];
        size_t required = 0;
        FILE *f = NULL;
        if (getenv_s(&required, dump_path, sizeof(dump_path), "LG_PACK_DUMP") == 0 &&
            required > 0 && dump_path[0] != '\0') {
            if (fopen_s(&f, dump_path, "wb") == 0) {
                fwrite(pack, 1, pack_len, f);
                fclose(f);
            }
        }
    }

    /* ---- 头：PACK + 版本 2 + 对象数 3 ---- */
    LG_TEST_ASSERT(memcmp(pack, "PACK", 4) == 0);
    LG_TEST_ASSERT(pack[4] == 0 && pack[5] == 0 && pack[6] == 0 && pack[7] == 2);
    LG_TEST_ASSERT(((size_t)pack[8] << 24 | (size_t)pack[9] << 16 |
                    (size_t)pack[10] << 8 | (size_t)pack[11]) == 3);

    /* ---- 逐对象解包并比对内容 ---- */
    for (i = 0; i < 3; i++) {
        unsigned int type = 0;
        size_t size = 0;
        unsigned char *inflated;
        mz_stream s;
        int st;

        pos += LgReadVarint(pack + pos, &type, &size);
        LG_TEST_ASSERT(type == (unsigned int)objects[i].type);
        LG_TEST_ASSERT(size == objects[i].content_len);

        inflated = (unsigned char *)malloc(size + 1);
        LG_TEST_ASSERT(inflated != NULL);

        memset(&s, 0, sizeof(s));
        s.next_in = pack + pos;
        s.avail_in = (unsigned int)(pack_len - pos);
        s.next_out = inflated;
        s.avail_out = (unsigned int)(size + 1);

        LG_TEST_ASSERT(mz_inflateInit(&s) == MZ_OK);
        st = mz_inflate(&s, MZ_FINISH);
        mz_inflateEnd(&s);

        LG_TEST_ASSERT(st == MZ_STREAM_END);
        LG_TEST_ASSERT((size_t)s.total_out == size);
        LG_TEST_ASSERT(memcmp(inflated, objects[i].content, size) == 0);

        pos += (size_t)s.total_in;   /* 跳过该对象的压缩数据 */
        free(inflated);
    }

    /* ---- 尾部：SHA-1 校验 ---- */
    LG_TEST_ASSERT(pos == pack_len - LG_PACK_TRAILER_SIZE);
    {
        char recomputed[LG_GIT_HASH_HEX_SIZE + 1];
        char stored[LG_GIT_HASH_HEX_SIZE + 1];
        LG_TEST_ASSERT(lg_sha1_hex(pack, pack_len - LG_PACK_TRAILER_SIZE, recomputed) == 0);
        lg_hex_encode(pack + pack_len - LG_PACK_TRAILER_SIZE, LG_PACK_TRAILER_SIZE, stored);
        LG_TEST_ASSERT_STREQ(recomputed, stored);
    }

    free(commit_content);
    free(pack);
    return 0;
}

int lg_run_pack_tests(void)
{
    LG_TEST_ASSERT(LgTestPackRoundTrip() == 0);
    return 0;
}
