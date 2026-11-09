/*
 * git_pack.c — Git Packfile 生成器实现
 *
 * 使用 miniz 的 zlib 风格 API 做对象压缩（zlib 格式，RFC 1950，与 Git 要求一致）。
 * 尾部的 SHA-1 校验和对“头 + 全部对象字节”计算。
 */
#include "lightgit/git/git_pack.h"
#include "lightgit/git/git_hash.h"

#include <miniz.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * 动态可增长字节缓冲区（内部辅助）
 * -------------------------------------------------------------------------- */
typedef struct LgBuffer {
    unsigned char *data;
    size_t         len;
    size_t         cap;
} LgBuffer;

static int LgBufferReserve(LgBuffer *b, size_t extra)
{
    size_t need = b->len + extra;
    size_t newcap;
    unsigned char *nd;

    if (need <= b->cap) {
        return 0;
    }

    newcap = (b->cap == 0) ? 4096 : b->cap;
    while (newcap < need) {
        newcap *= 2;
    }

    nd = (unsigned char *)realloc(b->data, newcap);
    if (nd == NULL) {
        return -1;
    }
    b->data = nd;
    b->cap = newcap;
    return 0;
}

static int LgBufferAppend(LgBuffer *b, const void *data, size_t len)
{
    if (len == 0) {
        return 0;
    }
    if (LgBufferReserve(b, len) != 0) {
        return -1;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    return 0;
}

/* --------------------------------------------------------------------------
 * 变长对象头编码
 * -------------------------------------------------------------------------- */
static size_t LgPackWriteObjectHeader(unsigned char out[16], unsigned int type, size_t size)
{
    unsigned char *p = out;
    unsigned char c = (unsigned char)(((type & 0x07) << 4) | (size & 0x0F));

    size >>= 4;
    while (size != 0) {
        *p++ = c | 0x80;                       /* 置后续标志位 */
        c = (unsigned char)(size & 0x7F);
        size >>= 7;
    }
    *p++ = c;                                  /* 最后一字节，无后续标志 */
    return (size_t)(p - out);
}

/* --------------------------------------------------------------------------
 * zlib 压缩：将 data 压缩后追加到缓冲区
 * -------------------------------------------------------------------------- */
static int LgPackDeflateAppend(LgBuffer *out, const unsigned char *data, size_t len)
{
    mz_stream stream;
    unsigned char obuf[8192];
    int status;

    memset(&stream, 0, sizeof(stream));
    /* avail_in 为 unsigned int；LightGit 单对象受 100 MiB 限制，安全 */
    stream.next_in = data;
    stream.avail_in = (unsigned int)len;

    if (mz_deflateInit(&stream, MZ_DEFAULT_COMPRESSION) != MZ_OK) {
        return -1;
    }

    do {
        stream.next_out = obuf;
        stream.avail_out = sizeof(obuf);
        status = mz_deflate(&stream, MZ_FINISH);
        if (status != MZ_OK && status != MZ_STREAM_END) {
            mz_deflateEnd(&stream);
            return -1;
        }
        if (LgBufferAppend(out, obuf, sizeof(obuf) - stream.avail_out) != 0) {
            mz_deflateEnd(&stream);
            return -1;
        }
    } while (status != MZ_STREAM_END);

    mz_deflateEnd(&stream);
    return 0;
}

/* --------------------------------------------------------------------------
 * packfile 生成
 * -------------------------------------------------------------------------- */
int lg_pack_generate(const lg_pack_object *objects, size_t count,
                     unsigned char **out_pack, size_t *out_len)
{
    LgBuffer pack;
    unsigned char header[LG_PACK_HEADER_SIZE];
    unsigned char trailer[LG_GIT_HASH_RAW_SIZE];
    lg_sha1_ctx *sha = NULL;
    size_t i;

    if ((objects == NULL && count != 0) || out_pack == NULL || out_len == NULL) {
        return -1;
    }
    *out_pack = NULL;
    *out_len = 0;
    memset(&pack, 0, sizeof(pack));

    /* ---- 头：PACK + 版本 2 + 对象数 ---- */
    memcpy(header, "PACK", 4);
    header[4] = 0; header[5] = 0; header[6] = 0; header[7] = 2;
    header[8]  = (unsigned char)((count >> 24) & 0xFF);
    header[9]  = (unsigned char)((count >> 16) & 0xFF);
    header[10] = (unsigned char)((count >> 8) & 0xFF);
    header[11] = (unsigned char)(count & 0xFF);
    if (LgBufferAppend(&pack, header, sizeof(header)) != 0) {
        return -1;
    }

    /* ---- 对象区 ---- */
    for (i = 0; i < count; i++) {
        unsigned char vh[16];
        size_t vhlen = LgPackWriteObjectHeader(vh, (unsigned int)objects[i].type,
                                               objects[i].content_len);
        if (LgBufferAppend(&pack, vh, vhlen) != 0) {
            free(pack.data);
            return -1;
        }
        if (LgPackDeflateAppend(&pack, objects[i].content, objects[i].content_len) != 0) {
            free(pack.data);
            return -1;
        }
    }

    /* ---- 尾：SHA-1（对前面全部字节）---- */
    if (lg_sha1_init(&sha) != 0) {
        free(pack.data);
        return -1;
    }
    if (lg_sha1_update(sha, pack.data, pack.len) != 0) {
        lg_sha1_abort(sha);
        free(pack.data);
        return -1;
    }
    if (lg_sha1_final(sha, trailer) != 0) {
        free(pack.data);
        return -1;
    }
    if (LgBufferAppend(&pack, trailer, sizeof(trailer)) != 0) {
        free(pack.data);
        return -1;
    }

    *out_pack = pack.data;
    *out_len = pack.len;
    return 0;
}
