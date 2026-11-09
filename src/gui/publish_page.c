/*
 * publish_page.c — Publish 页面实现（主页面）
 *
 * 选择本地目录 → 扫描并显示文件变化（可勾选）→ 填 Commit Message →
 * 一键发布（后台线程执行 lg_publish，进度条用跑马灯样式）。
 *
 * 说明：第一版发布过程为“整体阻塞 + 不确定进度”，取消/精确进度在后续细化。
 */
#include "lightgit/gui/publish_page.h"
#include "lightgit/app/app.h"
#include "lightgit/app/publish_manager.h"
#include "lightgit/file/file_scanner.h"
#include "lightgit/file/file_status.h"
#include "lightgit/file/gitignore.h"
#include "lightgit/git/git_blob.h"
#include "lightgit/storage/manifest.h"
#include "lightgit/i18n.h"

#include <commctrl.h>
#include <shlobj.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LG_PUB_ID_FOLDER   = 2301,
    LG_PUB_ID_BROWSE   = 2302,
    LG_PUB_ID_LIST     = 2303,
    LG_PUB_ID_MSG      = 2304,
    LG_PUB_ID_PUBLISH  = 2305,
    LG_PUB_ID_PROGRESS = 2306,
    LG_PUB_ID_STATUS   = 2307,
    LG_PUB_ID_REPO     = 2308
};

#define LG_MSG_PUB_SCAN    (WM_APP + 30)
#define LG_MSG_PUB_DONE    (WM_APP + 31)

/* 扫描结果中的一项 */
typedef struct LgPubFile {
    char   path[MAX_PATH];
    unsigned long long size;
    lg_file_status status;
} LgPubFile;

typedef struct LgPubScanTask {
    HWND        page;
    lg_publish_result pres;
    LgPubFile  *files;
    size_t      count;
    int         publish_mode;   /* 1 = 发布，0 = 仅扫描 */
    char      **selected;       /* 勾选的文件路径（publish_mode=1 时用） */
    size_t      selected_count;
} LgPubScanTask;

/* 读文本文件到内存（UTF-8） */
static char *LgPubReadText(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    char *buf;
    DWORD read = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (1 << 20)) {
        CloseHandle(h); return NULL;
    }
    buf = (char *)malloc((size_t)sz.QuadPart + 1);
    if (!buf) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &read, NULL)) { free(buf); CloseHandle(h); return NULL; }
    buf[read] = '\0';
    CloseHandle(h);
    return buf;
}

/* 加载 manifest（data/manifest.txt） */
static void LgPubLoadManifest(lg_manifest *m)
{
    wchar_t data_dir[MAX_PATH], path[MAX_PATH];
    char *text;
    memset(m, 0, sizeof(*m));
    if (lg_app_get_data_dir(data_dir, MAX_PATH) != 0) return;
    swprintf(path, MAX_PATH, L"%s\\manifest.txt", data_dir);
    text = LgPubReadText(path);
    if (text) { lg_manifest_parse(text, m); free(text); }
}

static void LgPubSaveManifest(const lg_manifest *m)
{
    wchar_t data_dir[MAX_PATH], path[MAX_PATH];
    char *text = NULL;
    if (lg_manifest_serialize(m, &text) != 0 || text == NULL) return;
    if (lg_app_get_data_dir(data_dir, MAX_PATH) == 0) {
        swprintf(path, MAX_PATH, L"%s\\manifest.txt", data_dir);
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD written = 0;
        if (h != INVALID_HANDLE_VALUE) {
            WriteFile(h, text, (DWORD)strlen(text), &written, NULL);
            CloseHandle(h);
        }
    }
    free(text);
}

