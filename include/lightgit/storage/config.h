/*
 * config.h — 应用配置（存 exe 同目录 data/config.json，UTF-8）
 *
 * 保存：当前用户名、默认分支、已关联的本地项目列表。
 * 凭证（PAT）单独用 DPAPI 加密存储（见 credential.h），不写入 config.json。
 */
#ifndef LIGHTGIT_STORAGE_CONFIG_H
#define LIGHTGIT_STORAGE_CONFIG_H

#include <stddef.h>

/* 一个关联项目 */
typedef struct lg_project {
    char   *owner;    /* GitHub owner */
    char   *repo;     /* 仓库名 */
    char   *folder;   /* 本地目录（UTF-8） */
    char   *branch;   /* 目标分支 */
} lg_project;

typedef struct lg_config {
    char        *username;        /* 当前登录用户名（可空） */
    char        *default_branch;  /* 默认分支（默认 "main"） */
    char        *language;        /* 界面语言 "zh"/"en"（默认 "zh"） */
    lg_project  *projects;        /* 项目列表 */
    size_t       project_count;
} lg_config;

/* 从 data_dir/config.json 加载（不存在则得到空配置）。成功返回 0。 */
int lg_config_load(const wchar_t *data_dir, lg_config *cfg);

/* 保存到 data_dir/config.json。成功返回 0。 */
int lg_config_save(const wchar_t *data_dir, const lg_config *cfg);

/* 添加/更新项目（按 owner+repo 去重）。成功返回 0。 */
int lg_config_upsert_project(lg_config *cfg, const char *owner, const char *repo,
                             const char *folder, const char *branch);

void lg_config_clear(lg_config *cfg);

#endif /* LIGHTGIT_STORAGE_CONFIG_H */
