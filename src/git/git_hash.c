/*
 * git_hash.c — 基于 Windows CNG/BCrypt 的 SHA-1 实现
 *
 * 使用 BCrypt 提供的 SHA1 算法（成熟、免自行实现密码学原语）。
 * 算法提供句柄在进程内惰性打开一次并复用（线程安全，各 hash 对象独立）。
 */
#include "lightgit/git/git_hash.h"
#include "lightgit/common/hex.h"

#include <windows.h>   /* 提供 NTSTATUS/ULONG/PVOID/INIT_ONCE 等类型，必须先于 bcrypt.h */
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * 进程级共享的 SHA-1 算法提供句柄
 * -------------------------------------------------------------------------- */
static BCRYPT_ALG_HANDLE g_sha1_alg = NULL;
static INIT_ONCE g_sha1_alg_once = INIT_ONCE_STATIC_INIT;

/* InitOnce 回调：打开 SHA-1 算法提供句柄 */
static BOOL CALLBACK LgSha1OpenAlg(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    NTSTATUS st = BCryptOpenAlgorithmProvider(&g_sha1_alg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    (void)once; (void)param; (void)ctx;
    return BCRYPT_SUCCESS(st);
}

/* --------------------------------------------------------------------------
 * 上下文结构
 * -------------------------------------------------------------------------- */
struct lg_sha1_ctx {
    BCRYPT_HASH_HANDLE handle;  /* BCrypt hash 句柄 */
    void *object;               /* BCrypt hash 对象工作区（大小由系统查询） */
};

int lg_sha1_init(lg_sha1_ctx **out_ctx)
{
    NTSTATUS st;
    DWORD cbObject = 0, cbResult = 0;
    lg_sha1_ctx *ctx;

    if (out_ctx == NULL) {
        return -1;
    }
    *out_ctx = NULL;

    /* 惰性打开算法提供句柄（整个进程仅一次） */
    if (!InitOnceExecuteOnce(&g_sha1_alg_once, LgSha1OpenAlg, NULL, NULL) || g_sha1_alg == NULL) {
        return -1;
    }

    ctx = (lg_sha1_ctx *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return -1;
    }

    /* 查询 hash 对象工作区大小 */
    st = BCryptGetProperty(g_sha1_alg, BCRYPT_OBJECT_LENGTH,
                           (PUCHAR)&cbObject, sizeof(cbObject), &cbResult, 0);
    if (!BCRYPT_SUCCESS(st)) {
        free(ctx);
        return -1;
    }

    ctx->object = malloc(cbObject);
    if (ctx->object == NULL) {
        free(ctx);
        return -1;
    }

    st = BCryptCreateHash(g_sha1_alg, &ctx->handle, (PUCHAR)ctx->object, cbObject, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(st)) {
        free(ctx->object);
        free(ctx);
        return -1;
    }

    *out_ctx = ctx;
    return 0;
}

int lg_sha1_update(lg_sha1_ctx *ctx, const void *data, size_t len)
{
    NTSTATUS st;

    if (ctx == NULL || (data == NULL && len != 0)) {
        return -1;
    }
    if (len == 0) {
        return 0;   /* 空数据直接成功 */
    }

    st = BCryptHashData(ctx->handle, (PUCHAR)data, (ULONG)len, 0);
    return BCRYPT_SUCCESS(st) ? 0 : -1;
}

int lg_sha1_final(lg_sha1_ctx *ctx, unsigned char out_raw[LG_GIT_HASH_RAW_SIZE])
{
    NTSTATUS st;
    int result = -1;

    if (ctx == NULL || out_raw == NULL) {
        return -1;
    }

    st = BCryptFinishHash(ctx->handle, out_raw, LG_GIT_HASH_RAW_SIZE, 0);
    result = BCRYPT_SUCCESS(st) ? 0 : -1;

    /* 无论成功与否，释放上下文资源 */
    BCryptDestroyHash(ctx->handle);
    free(ctx->object);
    free(ctx);
    return result;
}

void lg_sha1_abort(lg_sha1_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    BCryptDestroyHash(ctx->handle);
    free(ctx->object);
    free(ctx);
}

int lg_sha1_hex(const void *data, size_t len, char out_hex[LG_GIT_HASH_HEX_SIZE + 1])
{
    lg_sha1_ctx *ctx = NULL;
    unsigned char raw[LG_GIT_HASH_RAW_SIZE];

    if (out_hex == NULL) {
        return -1;
    }

    if (lg_sha1_init(&ctx) != 0) {
        return -1;
    }
    if (lg_sha1_update(ctx, data, len) != 0) {
        lg_sha1_abort(ctx);
        return -1;
    }
    if (lg_sha1_final(ctx, raw) != 0) {
        return -1;   /* final 已释放上下文 */
    }

    lg_hex_encode(raw, LG_GIT_HASH_RAW_SIZE, out_hex);
    return 0;
}
