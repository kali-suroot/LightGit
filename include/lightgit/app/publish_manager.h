/*
 * publish_manager.h — 发布管理（一键发布）
 *
 * 完成一次完整发布：
 *   扫描本地目录 → 应用 .gitignore → 计算 blob 哈希 → 判定变化（依据 Manifest）
 *   → 读取远程树 → 合并（只替换选中文件，保留远端其余文件，忽略本地删除）
 *   → 构建 Tree/Commit → 生成 Packfile → git-receive-pack 推送 → 更新 Manifest
 *
 * 网络/文件操作均为同步阻塞，调用方应在工作线程中执行（GUI 阶段处理）。
 */
#ifndef LIGHTGIT_APP_PUBLISH_MANAGER_H
#define LIGHTGIT_APP_PUBLISH_MANAGER_H

#include "lightgit/storage/manifest.h"

typedef struct lg_publish_input {
    const char   *token;          /* GitHub PAT */
    const char   *owner;          /* 仓库 owner */
    const char   *repo;           /* 仓库名 */
    const char   *branch;         /* 目标分支（"main" 等） */
    const char   *author_name;    /* 提交作者名（UTF-8） */
    const char   *author_email;   /* 提交作者邮箱（不含尖括号） */
    const wchar_t *local_dir;     /* 本地项目目录（宽字符） */
    const char   *commit_message; /* 提交信息（UTF-8，一行） */
    const char   *gitignore_text; /* .gitignore 内容（可为 NULL） */
    lg_manifest  *manifest;       /* 旧发布状态；成功后更新为新状态 */
    const char *const *selected_paths; /* 显式选中要发布的相对路径（UTF-8）；NULL = 发布全部变化 */
    size_t             selected_count; /* selected_paths 的条目数 */
} lg_publish_input;

typedef struct lg_publish_result {
    int   success;          /* 1 = 成功（或无需发布） */
    int   no_changes;       /* 1 = 没有需要发布的文件 */
    int   files_uploaded;   /* 本次上传的文件数 */
    char  commit_oid[41];   /* 新提交对象 ID（成功时有效） */
    char  error[512];       /* 失败原因 */
} lg_publish_result;

/*
 * 执行发布。成功返回 0（此时看 out->success / no_changes）；
 * 网络或协议级失败返回 -1（此时看 out->error）。
 * 注意：失败时 in->manifest 保持不变。
 */
int lg_publish(const lg_publish_input *in, lg_publish_result *out);

#endif /* LIGHTGIT_APP_PUBLISH_MANAGER_H */
