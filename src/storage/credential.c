/*
 * credential.c — DPAPI 凭证加解密实现
 */
#include "lightgit/storage/credential.h"

#include <windows.h>
#include <dpapi.h>
#include <stdlib.h>
#include <string.h>

int lg_credential_protect(const char *plaintext,
                          unsigned char **out_blob, size_t *out_len)
{
    DATA_BLOB in, out;
    size_t len;

    if (plaintext == NULL || out_blob == NULL || out_len == NULL) {
        return -1;
    }
    *out_blob = NULL;
    *out_len = 0;

    len = strlen(plaintext);
    in.pbData = (BYTE *)plaintext;
    in.cbData = (DWORD)len;

    /* CRYPTPROTECT_UI_FORBIDDEN：不弹 UI；加密到当前用户 */
    if (!CryptProtectData(&in, L"LightGit PAT", NULL, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return -1;
    }

    *out_blob = (unsigned char *)malloc(out.cbData);
    if (*out_blob == NULL) {
        LocalFree(out.pbData);
        return -1;
    }
    memcpy(*out_blob, out.pbData, out.cbData);
    *out_len = out.cbData;
    LocalFree(out.pbData);
    return 0;
}

int lg_credential_unprotect(const unsigned char *blob, size_t len, char **out_plaintext)
{
    DATA_BLOB in, out;
    LPWSTR desc = NULL;

    if (blob == NULL || len == 0 || out_plaintext == NULL) {
        return -1;
    }
    *out_plaintext = NULL;

    in.pbData = (BYTE *)blob;
    in.cbData = (DWORD)len;

    if (!CryptUnprotectData(&in, &desc, NULL, NULL, NULL,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return -1;
    }

    *out_plaintext = (char *)malloc(out.cbData + 1);
    if (*out_plaintext == NULL) {
        LocalFree(out.pbData);
        if (desc) LocalFree(desc);
        return -1;
    }
    memcpy(*out_plaintext, out.pbData, out.cbData);
    (*out_plaintext)[out.cbData] = '\0';

    LocalFree(out.pbData);
    if (desc) LocalFree(desc);
    return 0;
}
