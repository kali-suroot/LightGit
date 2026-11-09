/*
 * account_page.c — Account 页面实现（含多 PAT 管理）
 *
 * 提供：
 *   - 已保存 PAT 下拉选择（每个 PAT 带注释/用户名）
 *   - 新增 PAT（可填注释）+ 连接验证
 *   - 删除选中的 PAT
 *   - 自动恢复登录（启动时用上次激活的 PAT 自动连接）
 * Token 一律用 DPAPI 加密存入凭证库（不写注册表、不落明文）。
 */
#include "lightgit/gui/account_page.h"
#include "lightgit/app/app.h"
#include "lightgit/github/github_auth.h"
#include "lightgit/storage/pat_store.h"
#include "lightgit/i18n.h"

#include <commctrl.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LG_ACC_ID_COMBO   = 2101,
    LG_ACC_ID_DELETE  = 2102,
    LG_ACC_ID_NOTE    = 2103,
    LG_ACC_ID_TOKEN   = 2104,
    LG_ACC_ID_CONNECT = 2105,
    LG_ACC_ID_STATUS  = 2106,
    LG_ACC_ID_LOGOUT  = 2107,
    LG_ACC_ID_USER    = 2108
};

#define LG_MSG_ACC_CONNECT (WM_APP + 10)

/* 连接任务（工作线程 → 页面） */
typedef struct LgAccConnectTask {
    HWND             page;
    char             token[256];
    char             note[128];
    int              from_store;   /* 1 = 来自已保存 PAT（成功后不重复保存） */
    int              ok;
    char             login[128];
    lg_github_error  err;
} LgAccConnectTask;

static void LgAccSaveStore(void)
{
    wchar_t data_dir[MAX_PATH];
    lg_app *app = lg_app_get();
    if (lg_app_get_data_dir(data_dir, MAX_PATH) == 0) {
        lg_pat_store_save(data_dir, &app->pat_store);
    }
}

static void LgAccRefreshCombo(HWND page)
{
    HWND combo = GetDlgItem(page, LG_ACC_ID_COMBO);
    lg_app *app = lg_app_get();
    size_t i;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < app->pat_store.count; i++) {
        lg_pat_entry *e = &app->pat_store.entries[i];
        wchar_t item[512];
        wchar_t note_w[256];
        /* note 是 UTF-8，必须显式转宽字符；%hs 会按 ANSI/GBK 解释导致中文乱码 */
        MultiByteToWideChar(CP_UTF8, 0, e->note, -1, note_w, 256);
        if (e->username != NULL && e->username[0] != '\0') {
            wchar_t user_w[128];
            MultiByteToWideChar(CP_UTF8, 0, e->username, -1, user_w, 128);
            swprintf(item, 512, L"%s（%s）", note_w, user_w);
        } else {
            swprintf(item, 512, L"%s", note_w);
        }
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)item);
    }

    if (app->pat_store.count == 0) {
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)lg_tr(LG_STR_PAT_NONE));
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    } else {
        int idx = app->pat_store.active_index;
        if (idx < 0 || (size_t)idx >= app->pat_store.count) idx = 0;
        SendMessageW(combo, CB_SETCURSEL, (WPARAM)idx, 0);
    }
}

static void LgAccRefreshUI(HWND page)
{
    lg_app *app = lg_app_get();
    wchar_t buf[512];

    if (app->connected) {
        swprintf(buf, 512, lg_tr(LG_STR_CONNECTED_FMT), app->username);
    } else {
        wcscpy_s(buf, 512, lg_tr(LG_STR_NOT_CONNECTED));
    }
    SetDlgItemTextW(page, LG_ACC_ID_STATUS, buf);
    LgAccRefreshCombo(page);
}

static unsigned __stdcall LgAccConnectThread(void *arg)
{
    LgAccConnectTask *t = (LgAccConnectTask *)arg;
    lg_github_user user;

    if (lg_github_get_current_user(t->token, &user, &t->err) == 0) {
        t->ok = 1;
        snprintf(t->login, sizeof(t->login), "%s", user.login);
        lg_github_user_free(&user);
    } else {
        t->ok = 0;
    }
    PostMessageW(t->page, LG_MSG_ACC_CONNECT, 0, (LPARAM)t);
    return 0;
}

