/*
 * pat_store.h — GitHub PAT 凭证库
 *
 * 管理多个 PAT：每个 PAT 有注释（note）与（连接成功后记录的）用户名，
 * token 本身用 DPAPI 加密后以 base64 存入 data/credentials/pat_vault.json。
 * 支持添加、删除、切换、解密取用。
 */
#ifndef LIGHTGIT_STORAGE_PAT_STORE_H
#define LIGHTGIT_STORAGE_PAT_STORE_H

#include <stddef.h>

typedef struct lg_pat_entry {
    char          *note;      /* 注释（辅助记忆） */
    char          *username;  /* 该 PAT 对应用户名（可为空） */
    unsigned char *blob;      /* DPAPI 加密后的 token */
    size_t         blob_len;
} lg_pat_entry;

typedef struct lg_pat_store {
    lg_pat_entry *entries;
    size_t        count;
    int           active_index;   /* 当前选中的 PAT 下标，-1 表示无 */
} lg_pat_store;

/* 从 data_dir/credentials/pat_vault.json 加载（不存在则空）。成功返回 0 */
int lg_pat_store_load(const wchar_t *data_dir, lg_pat_store *store);

/* 保存到 data_dir/credentials/pat_vault.json。成功返回 0 */
int lg_pat_store_save(const wchar_t *data_dir, const lg_pat_store *store);

/* 添加/更新（按 note 去重）：token 用 DPAPI 加密。成功返回 0，并置为 active */
int lg_pat_store_add(lg_pat_store *store, const char *note,
                     const char *username, const char *token);

/* 删除指定下标，并修正 active_index。成功返回 0 */
int lg_pat_store_remove(lg_pat_store *store, int index);

/* 解密第 index 个 PAT 的 token 到 out（明文，仅在内存）。成功返回 0 */
int lg_pat_store_get_token(const lg_pat_store *store, int index, char *out, size_t cap);

void lg_pat_store_clear(lg_pat_store *store);

#endif /* LIGHTGIT_STORAGE_PAT_STORE_H */
