/*
 * git_blob.c — Git Blob 对象实现
 */
#include "lightgit/git/git_blob.h"
#include "lightgit/git/git_object.h"
#include "lightgit/common/hex.h"

#include <windows.h>

/* 文件流式读取的分块大小（64 KB） */
#define LG_BLOB_READ_CHUNK 65536

int lg_blob_hash_memory(const void *data, size_t len, char out_hex[LG_GIT_HASH_HEX_SIZE + 1])
{
    if (data == NULL && len != 0) {
        return -1;
    }
    return lg_object_id_compute(LG_OBJECT_BLOB, data, len, out_hex);
}

int lg_blob_hash_file(const wchar_t *path, char out_hex[LG_GIT_HASH_HEX_SIZE + 1])
{
    HANDLE hFile;
    LARGE_INTEGER fileSize;
    char header[64];
    size_t hlen;
    lg_sha1_ctx *ctx = NULL;
    unsigned char raw[LG_GIT_HASH_RAW_SIZE];
    unsigned char buf[LG_BLOB_READ_CHUNK];

    if (path == NULL || out_hex == NULL) {
        return -1;
    }

    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }

    /* 先取文件大小，用于构建对象头 */
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return -1;
    }

    hlen = lg_object_header_build(LG_OBJECT_BLOB, (size_t)fileSize.QuadPart,
                                  header, sizeof(header));

    if (lg_sha1_init(&ctx) != 0) {
        CloseHandle(hFile);
        return -1;
    }
    if (lg_sha1_update(ctx, header, hlen) != 0) {
        lg_sha1_abort(ctx);
        CloseHandle(hFile);
        return -1;
    }

    /* 流式读取文件内容，逐块喂给哈希 */
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(hFile, buf, sizeof(buf), &read, NULL)) {
            lg_sha1_abort(ctx);
            CloseHandle(hFile);
            return -1;
        }
        if (read == 0) {
            break;   /* 读到文件末尾 */
        }
        if (lg_sha1_update(ctx, buf, read) != 0) {
            lg_sha1_abort(ctx);
            CloseHandle(hFile);
            return -1;
        }
    }

    CloseHandle(hFile);

    if (lg_sha1_final(ctx, raw) != 0) {
        return -1;
    }

    lg_hex_encode(raw, LG_GIT_HASH_RAW_SIZE, out_hex);
    return 0;
}
