/*
 * git_transport.h — Git Smart HTTP 传输（git-receive-pack 推送）
 *
 * 实现两条 HTTP 交互：
 *   1) 发现引用：GET  /{owner}/{repo}.git/info/refs?service=git-receive-pack
 *   2) 推送     ：POST /{owner}/{repo}.git/git-receive-pack
 *
 * 只做 push 侧协议（不实现 fetch/upload-pack 的 want/have 协商）。
 */
#ifndef LIGHTGIT_GIT_GIT_TRANSPORT_H
#define LIGHTGIT_GIT_GIT_TRANSPORT_H

#include <stddef.h>
#include "lightgit/git/git_hash.h"

/* 一条远程引用 */
typedef struct lg_remote_ref {
    char name[256];                            /* 如 "refs/heads/main" */
    char oid[LG_GIT_HASH_HEX_SIZE + 1];        /* 40 字符小写十六进制 */
} lg_remote_ref;

/* 推送结果 */
typedef struct lg_push_result {
    int  success;       /* 1 = 服务端接受，0 = 失败 */
    char error[512];    /* 失败原因（unpack 错误或 ng 原因），成功时为空 */
} lg_push_result;

/*
 * 发现远程引用（GET info/refs）。
 * owner / repo 为 UTF-8 字符串（ASCII 路径）；auth_token 传 NULL 表示公开仓库。
 * 成功返回 0，并通过 *refs / *ref_count 返回引用列表（调用方 free(*refs)）；
 * 空仓库返回 ref_count==0。失败返回 -1，并通过 error/error_cap 写出原因。
 */
int lg_transport_discover(const char *owner, const char *repo,
                          const char *auth_user, const char *auth_token,
                          lg_remote_ref **refs, size_t *ref_count,
                          char *error, size_t error_cap);

/*
 * 推送一个分支引用（POST git-receive-pack）。
 * branch  : 分支名（不含 "refs/heads/" 前缀），如 "main"。
 * old_oid : 期望的远端旧对象 ID；创建新分支时传 40 个 '0'。
 * new_oid : 新提交对象 ID（40 字符十六进制）。
 * packfile : 自包含的 packfile 字节（无 delta，含全部新对象）。
 * result  : 输出服务端结果。
 * 成功返回 0（注意：result->success 才表示服务端是否真正接受推送）。
 */
int lg_transport_push(const char *owner, const char *repo,
                      const char *branch,
                      const char *old_oid, const char *new_oid,
                      const unsigned char *packfile, size_t pack_len,
                      const char *auth_user, const char *auth_token,
                      lg_push_result *result);

#endif /* LIGHTGIT_GIT_GIT_TRANSPORT_H */
