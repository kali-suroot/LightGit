/*
 * test_runner.c — 单元测试汇总入口
 *
 * 依次运行各模块的测试，统计通过/失败，返回进程退出码（0 = 全部通过）。
 */
#include <stdio.h>

/* 各测试模块导出的入口 */
int lg_run_hash_tests(void);
int lg_run_blob_tests(void);
int lg_run_tree_tests(void);
int lg_run_commit_tests(void);
int lg_run_pack_tests(void);
int lg_run_json_tests(void);
int lg_run_gitignore_tests(void);
int lg_run_manifest_tests(void);
int lg_run_scanner_tests(void);

int main(void)
{
    int fails = 0;
    int total = 0;

    struct {
        const char *name;
        int (*run)(void);
    } suites[] = {
        { "hash",   lg_run_hash_tests   },
        { "blob",   lg_run_blob_tests   },
        { "tree",   lg_run_tree_tests   },
        { "commit", lg_run_commit_tests },
        { "pack",   lg_run_pack_tests   },
        { "json",   lg_run_json_tests   },
        { "gitignore", lg_run_gitignore_tests },
        { "manifest", lg_run_manifest_tests },
        { "scanner", lg_run_scanner_tests },
    };

    size_t i;
    for (i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        total++;
        printf("[RUN ] %s\n", suites[i].name);
        if (suites[i].run() == 0) {
            printf("[PASS] %s\n", suites[i].name);
        } else {
            printf("[FAIL] %s\n", suites[i].name);
            fails++;
        }
    }

    printf("\nSummary: %d/%d suites passed\n", total - fails, total);
    return fails == 0 ? 0 : 1;
}
