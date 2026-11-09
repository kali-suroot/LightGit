/*
 * window.c — 主窗口实现
 *
 * 使用 Tab 控件承载四个页面：Account / Repository / Publish / Settings。
 * 支持将文件夹拖放到窗口（DragAcceptFiles + WM_DROPFILES）。
 */
#include "lightgit/gui/window.h"
#include "lightgit/gui/account_page.h"
#include "lightgit/gui/repository_page.h"
#include "lightgit/gui/publish_page.h"
#include "lightgit/gui/settings_page.h"
#include "lightgit/app/app.h"
#include "lightgit/i18n.h"

#include <commctrl.h>
#include <shellapi.h>

enum {
    LG_WIN_ID_TAB = 1001
};

/* 各页面句柄（全局，供 WM_SIZE / 拖放使用） */
static HWND g_page_account = NULL;
static HWND g_page_repository = NULL;
static HWND g_page_publish = NULL;
static HWND g_page_settings = NULL;

static BOOL LgRegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = LgWindowProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = LG_WINDOW_CLASS_NAME;
    return RegisterClassExW(&wc) != 0;
}

static void LgCreateTabs(HWND hWnd, HINSTANCE hinst)
{
    HWND tab;
    TCITEMW item;
    RECT rc;
    const wchar_t *names[] = {
        lg_tr(LG_STR_TAB_ACCOUNT),
        lg_tr(LG_STR_TAB_REPOSITORY),
        lg_tr(LG_STR_TAB_PUBLISH),
        lg_tr(LG_STR_TAB_SETTINGS)
    };
    int i;

    tab = CreateWindowExW(0, WC_TABCONTROLW, L"",
                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                          0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)LG_WIN_ID_TAB, hinst, NULL);
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)tab);

    for (i = 0; i < 4; i++) {
        memset(&item, 0, sizeof(item));
        item.mask = TCIF_TEXT;
        item.pszText = (LPWSTR)names[i];
        TabCtrl_InsertItem(tab, i, &item);
    }

    g_page_account    = LgAccountPageCreate(hWnd, hinst);
    g_page_repository = LgRepositoryPageCreate(hWnd, hinst);
    g_page_publish    = LgPublishPageCreate(hWnd, hinst);
    g_page_settings   = LgSettingsPageCreate(hWnd, hinst);

    /* 页面区域位于 Tab 下方 */
    GetClientRect(hWnd, &rc);
    TabCtrl_AdjustRect(tab, FALSE, &rc);
    MoveWindow(g_page_account,    rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    MoveWindow(g_page_repository, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    MoveWindow(g_page_publish,    rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    MoveWindow(g_page_settings,   rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

    /* 默认显示 Publish 页 */
    TabCtrl_SetCurSel(tab, 2);
    ShowWindow(g_page_account, SW_HIDE);
    ShowWindow(g_page_repository, SW_HIDE);
    ShowWindow(g_page_settings, SW_HIDE);
    ShowWindow(g_page_publish, SW_SHOW);
}

static void LgShowCurrentPage(HWND hWnd)
{
    HWND tab = (HWND)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    int sel = TabCtrl_GetCurSel(tab);
    ShowWindow(g_page_account,    sel == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_page_repository, sel == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_page_publish,    sel == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_page_settings,   sel == 3 ? SW_SHOW : SW_HIDE);
    if (sel == 2) {
        LgPublishPageRefreshRepo(g_page_publish);   /* 切到发布页时刷新目标仓库显示 */
    }
}

static void LgLayout(HWND hWnd)
{
    HWND tab = (HWND)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    RECT rc;
    GetClientRect(hWnd, &rc);
    if (tab) {
        MoveWindow(tab, 0, 0, rc.right, rc.bottom, TRUE);
        TabCtrl_AdjustRect(tab, FALSE, &rc);
        MoveWindow(g_page_account,    rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        MoveWindow(g_page_repository, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        MoveWindow(g_page_publish,    rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        MoveWindow(g_page_settings,   rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }
}

static void LgHandleDrop(HWND hWnd, HDROP hDrop)
{
    wchar_t path[MAX_PATH];
    if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0) {
        DWORD attr = GetFileAttributesW(path);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            lg_app *app = lg_app_get();
            wcscpy_s(app->local_dir, MAX_PATH, path);
            HWND tab = (HWND)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
            TabCtrl_SetCurSel(tab, 2);   /* 切到 Publish */
            LgShowCurrentPage(hWnd);
            LgPublishPageRescan(g_page_publish);
        }
    }
    DragFinish(hDrop);
}

BOOL LgWindowCreate(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;

    if (!LgRegisterWindowClass(hInstance)) {
        return FALSE;
    }

    hWnd = CreateWindowExW(0, LG_WINDOW_CLASS_NAME, LG_APP_TITLE,
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 700, 560,
                           NULL, NULL, hInstance, NULL);
    if (hWnd == NULL) {
        return FALSE;
    }

    DragAcceptFiles(hWnd, TRUE);   /* 启用文件夹拖放 */
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK LgWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        LgCreateTabs(hWnd, ((LPCREATESTRUCTW)lParam)->hInstance);
        return 0;

    case WM_SIZE:
        LgLayout(hWnd);
        return 0;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->idFrom == LG_WIN_ID_TAB && nm->code == TCN_SELCHANGE) {
            LgShowCurrentPage(hWnd);
        }
        return 0;
    }

    case WM_DROPFILES:
        LgHandleDrop(hWnd, (HDROP)wParam);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