static void LgAccStartConnect(HWND page, const char *token, const char *note, int from_store)
{
    LgAccConnectTask *t = (LgAccConnectTask *)calloc(1, sizeof(*t));
    if (t == NULL) return;
    t->page = page;
    snprintf(t->token, sizeof(t->token), "%s", token);
    if (note != NULL) snprintf(t->note, sizeof(t->note), "%s", note);
    t->from_store = from_store;
    SetDlgItemTextW(page, LG_ACC_ID_STATUS, lg_tr(LG_STR_VERIFYING));
    EnableWindow(GetDlgItem(page, LG_ACC_ID_CONNECT), FALSE);
    _beginthreadex(NULL, 0, LgAccConnectThread, t, 0, NULL);
}

/* 根据错误码生成友好提示（宽） */
static void LgAccErrorText(lg_github_error *err, wchar_t *buf, size_t cap)
{
    if (err->code == LG_GITHUB_ERR_AUTH) {
        wcscpy_s(buf, cap, lg_tr(LG_STR_PAT_INVALID));      /* 401：PAT 无效或已过期 */
    } else if (err->code == LG_GITHUB_ERR_FORBIDDEN) {
        wcscpy_s(buf, cap, lg_tr(LG_STR_PAT_FORBIDDEN));    /* 403：权限不足 */
    } else if (err->message[0] != '\0') {
        MultiByteToWideChar(CP_UTF8, 0, err->message, -1, buf, (int)cap);
    } else {
        wcscpy_s(buf, cap, lg_tr(LG_STR_VERIFY_FAILED));
    }
}

