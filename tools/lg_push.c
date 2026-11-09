/*
 * lg_push.c — LightGit 命令行发布工具（开发/集成测试用）
 *
 * 驱动发布管理器（lg_publish）完成一次真实发布，验证 Phase 7 全链路：
 * 扫描 → 合并远程树 → 增量提交 → packfile → push → 更新 Manifest。
 *
 * 用法：lg_push <owner> <repo> <branch> <本地目录> <commit message>
 * 环境变量：LG_TOKEN（必填）；LG_USER（可选）
 *
 * 持久化（仅本工具）：manifest 保存在 exe 同目录 data/manifest.txt。
 */
#include "lightgit/app/publish_manager.h"
#include "lightgit/storage/manifest.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 读取整个文件到内存（UTF-8 文本），失败返回 NULL */
static char *LgReadTextFile(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    char *buf;
    DWORD read = 0;

    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (1 << 20)) {
        CloseHandle(h);
        return NULL;
    }
    buf = (char *)malloc((size_t)sz.QuadPart + 1);
    if (buf == NULL) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &read, NULL)) {
        free(buf); CloseHandle(h); return NULL;
    }
    buf[read] = '\0';
    CloseHandle(h);
    return buf;
}

static int LgWriteTextFile(const wchar_t *path, const char *text)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    WriteFile(h, text, (DWORD)strlen(text), &written, NULL);
    CloseHandle(h);
    return 0;
}

/* 宽字符串 → UTF-8（malloc） */
static char *LgW2U(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *u;
    if (n <= 0) return NULL;
    u = (char *)malloc((size_t)n);
    if (u == NULL) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, u, n, NULL, NULL);
    return u;
}

int wmain(int argc, wchar_t **argv)
{
    char token[512];
    size_t need = 0;
    char *owner, *repo, *branch, *message;
    lg_manifest manifest;
    lg_publish_input in;
    lg_publish_result out;
    wchar_t exe_dir[MAX_PATH];
    wchar_t data_dir[MAX_PATH];
    wchar_t manifest_path[MAX_PATH];
    wchar_t gitignore_path[MAX_PATH];
    char *gitignore_text = NULL;
    char *manifest_text = NULL;
    char *ser = NULL;
    int rc = 1;

    if (argc < 6) {
        wprintf(L"用法: lg_push <owner> <repo> <branch> <本地目录> <commit message>\n");
        return 2;
    }

    if (getenv_s(&need, token, sizeof(token), "LG_TOKEN") != 0 || need == 0) {
        fprintf(stderr, "缺少 LG_TOKEN 环境变量\n");
        return 2;
    }

    owner = LgW2U(argv[1]);
    repo = LgW2U(argv[2]);
    branch = LgW2U(argv[3]);
    message = LgW2U(argv[5]);
    if (!owner || !repo || !branch || !message) return 2;

    /* 定位 exe 同目录 data/ */
    GetModuleFileNameW(NULL, exe_dir, MAX_PATH);
    {
        wchar_t *slash = wcsrchr(exe_dir, L'\\');
        if (slash) *slash = L'\0';
    }
    swprintf(data_dir, MAX_PATH, L"%s\\data", exe_dir);
    CreateDirectoryW(data_dir, NULL);
    swprintf(manifest_path, MAX_PATH, L"%s\\manifest.txt", data_dir);

    /* 读取 .gitignore（可选） */
    swprintf(gitignore_path, MAX_PATH, L"%s\\.gitignore", argv[4]);
    gitignore_text = LgReadTextFile(gitignore_path);

    /* 读取 manifest（可选） */
    manifest_text = LgReadTextFile(manifest_path);
    memset(&manifest, 0, sizeof(manifest));
    if (manifest_text != NULL) {
        lg_manifest_parse(manifest_text, &manifest);
    }

    /* 执行发布 */
    memset(&in, 0, sizeof(in));
    in.token = token;
    in.owner = owner;
    in.repo = repo;
    in.branch = branch;
    in.author_name = "LightGit";
    in.author_email = "lightgit@users.noreply.github.com";
    in.local_dir = argv[4];
    in.commit_message = message;
    in.gitignore_text = gitignore_text;
    in.manifest = &manifest;

    if (lg_publish(&in, &out) != 0) {
        fprintf(stderr, "发布失败: %s\n", out.error);
        goto cleanup;
    }

    if (out.no_changes) {
        printf("No changes to publish.\n");
        rc = 0;
        goto cleanup;
    }

    if (!out.success) {
        fprintf(stderr, "发布被拒绝: %s\n", out.error);
        goto cleanup;
    }

    printf("发布成功: %s -> refs/heads/%s（上传 %d 个文件）\n",
           out.commit_oid, branch, out.files_uploaded);

    /* 保存新 manifest */
    if (lg_manifest_serialize(&manifest, &ser) == 0 && ser != NULL) {
        LgWriteTextFile(manifest_path, ser);
    }
    rc = 0;

cleanup:
    free(ser);
    lg_manifest_clear(&manifest);
    free(manifest_text);
    free(gitignore_text);
    free(owner); free(repo); free(branch); free(message);
    return rc;
}
