/*
 * app.h — 全局应用状态
 *
 * 保存跨页面共享的运行状态：当前登录用户名、PAT（仅内存，落盘走 DPAPI）、
 * 已关联项目列表、当前选中仓库等。GUI 各页面据此读写。
 */
#ifndef LIGHTGIT_APP_APP_H
#define LIGHTGIT_APP_APP_H

#include <windows.h>
#include "lightgit/storage/config.h"
#include "lightgit/storage/pat_store.h"

/* 全局应用状态 */
typedef struct lg_app {
    char     token[256];     /* PAT（仅内存，不落盘明文） */
    char     username[128];  /* 当前登录用户名（空 = 未登录） */
    int      connected;      /* 1 = 已连接 */

    lg_config config;        /* 配置（项目列表等） */

    /* 当前选中仓库 */
    char     repo_owner[128];
    char     repo_name[128];
    char     repo_branch[128];

    /* 当前项目本地目录 */
    wchar_t  local_dir[MAX_PATH];

    lg_pat_store pat_store;   /* PAT 凭证库（多 PAT + 注释） */
} lg_app;

/* 全局单例（GUI 各页面共用） */
lg_app *lg_app_get(void);

/*
 * 获取数据目录（exe 同目录 data\），不存在则创建。
 * 返回 0 成功，失败返回 -1。
 */
int lg_app_get_data_dir(wchar_t *out, size_t cap);

#endif /* LIGHTGIT_APP_APP_H */
