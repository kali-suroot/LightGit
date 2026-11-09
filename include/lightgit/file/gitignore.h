/*
 * gitignore.h — .gitignore 规则匹配
 *
 * 实现 Git ignore 规则常用子集（第一版）：
 *   - 通配符 *（不跨 /）、**（跨任意层）、?（单字符，不匹配 /）
 *   - 字符类 [...]（含 !/^ 取反、a-z 范围）
 *   - 取反规则 !、以 / 开头（锚定）、以 / 结尾（仅目录）
 *   - 注释行 #、反斜杠转义
 *
 * 已知边界（第一版未实现，见 README）：
 *   - 不含 / 的模式按“任意层级文件名”匹配（与 Git 一致）
 *   - 不处理方括号内 POSIX 类（[:alpha:]）与字符类中的反斜杠转义细节
 */
#ifndef LIGHTGIT_FILE_GITIGNORE_H
#define LIGHTGIT_FILE_GITIGNORE_H

#include <stddef.h>

typedef struct LgGitignore LgGitignore;

/*
 * 解析 .gitignore 文本，返回新对象（调用方 lg_gitignore_free）。
 * 失败返回 NULL。
 */
LgGitignore *lg_gitignore_parse(const char *text);

void lg_gitignore_free(LgGitignore *gi);

/*
 * 判断相对路径 path（以 '/' 分隔，UTF-8）是否被忽略。
 * is_dir 为 1 表示该路径是目录（影响以 / 结尾的规则）。
 * 返回 1 = 忽略，0 = 不忽略。
 */
int lg_gitignore_match(const LgGitignore *gi, const char *path, int is_dir);

#endif /* LIGHTGIT_FILE_GITIGNORE_H */