/* 后台线程：扫描（或发布） */
static unsigned __stdcall LgPubThread(void *arg)
{
    LgPubScanTask *t = (LgPubScanTask *)arg;
    lg_app *app = lg_app_get();

    if (!t->publish_mode) {
        /* 扫描 + 状态 */
        lg_scan_result scan;
        LgGitignore *gi = NULL;
        lg_manifest manifest;
        wchar_t gitignore_path[MAX_PATH];
        char *gitignore_text = NULL;
        size_t i;

        memset(&scan, 0, sizeof(scan));
        LgPubLoadManifest(&manifest);

        swprintf(gitignore_path, MAX_PATH, L"%s\\.gitignore", app->local_dir);
        gitignore_text = LgPubReadText(gitignore_path);
        if (gitignore_text) gi = lg_gitignore_parse(gitignore_text);

        if (lg_scan_directory(app->local_dir, &scan) == 0) {
            t->files = (LgPubFile *)calloc(scan.count ? scan.count : 1, sizeof(LgPubFile));
            for (i = 0; i < scan.count; i++) {
                LgPubFile *f = &t->files[t->count];
                char blob_sha[LG_GIT_HASH_HEX_SIZE + 1];
                wchar_t full[MAX_PATH * 2], rel[MAX_PATH];
                const char *old;

                if (gi && lg_gitignore_match(gi, scan.files[i].path, 0)) continue;

                MultiByteToWideChar(CP_UTF8, 0, scan.files[i].path, -1, rel, MAX_PATH);
                swprintf(full, MAX_PATH * 2, L"%s\\%s", app->local_dir, rel);

                if (lg_blob_hash_file(full, blob_sha) != 0) continue;

                snprintf(f->path, sizeof(f->path), "%s", scan.files[i].path);
                f->size = scan.files[i].size;
                old = lg_manifest_find(&manifest, f->path);
                f->status = lg_file_status_compute(old, blob_sha);
                t->count++;
            }
        }
        lg_manifest_clear(&manifest);
        lg_gitignore_free(gi);
        free(gitignore_text);
        lg_scan_result_free(&scan);
        PostMessageW(t->page, LG_MSG_PUB_SCAN, 0, (LPARAM)t);
    } else {
        /* 发布 */
        lg_publish_input in;
        lg_manifest manifest;
        wchar_t gitignore_path[MAX_PATH];
        char *gitignore_text = NULL;
        char msg[1024];

        LgPubLoadManifest(&manifest);
        swprintf(gitignore_path, MAX_PATH, L"%s\\.gitignore", app->local_dir);
        gitignore_text = LgPubReadText(gitignore_path);
        GetDlgItemTextA(t->page, LG_PUB_ID_MSG, msg, sizeof(msg));

        memset(&in, 0, sizeof(in));
        in.token = app->token;
        in.owner = app->repo_owner;
        in.repo = app->repo_name;
        in.branch = app->repo_branch[0] ? app->repo_branch : "main";
        in.author_name = app->username[0] ? app->username : "LightGit";
        in.author_email = "lightgit@users.noreply.github.com";
        in.local_dir = app->local_dir;
        in.commit_message = msg;
        in.gitignore_text = gitignore_text;
        in.manifest = &manifest;
        in.selected_paths = (const char *const *)t->selected;
        in.selected_count = t->selected_count;

        lg_publish(&in, &t->pres);
        LgPubSaveManifest(&manifest);

        lg_manifest_clear(&manifest);
        free(gitignore_text);
        PostMessageW(t->page, LG_MSG_PUB_DONE, 0, (LPARAM)t);
    }
    return 0;
}

static void LgPubFillList(HWND page, LgPubScanTask *t)
{
    HWND lv = GetDlgItem(page, LG_PUB_ID_LIST);
    size_t i;
    ListView_DeleteAllItems(lv);
    for (i = 0; i < t->count; i++) {
        LgPubFile *f = &t->files[i];
        LVITEMW item;
        wchar_t path[MAX_PATH], size[64], status[16];
        const wchar_t *st;

        MultiByteToWideChar(CP_UTF8, 0, f->path, -1, path, MAX_PATH);
        swprintf(size, 64, L"%llu", f->size);
        st = (f->status == LG_FILE_ADDED) ? lg_tr(LG_STR_ADDED) :
             (f->status == LG_FILE_MODIFIED) ? lg_tr(LG_STR_MODIFIED) : lg_tr(LG_STR_UNCHANGED);
        wcscpy_s(status, 16, st);

        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.iSubItem = 0;
        item.pszText = path;
        ListView_InsertItem(lv, &item);

        item.iSubItem = 1; item.pszText = status; ListView_SetItem(lv, &item);
        item.iSubItem = 2; item.pszText = size;   ListView_SetItem(lv, &item);

        /* 默认勾选 Added/Modified */
        ListView_SetCheckState(lv, (int)i,
            (f->status == LG_FILE_ADDED || f->status == LG_FILE_MODIFIED) ? TRUE : FALSE);
    }
}

