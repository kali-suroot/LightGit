/*
 * repository_page.c — Repository 页面实现
 *
 * 列出当前账号仓库（分页），点击选中作为发布目标。
 */
#include "lightgit/gui/repository_page.h"
#include "lightgit/app/app.h"
#include "lightgit/github/github_repository.h"
#include "lightgit/i18n.h"

#include <commctrl.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LG_REPO_ID_LIST    = 2201,
    LG_REPO_ID_REFRESH = 2202
};

#define LG_MSG_REPO_LIST (WM_APP + 20)

typedef struct LgRepoTask {
    HWND             page;
    lg_github_repo  *list;
    size_t           count;
    int              ok;
    lg_github_error  err;
} LgRepoTask;

static unsigned __stdcall LgRepoListThread(void *arg)
{
    LgRepoTask *t = (LgRepoTask *)arg;
    if (lg_github_list_repos(lg_app_get()->token, &t->list, &t->count, &t->err) == 0) {
        t->ok = 1;
    } else {
        t->ok = 0;
    }
    PostMessageW(t->page, LG_MSG_REPO_LIST, 0, (LPARAM)t);
    return 0;
}

static void LgRepoFillList(HWND page, lg_github_repo *list, size_t count)
{
    HWND lv = GetDlgItem(page, LG_REPO_ID_LIST);
    size_t i;
    ListView_DeleteAllItems(lv);
    for (i = 0; i < count; i++) {
        LVITEMW item;
        wchar_t name[256], desc[512], branch[128];
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.iSubItem = 0;

        MultiByteToWideChar(CP_UTF8, 0, list[i].name, -1, name, 256);
        MultiByteToWideChar(CP_UTF8, 0, list[i].description ? list[i].description : "",
                            -1, desc, 512);
        MultiByteToWideChar(CP_UTF8, 0, list[i].default_branch ? list[i].default_branch : "",
                            -1, branch, 128);

        item.pszText = name;
        ListView_InsertItem(lv, &item);

        item.mask = LVIF_TEXT;
        item.iSubItem = 1;
        item.pszText = desc;
        ListView_SetItem(lv, &item);

        item.iSubItem = 2;
        item.pszText = branch;
        ListView_SetItem(lv, &item);

        item.iSubItem = 3;
        item.pszText = list[i].is_private ? (LPWSTR)lg_tr(LG_STR_PRIVATE) : (LPWSTR)lg_tr(LG_STR_PUBLIC);
        ListView_SetItem(lv, &item);
    }
}

static LRESULT CALLBACK LgRepoPageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == LG_REPO_ID_REFRESH) {
            LgRepoTask *t = (LgRepoTask *)calloc(1, sizeof(*t));
            if (t == NULL) return 0;
            t->page = hWnd;
            EnableWindow(GetDlgItem(hWnd, LG_REPO_ID_REFRESH), FALSE);
            _beginthreadex(NULL, 0, LgRepoListThread, t, 0, NULL);
            return 0;
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->idFrom == LG_REPO_ID_LIST && nm->code == NM_CLICK) {
            /* 点击选中仓库 */
            NMITEMACTIVATE *ia = (NMITEMACTIVATE *)lParam;
            HWND lv = GetDlgItem(hWnd, LG_REPO_ID_LIST);
            if (ia->iItem >= 0) {
                wchar_t name[256], branch[128];
                lg_app *app = lg_app_get();
                ListView_GetItemText(lv, ia->iItem, 0, name, 256);
                ListView_GetItemText(lv, ia->iItem, 2, branch, 128);
                /* 暂存 owner=当前用户名，name=仓库名 */
                snprintf(app->repo_owner, sizeof(app->repo_owner), "%s", app->username);
                WideCharToMultiByte(CP_UTF8, 0, name, -1, app->repo_name,
                                    sizeof(app->repo_name), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, branch, -1, app->repo_branch,
                                    sizeof(app->repo_branch), NULL, NULL);
            }
        }
        return 0;
    }

    case LG_MSG_REPO_LIST: {
        LgRepoTask *t = (LgRepoTask *)lParam;
        EnableWindow(GetDlgItem(hWnd, LG_REPO_ID_REFRESH), TRUE);
        if (t->ok) {
            LgRepoFillList(hWnd, t->list, t->count);
            lg_github_repo_list_free(t->list, t->count);
        } else {
            /* 简单提示 */
        }
        free(t);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

HWND LgRepositoryPageCreate(HWND parent, HINSTANCE hinst)
{
    HWND page, lv, btn;
    LVCOLUMNW col;

    page = CreateWindowExW(0, L"STATIC", L"",
                           WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                           parent, NULL, hinst, NULL);
    SetWindowLongPtrW(page, GWLP_WNDPROC, (LONG_PTR)LgRepoPageProc);

    btn = CreateWindowExW(0, L"BUTTON", lg_tr(LG_STR_REFRESH),
                          WS_CHILD | WS_VISIBLE, 20, 20, 90, 24,
                          page, (HMENU)(INT_PTR)LG_REPO_ID_REFRESH, hinst, NULL);

    lv = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                         20, 56, 560, 320, page, (HMENU)(INT_PTR)LG_REPO_ID_LIST, hinst, NULL);

    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_NAME); col.cx = 140; ListView_InsertColumn(lv, 0, &col);
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_DESC); col.cx = 240; ListView_InsertColumn(lv, 1, &col);
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_BRANCH); col.cx = 90; ListView_InsertColumn(lv, 2, &col);
    col.pszText = (LPWSTR)lg_tr(LG_STR_COL_VISIBILITY); col.cx = 80; ListView_InsertColumn(lv, 3, &col);

    return page;
}
