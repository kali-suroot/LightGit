/*
 * test_framework.h — LightGit 极简单元测试框架
 *
 * 约定：每个测试文件导出一个函数 int lg_run_xxx_tests(void)，
 *       内部使用 LG_TEST_ASSERT* 断言，返回 0 表示全部通过、非 0 表示失败。
 *       tests/test_runner.c 汇总调用所有 lg_run_* 函数并统计结果。
 *
 * 仅依赖 C 标准库，无任何外部依赖。
 */
#ifndef LIGHTGIT_TEST_FRAMEWORK_H
#define LIGHTGIT_TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

/* 断言失败：打印位置与表达式，并立即从当前测试函数返回 1 */
#define LG_TEST_ASSERT(cond) do {                                            \
    if (!(cond)) {                                                           \
        fprintf(stderr, "    ASSERT FAILED at %s:%d: %s\n",                  \
                __FILE__, __LINE__, #cond);                                  \
        return 1;                                                            \
    }                                                                        \
} while (0)

/* 断言两个字符串相等（NULL 安全） */
#define LG_TEST_ASSERT_STREQ(actual, expect) do {                            \
    const char *_a = (actual);                                               \
    const char *_e = (expect);                                               \
    if (_a == NULL || _e == NULL || strcmp(_a, _e) != 0) {                   \
        fprintf(stderr, "    ASSERT FAILED at %s:%d\n"                       \
                        "      actual: %s\n      expect: %s\n",              \
                __FILE__, __LINE__, _a ? _a : "(null)", _e ? _e : "(null)"); \
        return 1;                                                            \
    }                                                                        \
} while (0)

/* 断言两个内存块相等 */
#define LG_TEST_ASSERT_MEMEQ(actual, expect, n) do {                         \
    if (memcmp((actual), (expect), (n)) != 0) {                              \
        fprintf(stderr, "    ASSERT FAILED at %s:%d: memory differs (%zu bytes)\n", \
                __FILE__, __LINE__, (size_t)(n));                            \
        return 1;                                                            \
    }                                                                        \
} while (0)

#endif /* LIGHTGIT_TEST_FRAMEWORK_H */
