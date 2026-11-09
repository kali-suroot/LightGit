/*
 * file_scanner.h — 本地目录递归扫描
 *
 * 扫描目录下所有文件，返回相对路径（UTF-8，'/' 分隔）与文件大小。
 * 内置排除 .git 目录（以及 LightGit 内部目录，后续阶段补充）。
 */
#ifndef LIGHTGIT_FILE_FILE_SCANNER_H
#define LIGHTGIT_FILE_FILE_SCANNER_H

#include <stddef.h>

typedef struct lg_scanned_file {
    char               *path;   /* 相对路径（UTF-8，'/' 分隔） */
    unsigned long long  size;   /* 文件字节数 */
} lg_scanned_file;

typedef struct lg_scan_result {
    lg_scanned_file *files;
    size_t           count;
} lg_scan_result;

/*
 * 递归扫描目录（root_dir 为宽字符绝对路径）。
 * 成功返回 0 并填充 out（调用方 lg_scan_result_free）；失败返回 -1。
 */
int lg_scan_directory(const wchar_t *root_dir, lg_scan_result *out);

void lg_scan_result_free(lg_scan_result *r);

#endif /* LIGHTGIT_FILE_FILE_SCANNER_H */
