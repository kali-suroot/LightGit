/*
 * main.c — LightGit 程序入口
 *
 * 职责：
 *   1. 初始化 Windows 通用控件（后续使用 ListView / ProgressBar 等）。
 *   2. 调用 LgWindowCreate 创建主窗口。
 *   3. 进入消息循环，直到收到 WM_QUIT。
 *
 * 说明：
 *   - 使用 wWinMain + /SUBSYSTEM:WINDOWS，程序为纯 GUI 应用（无控制台）。
 *   - 长时间网络 / 文件任务一律不得在此线程执行，应放到工作线程（后续阶段实现）。
 */
#include "lightgit/common/defs.h"
#include "lightgit/gui/window.h"
#include "lightgit/app/app.h"
#include "lightgit/i18n.h"

/* 通用控件初始化结构（为 ListView 等控件启用视觉样式） */
#include <commctrl.h>
#include <objbase.h>   /* CoInitializeEx / CoUninitialize */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
    /* 初始化通用控件库（必须在使用 ListView/ProgressBar 等控件前调用） */
    INITCOMMONCONTROLSEX icc = {0};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    /* 初始化 COM（IFileOpenDialog 需要） */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    /* 加载配置并设置界面语言（默认中文） */
    {
        lg_app *app = lg_app_get();
        wchar_t data_dir[MAX_PATH];
        if (lg_app_get_data_dir(data_dir, MAX_PATH) == 0) {
            lg_config_load(data_dir, &app->config);
            lg_pat_store_load(data_dir, &app->pat_store);
        }
        lg_i18n_set_lang_name(app->config.language ? app->config.language : "zh");
    }

    /* 创建并显示主窗口；失败则退出 */
    if (!LgWindowCreate(hInstance, nCmdShow)) {
        MessageBoxW(NULL, lg_tr(LG_STR_MSG_FAILED), LG_APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }

    /* 标准消息循环 */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* msg.wParam 为 WM_QUIT 的退出码 */
    CoUninitialize();
    return (int)msg.wParam;
}
