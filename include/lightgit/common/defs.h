/*
 * defs.h — LightGit 公共定义
 *
 * 集中放置全项目共用的版本号、常用头文件包含和基础宏，
 * 各模块的头文件统一从这里开始。
 */
#ifndef LIGHTGIT_COMMON_DEFS_H
#define LIGHTGIT_COMMON_DEFS_H

/* 版本号（与 CMakeLists.txt 中的 project(VERSION ...) 保持一致） */
#define LG_VERSION_MAJOR 0
#define LG_VERSION_MINOR 1
#define LG_VERSION_PATCH 0
#define LG_VERSION_STRING "0.1.0"

/* 应用显示名 */
#define LG_APP_NAME        L"LightGit"
#define LG_APP_TITLE       L"LightGit — GitHub File Publisher"

/* 公共头文件（Windows API 精简模式）
 * 说明：WIN32_LEAN_AND_MEAN / NOMINMAX 已由 CMakeLists.txt 全局传入，
 *       此处不再重复定义，避免 C4005 宏重定义警告。 */
#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#endif /* LIGHTGIT_COMMON_DEFS_H */
