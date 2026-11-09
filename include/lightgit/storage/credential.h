/*
 * credential.h — 凭证安全存储（Windows DPAPI）
 *
 * 使用 DPAPI（CryptProtectData / CryptUnprotectData）加密/解密 GitHub PAT。
 * 特性：
 *   - 密钥由 Windows 用户账户派生，加密后仅本机当前用户可解；
 *   - 只做加解密，不写注册表；密文由调用方决定存放位置（本项目存 exe 同目录 data/）。
 */
#ifndef LIGHTGIT_STORAGE_CREDENTIAL_H
#define LIGHTGIT_STORAGE_CREDENTIAL_H

#include <stddef.h>

/*
 * 加密明文凭证，返回密文 blob（malloc，调用方 free）。
 * 成功返回 0，失败返回 -1。
 */
int lg_credential_protect(const char *plaintext,
                          unsigned char **out_blob, size_t *out_len);

/*
 * 解密 blob，返回明文（malloc，调用方 free）。
 * 成功返回 0，失败返回 -1。
 */
int lg_credential_unprotect(const unsigned char *blob, size_t len, char **out_plaintext);

#endif /* LIGHTGIT_STORAGE_CREDENTIAL_H */