static LRESULT CALLBACK LgAccPageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == LG_ACC_ID_CONNECT) {
            char token[256];
            char note[128];
            wchar_t wtoken[256];
            wchar_t wnote[128];

            GetDlgItemTextW(hWnd, LG_ACC_ID_TOKEN, wtoken, 256);
            GetDlgItemTextW(hWnd, LG_ACC_ID_NOTE, wnote, 128);
            {
                char utf8[256], *src, *dst;
                WideCharToMultiByte(CP_UTF8, 0, wtoken, -1, utf8, sizeof(utf8), NULL, NULL);
                for (src = utf8, dst = utf8; *src; src++) {
                    if (*src != ' ' && *src != '\t' && *src != '\r' && *src != '\n') *dst++ = *src;
                }
                *dst = '\0';
                snprintf(token, sizeof(token), "%s", utf8);
                WideCharToMultiByte(CP_UTF8, 0, wnote, -1, note, sizeof(note), NULL, NULL);
            }

            if (token[0] == '\0') {
                SetDlgItemTextW(hWnd, LG_ACC_ID_STATUS, lg_tr(LG_STR_ENTER_TOKEN));
                return 0;
            }
            LgAccStartConnect(hWnd, token, note, 0);
            return 0;
        }

        if (LOWORD(wParam) == LG_ACC_ID_DELETE) {
            lg_app *app = lg_app_get();
            HWND combo = GetDlgItem(hWnd, LG_ACC_ID_COMBO);
            int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
            if (app->pat_store.count > 0 && sel >= 0 && (size_t)sel < app->pat_store.count) {
                lg_pat_store_remove(&app->pat_store, sel);
                LgAccSaveStore();
                app->connected = 0;
                app->token[0] = '\0';
                app->username[0] = '\0';
                LgAccRefreshUI(hWnd);
            }
            return 0;
        }

        if (LOWORD(wParam) == LG_ACC_ID_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            lg_app *app = lg_app_get();
            HWND combo = GetDlgItem(hWnd, LG_ACC_ID_COMBO);
            int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
            if (app->pat_store.count > 0 && sel >= 0 && (size_t)sel < app->pat_store.count) {
                char token[256];
                app->pat_store.active_index = sel;
                LgAccSaveStore();
                if (lg_pat_store_get_token(&app->pat_store, sel, token, sizeof(token)) == 0) {
                    LgAccStartConnect(hWnd, token, NULL, 1);
                }
            }
            return 0;
        }

        if (LOWORD(wParam) == LG_ACC_ID_LOGOUT) {
            lg_app *app = lg_app_get();
            app->connected = 0;
            app->token[0] = '\0';
            app->username[0] = '\0';
            LgAccRefreshUI(hWnd);
            return 0;
        }
        break;

    case LG_MSG_ACC_CONNECT: {
        LgAccConnectTask *t = (LgAccConnectTask *)lParam;
        lg_app *app = lg_app_get();
        EnableWindow(GetDlgItem(hWnd, LG_ACC_ID_CONNECT), TRUE);

        if (t->ok) {
            snprintf(app->token, sizeof(app->token), "%s", t->token);
            snprintf(app->username, sizeof(app->username), "%s", t->login);
            app->connected = 1;

            /* 新输入的 PAT：成功后存入凭证库（note 为空则用用户名） */
            if (!t->from_store) {
                const char *note = t->note[0] ? t->note : t->login;
                lg_pat_store_add(&app->pat_store, note, t->login, t->token);
                LgAccSaveStore();
            }
        } else {
            wchar_t buf[600];
            LgAccErrorText(&t->err, buf, 600);
            SetDlgItemTextW(hWnd, LG_ACC_ID_STATUS, buf);
        }
        LgAccRefreshUI(hWnd);
        free(t);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

HWND LgAccountPageCreate(HWND parent, HINSTANCE hinst)
{
    HWND page, ctl;
    lg_app *app = lg_app_get();
    int y = 24;

    page = CreateWindowExW(0, L"STATIC", L"",
                           WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                           parent, NULL, hinst, NULL);
    SetWindowLongPtrW(page, GWLP_WNDPROC, (LONG_PTR)LgAccPageProc);

    /* 已保存 PAT 下拉 + 删除 */
    CreateWindowExW(0, L"STATIC", lg_tr(LG_STR_PAT_LIST),
                    WS_CHILD | WS_VISIBLE, 20, y, 200, 20, page, NULL, hinst, NULL);
    ctl = CreateWindowExW(0, WC_COMBOBOXW, L"",
                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                          20, y + 20, 400, 200, page, (HMENU)(INT_PTR)LG_ACC_ID_COMBO, hinst, NULL);
    CreateWindowExW(0, L"BUTTON", lg_tr(LG_STR_DELETE),
                    WS_CHILD | WS_VISIBLE, 430, y + 20, 90, 24,
                    page, (HMENU)(INT_PTR)LG_ACC_ID_DELETE, hinst, NULL);

    /* 注释 */
    CreateWindowExW(0, L"STATIC", lg_tr(LG_STR_PAT_NOTE),
                    WS_CHILD | WS_VISIBLE, 20, y + 56, 200, 20, page, NULL, hinst, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    20, y + 76, 500, 22, page, (HMENU)(INT_PTR)LG_ACC_ID_NOTE, hinst, NULL);

    /* 新令牌 */
    CreateWindowExW(0, L"STATIC", lg_tr(LG_STR_NEW_TOKEN),
                    WS_CHILD | WS_VISIBLE, 20, y + 106, 200, 20, page, NULL, hinst, NULL);
    ctl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL,
                          20, y + 126, 500, 40, page, (HMENU)(INT_PTR)LG_ACC_ID_TOKEN, hinst, NULL);
    SendMessageW(ctl, EM_SETLIMITTEXT, 255, 0);

    CreateWindowExW(0, L"BUTTON", lg_tr(LG_STR_CONNECT),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    20, y + 176, 120, 26, page, (HMENU)(INT_PTR)LG_ACC_ID_CONNECT, hinst, NULL);

    CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE, 160, y + 180, 400, 20,
                    page, (HMENU)(INT_PTR)LG_ACC_ID_STATUS, hinst, NULL);

    CreateWindowExW(0, L"BUTTON", lg_tr(LG_STR_LOGOUT),
                    WS_CHILD | WS_VISIBLE, 20, y + 216, 90, 24,
                    page, (HMENU)(INT_PTR)LG_ACC_ID_LOGOUT, hinst, NULL);

    LgAccRefreshUI(page);

    /* 自动恢复登录：用上次激活的 PAT 自动连接（项目书 §3.1） */
    if (app->pat_store.count > 0) {
        int idx = app->pat_store.active_index;
        char saved[256];
        if (idx < 0 || (size_t)idx >= app->pat_store.count) idx = 0;
        if (lg_pat_store_get_token(&app->pat_store, idx, saved, sizeof(saved)) == 0) {
            LgAccStartConnect(page, saved, NULL, 1);
        }
    }
    return page;
}