static void LgPubPickFolder(HWND page)
{
    IFileOpenDialog *dlg = NULL;
    IShellItem *item = NULL;
    PWSTR path = NULL;

    if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IFileOpenDialog, (void **)&dlg))) {
        DWORD opts;
        dlg->lpVtbl->GetOptions(dlg, &opts);
        dlg->lpVtbl->SetOptions(dlg, opts | FOS_PICKFOLDERS);
        if (SUCCEEDED(dlg->lpVtbl->Show(dlg, page))) {
            if (SUCCEEDED(dlg->lpVtbl->GetResult(dlg, &item))) {
                item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &path);
                if (path) {
                    lg_app *app = lg_app_get();
                    wcscpy_s(app->local_dir, MAX_PATH, path);
                    SetDlgItemTextW(page, LG_PUB_ID_FOLDER, path);
                    CoTaskMemFree(path);
                }
                item->lpVtbl->Release(item);
            }
        }
        dlg->lpVtbl->Release(dlg);
    }
}

/* 刷新“目标仓库”显示 */
static void LgPubUpdateRepoLabel(HWND page)
{
    lg_app *app = lg_app_get();
    wchar_t buf[512];

    if (app->repo_owner[0] != '\0' && app->repo_name[0] != '\0') {
        swprintf(buf, 512, lg_tr(LG_STR_TARGET_REPO),
                 app->repo_owner, app->repo_name,
                 app->repo_branch[0] ? app->repo_branch : "main");
    } else {
        wcscpy_s(buf, 512, lg_tr(LG_STR_NO_REPO));
    }
    SetDlgItemTextW(page, LG_PUB_ID_REPO, buf);
}

void LgPublishPageRefreshRepo(HWND page)
{
    LgPubUpdateRepoLabel(page);
}

void LgPublishPageRescan(HWND page)
{
    lg_app *app = lg_app_get();
    LgPubScanTask *t;

    SetDlgItemTextW(page, LG_PUB_ID_FOLDER, app->local_dir);
    LgPubUpdateRepoLabel(page);
    t = (LgPubScanTask *)calloc(1, sizeof(*t));
    if (t != NULL) {
        t->page = page;
        _beginthreadex(NULL, 0, LgPubThread, t, 0, NULL);
    }
}

/* 收集 ListView 中勾选的文件路径（相对路径 UTF-8） */
static void LgPubCollectSelection(HWND page, char ***out_paths, size_t *out_count)
{
    HWND lv = GetDlgItem(page, LG_PUB_ID_LIST);
    int n = ListView_GetItemCount(lv);
    size_t cnt = 0, idx = 0, i;
    char **paths;

    for (i = 0; i < (size_t)n; i++) {
        if (ListView_GetCheckState(lv, (int)i)) cnt++;
    }
    paths = (char **)calloc(cnt + 1, sizeof(char *));
    if (paths == NULL) {
        *out_paths = NULL;
        *out_count = 0;
        return;
    }
    for (i = 0; i < (size_t)n; i++) {
        if (ListView_GetCheckState(lv, (int)i)) {
            wchar_t wpath[MAX_PATH];
            char path[MAX_PATH];
            ListView_GetItemText(lv, (int)i, 0, wpath, MAX_PATH);
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH, NULL, NULL);
            paths[idx++] = _strdup(path);
        }
    }
    paths[cnt] = NULL;
    *out_paths = paths;
    *out_count = cnt;
}

static void LgPubFreeSelection(char **paths, size_t count)
{
    size_t i;
    if (paths != NULL) {
        for (i = 0; i < count; i++) {
            free(paths[i]);
        }
        free(paths);
    }
}

