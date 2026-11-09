/*
 * settings_page.c — Settings 页面实现
 *
 * 提供语言切换（中文/English，重启后生效）与关于信息。
 */
#include "lightgit/gui/settings_page.h"
#include "lightgit/app/app.h"
#include "lightgit/storage/config.h"
#include "lightgit/i18n.h"

#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LG_SET_ID_LANG = 2401
};

static LRESULT CALLBACK LgSettingsPageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == LG_SET_ID_LANG && HIWORD(wParam) == CBN_SELCHANGE) {
            lg_app *app = lg_app_get();
            int sel = (int)SendDlgItemMessageW(hWnd, LG_SET_ID_LANG, CB_GETCURSEL, 0, 0);
            /* 0 = 中文, 1 = English */
            free(app->config.language);
            app->config.language = _strdup(sel == 1 ? "en" : "zh");
            {
                wchar_t data_dir[MAX_PATH];
                if (lg_app_get_data_dir(data_dir, MAX_PATH) == 0) {
                    lg_config_save(data_dir, &app->config);
                }
            }
        }
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

HWND LgSettingsPageCreate(HWND parent, HINSTANCE hinst)
{
    HWND page, combo;
    lg_app *app = lg_app_get();
    wchar_t data_dir[MAX_PATH];
    wchar_t buf[1024];

    page = CreateWindowExW(0, L"STATIC", L"",
                           WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                           parent, NULL, hinst, NULL);
    SetWindowLongPtrW(page, GWLP_WNDPROC, (LONG_PTR)LgSettingsPageProc);

    /* 语言选择 */
    CreateWindowExW(0, L"STATIC", lg_tr(LG_STR_LANGUAGE),
                    WS_CHILD | WS_VISIBLE, 24, 24, 160, 20, page, NULL, hinst, NULL);
    combo = CreateWindowExW(0, WC_COMBOBOXW, L"",
                            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                            190, 22, 140, 200, page, (HMENU)(INT_PTR)LG_SET_ID_LANG, hinst, NULL);
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"中文");
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(combo, CB_SETCURSEL, lg_i18n_get_lang() == LG_LANG_EN ? 1 : 0, 0);

    CreateWindowExW(0, L"STATIC", lg_tr(LG_STR_RESTART_HINT),
                    WS_CHILD | WS_VISIBLE, 24, 52, 400, 20, page, NULL, hinst, NULL);

    /* 关于 */
    lg_app_get_data_dir(data_dir, MAX_PATH);
    swprintf(buf, 1024, lg_tr(LG_STR_ABOUT_FMT), data_dir);
    CreateWindowExW(0, L"STATIC", buf,
                    WS_CHILD | WS_VISIBLE, 24, 88, 520, 240,
                    page, NULL, hinst, NULL);
    return page;
}
