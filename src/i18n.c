/*
 * i18n.c — 中英双语字符串表
 */
#include "lightgit/i18n.h"

#include <string.h>

static lg_lang g_lang = LG_LANG_ZH;

/* 中文 */
static const wchar_t *g_zh[LG_STR_COUNT] = {
    /* 0  */ L"账户",
    /* 1  */ L"仓库",
    /* 2  */ L"发布",
    /* 3  */ L"设置",
    /* 4  */ L"访问令牌 (PAT)：",
    /* 5  */ L"连接",
    /* 6  */ L"退出登录",
    /* 7  */ L"未连接",
    /* 8  */ L"已连接：%hs",
    /* 9  */ L"请输入 Token",
    /* 10 */ L"正在验证...",
    /* 11 */ L"验证失败，请检查 Token",
    /* 12 */ L"刷新",
    /* 13 */ L"名称",
    /* 14 */ L"描述",
    /* 15 */ L"分支",
    /* 16 */ L"可见性",
    /* 17 */ L"私有",
    /* 18 */ L"公开",
    /* 19 */ L"浏览...",
    /* 20 */ L"提交说明：",
    /* 21 */ L"发布到 GitHub",
    /* 22 */ L"正在发布...",
    /* 23 */ L"没有需要发布的内容。",
    /* 24 */ L"发布成功：%hs（%d 个文件）",
    /* 25 */ L"文件",
    /* 26 */ L"状态",
    /* 27 */ L"大小",
    /* 28 */ L"新增",
    /* 29 */ L"修改",
    /* 30 */ L"未变",
    /* 31 */ L"语言 (Language)：",
    /* 32 */ L"（语言切换后重启生效）",
    /* 33 */ L"LightGit 0.1.0\r\n\r\n轻量级 GitHub 文件发布客户端（C + Win32）。\r\n\r\n数据目录（exe 同目录）：\r\n%s\r\n\r\nToken 使用 Windows DPAPI 加密存储，不写注册表。",
    /* 34 */ L"创建主窗口失败。",
    /* 35 */ L"目标仓库：%hs/%hs（分支 %hs）",
    /* 36 */ L"未选择仓库",
    /* 37 */ L"请先在“仓库”页选择目标仓库",
    /* 38 */ L"已保存的 PAT：",
    /* 39 */ L"注释（可选）：",
    /* 40 */ L"新令牌 (PAT)：",
    /* 41 */ L"删除",
    /* 42 */ L"PAT 无效或已过期",
    /* 43 */ L"PAT 权限不足",
    /* 44 */ L"（无，输入新 PAT）",
};

/* English */
static const wchar_t *g_en[LG_STR_COUNT] = {
    /* 0  */ L"Account",
    /* 1  */ L"Repository",
    /* 2  */ L"Publish",
    /* 3  */ L"Settings",
    /* 4  */ L"Personal Access Token:",
    /* 5  */ L"Connect",
    /* 6  */ L"Logout",
    /* 7  */ L"Not connected",
    /* 8  */ L"Connected: %hs",
    /* 9  */ L"Please enter Token",
    /* 10 */ L"Verifying...",
    /* 11 */ L"Verification failed. Check your Token.",
    /* 12 */ L"Refresh",
    /* 13 */ L"Name",
    /* 14 */ L"Description",
    /* 15 */ L"Branch",
    /* 16 */ L"Visibility",
    /* 17 */ L"private",
    /* 18 */ L"public",
    /* 19 */ L"Browse...",
    /* 20 */ L"Commit Message:",
    /* 21 */ L"Publish to GitHub",
    /* 22 */ L"Publishing...",
    /* 23 */ L"No changes to publish.",
    /* 24 */ L"Published: %hs (%d files)",
    /* 25 */ L"File",
    /* 26 */ L"Status",
    /* 27 */ L"Size",
    /* 28 */ L"Added",
    /* 29 */ L"Modified",
    /* 30 */ L"Unchanged",
    /* 31 */ L"Language:",
    /* 32 */ L"(Language change takes effect after restart)",
    /* 33 */ L"LightGit 0.1.0\r\n\r\nA lightweight GitHub file publisher (C + Win32).\r\n\r\nData directory (next to exe):\r\n%s\r\n\r\nToken is encrypted with Windows DPAPI. No registry writes.",
    /* 34 */ L"Failed to create the main window.",
    /* 35 */ L"Target repo: %hs/%hs (branch %hs)",
    /* 36 */ L"No repository selected",
    /* 37 */ L"Please select a target repository in the Repository page first",
    /* 38 */ L"Saved PATs:",
    /* 39 */ L"Note (optional):",
    /* 40 */ L"New token (PAT):",
    /* 41 */ L"Delete",
    /* 42 */ L"PAT invalid or expired",
    /* 43 */ L"PAT has insufficient permissions",
    /* 44 */ L"(none, enter a new PAT)",
};

void lg_i18n_set_lang(lg_lang lang)
{
    g_lang = lang;
}

lg_lang lg_i18n_get_lang(void)
{
    return g_lang;
}

void lg_i18n_set_lang_name(const char *name)
{
    if (name != NULL && strcmp(name, "en") == 0) {
        g_lang = LG_LANG_EN;
    } else {
        g_lang = LG_LANG_ZH;
    }
}

const wchar_t *lg_tr(int key)
{
    const wchar_t **tbl;

    if (key < 0 || key >= LG_STR_COUNT) {
        return L"";
    }
    tbl = (g_lang == LG_LANG_EN) ? g_en : g_zh;
    return tbl[key];
}