/* 文件夹输入框子类：回车触发扫描 */
static WNDPROC g_folder_edit_orig = NULL;
static LRESULT CALLBACK LgFolderEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND page = GetParent(hWnd);
        lg_app *app = lg_app_get();
        GetWindowTextW(hWnd, app->local_dir, MAX_PATH);
        LgPublishPageRescan(page);
        return 0;
    }
    return CallWindowProcW(g_folder_edit_orig, hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK LgPubPageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == LG_PUB_ID_BROWSE) {
            LgPubPickFolder(hWnd);
            LgPublishPageRescan(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == LG_PUB_ID_PUBLISH) {
            LgPubScanTask *t;
            lg_app *app = lg_app_get();

            /* 发布前校验：必须已选择目标仓库 */
            if (app->repo_owner[0] == '\0' || app->repo_name[0] == '\0') {
                SetDlgItemTextW(hWnd, LG_PUB_ID_STATUS, lg_tr(LG_STR_SELECT_REPO_FIRST));
                return 0;
            }

            t = (LgPubScanTask *)calloc(1, sizeof(*t));
            if (t == NULL) return 0;
            t->page = hWnd;
            t->publish_mode = 1;
            LgPubCollectSelection(hWnd, &t->selected, &t->selected_count);
            SetDlgItemTextW(hWnd, LG_PUB_ID_STATUS, lg_tr(LG_STR_PUBLISHING));
            EnableWindow(GetDlgItem(hWnd, LG_PUB_ID_PUBLISH), FALSE);
            SendDlgItemMessageW(hWnd, LG_PUB_ID_PROGRESS, PBM_SETMARQUEE, 1, 50);
            _beginthreadex(NULL, 0, LgPubThread, t, 0, NULL);
            return 0;
        }
        break;

    case LG_MSG_PUB_SCAN: {
        LgPubScanTask *t = (LgPubScanTask *)lParam;
        LgPubFillList(hWnd, t);
        free(t->files);
        free(t);
        return 0;
    }

    case LG_MSG_PUB_DONE: {
        LgPubScanTask *t = (LgPubScanTask *)lParam;
        SendDlgItemMessageW(hWnd, LG_PUB_ID_PROGRESS, PBM_SETMARQUEE, 0, 0);
        EnableWindow(GetDlgItem(hWnd, LG_PUB_ID_PUBLISH), TRUE);
        if (t->pres.success) {
            wchar_t buf[512];
            if (t->pres.no_changes) {
                wcscpy_s(buf, 512, lg_tr(LG_STR_NO_CHANGES));
            } else {
                swprintf(buf, 512, lg_tr(LG_STR_PUBLISHED_FMT),
                         t->pres.commit_oid, t->pres.files_uploaded);
            }
            SetDlgItemTextW(hWnd, LG_PUB_ID_STATUS, buf);
        } else {
            wchar_t buf[600];
            MultiByteToWideChar(CP_UTF8, 0, t->pres.error, -1, buf, 600);
            SetDlgItemTextW(hWnd, LG_PUB_ID_STATUS, buf);
        }
        LgPubFreeSelection(t->selected, t->selected_count);
        free(t);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

HWND LgPublishPageCreate(HWND parent, HINSTANCE hinst)
{
    HWND page, lv;
    LVCOLUMNW col;

    page = CreateWindowExW(0, L"STATIC", L"",
                           WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                           parent, NULL, hinst, NULL);
    SetWindowLongPtrW(page, GWLP_WNDPROC, (LONG_PTR)LgPubPageProc);

    /* 目标仓库显示 */
    CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE, 20, 2, 520, 16,
                    page, (HMENU)(INT_PTR)LG_PUB_ID_REPO, hinst, NULL);

    {
        HWND folder = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        20, 20, 420, 22, page, (HMENU)(INT_PTR)LG_PUB_ID_FOLDER, hinst, NULL);
        g_folder_edit_orig = (WNDPROC)SetWindowLongPtrW(folder, GWLP_WNDPROC, (LONG_PTR)LgFolderEditProc);
    }
    CreateWindowExW(0, L"BUTTON", lg_tr(LG_STR_BROWSE),
                    WS_CHILD | WS_VISIBLE, 450, 20, 90, 24,
                    page, (HMENU)(INT_PTR)LG_PUB_ID_BROWSE, hinst, NULL);

    lv = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                         20, 56, 520, 260, page, (HMENU)(INT_PTR)LG_PUB_ID_LIST, hinst, NULL);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_FILE); col.cx = 300; ListView_InsertColumn(lv, 0, &col);
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_STATUS); col.cx = 100; ListView_InsertColumn(lv, 1, &col);
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_SIZE); col.cx = 100; ListView_InsertColumn(lv, 2, &col);

    CreateWindowExW(0, L"STATIC", lg_tr(LG_STR_COMMIT_MSG),
                    WS_CHILD | WS_VISIBLE, 20, 326, 120, 20, page, NULL, hinst, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    20, 348, 520, 22, page, (HMENU)(INT_PTR)LG_PUB_ID_MSG, hinst, NULL);

    CreateWindowExW(0, L"BUTTON", lg_tr(LG_STR_PUBLISH),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    20, 384, 160, 28, page, (HMENU)(INT_PTR)LG_PUB_ID_PUBLISH, hinst, NULL);

    CreateWindowExW(0, PROGRESS_CLASSW, L"",
                    WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
                    200, 390, 340, 16, page, (HMENU)(INT_PTR)LG_PUB_ID_PROGRESS, hinst, NULL);

    CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE, 20, 424, 520, 20,
                    page, (HMENU)(INT_PTR)LG_PUB_ID_STATUS, hinst, NULL);

    return page;
}
