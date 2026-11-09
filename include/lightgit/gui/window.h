/*
 * window.h — 主窗口
 *
 * 负责创建 LightGit 主窗口、注册窗口类、分发消息。
 * 后续阶段会在主窗口内嵌入 Account / Repository / Publish / Settings 四个页面。
 */
#ifndef LIGHTGIT_GUI_WINDOW_H
#define LIGHTGIT_GUI_WINDOW_H

#include "lightgit/common/defs.h"

/* 主窗口类名（全局唯一，用于 RegisterClass 与 CreateWindow） */
#define LG_WINDOW_CLASS_NAME L"LightGitMainWindow"

/*
 * 创建并显示主窗口。
 *
 * hInstance   当前进程实例句柄（由 WinMain 传入）。
 * nCmdShow    初始显示方式（SW_SHOWNORMAL 等，由 WinMain 传入）。
 *
 * 返回：成功返回 TRUE；失败返回 FALSE（可调用 GetLastError 获取原因）。
 */
BOOL LgWindowCreate(HINSTANCE hInstance, int nCmdShow);

/*
 * 主窗口消息处理函数（WndProc）。
 *
 * 由系统在消息循环中调用，负责处理 WM_CREATE / WM_COMMAND / WM_CLOSE 等。
 */
LRESULT CALLBACK LgWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif /* LIGHTGIT_GUI_WINDOW_H */
