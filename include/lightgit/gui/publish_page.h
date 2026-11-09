/*
 * publish_page.h — Publish 页面（主页面）
 */
#ifndef LIGHTGIT_GUI_PUBLISH_PAGE_H
#define LIGHTGIT_GUI_PUBLISH_PAGE_H

#include <windows.h>

HWND LgPublishPageCreate(HWND parent, HINSTANCE hinst);

/* 用 app->local_dir 刷新文件夹显示并重新扫描（拖放文件夹后由主窗口调用） */
void LgPublishPageRescan(HWND page);

/* 刷新“目标仓库”显示（切到 Publish 页时由主窗口调用） */
void LgPublishPageRefreshRepo(HWND page);

#endif /* LIGHTGIT_GUI_PUBLISH_PAGE_H */
