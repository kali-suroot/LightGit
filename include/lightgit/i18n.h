/*
 * i18n.h — 界面国际化（中英双语）
 *
 * 轻量字符串表：按语言（默认中文）返回对应 UI 文案。
 * 语言切换写入 config.json，重启后生效（v1 简化策略）。
 */
#ifndef LIGHTGIT_I18N_H
#define LIGHTGIT_I18N_H

#include <wchar.h>

typedef enum lg_lang {
    LG_LANG_ZH = 0,   /* 中文（默认） */
    LG_LANG_EN = 1    /* English */
} lg_lang;

/* 字符串键 */
enum {
    LG_STR_TAB_ACCOUNT = 0,
    LG_STR_TAB_REPOSITORY,
    LG_STR_TAB_PUBLISH,
    LG_STR_TAB_SETTINGS,
    LG_STR_TOKEN_LABEL,
    LG_STR_CONNECT,
    LG_STR_LOGOUT,
    LG_STR_NOT_CONNECTED,
    LG_STR_CONNECTED_FMT,
    LG_STR_ENTER_TOKEN,
    LG_STR_VERIFYING,
    LG_STR_VERIFY_FAILED,
    LG_STR_REFRESH,
    LG_STR_COL_NAME,
    LG_STR_COL_DESC,
    LG_STR_COL_BRANCH,
    LG_STR_COL_VISIBILITY,
    LG_STR_PRIVATE,
    LG_STR_PUBLIC,
    LG_STR_BROWSE,
    LG_STR_COMMIT_MSG,
    LG_STR_PUBLISH,
    LG_STR_PUBLISHING,
    LG_STR_NO_CHANGES,
    LG_STR_PUBLISHED_FMT,
    LG_STR_COL_FILE,
    LG_STR_COL_STATUS,
    LG_STR_COL_SIZE,
    LG_STR_ADDED,
    LG_STR_MODIFIED,
    LG_STR_UNCHANGED,
    LG_STR_LANGUAGE,
    LG_STR_RESTART_HINT,
    LG_STR_ABOUT_FMT,
    LG_STR_MSG_FAILED,
    LG_STR_TARGET_REPO,
    LG_STR_NO_REPO,
    LG_STR_SELECT_REPO_FIRST,
    LG_STR_PAT_LIST,
    LG_STR_PAT_NOTE,
    LG_STR_NEW_TOKEN,
    LG_STR_DELETE,
    LG_STR_PAT_INVALID,
    LG_STR_PAT_FORBIDDEN,
    LG_STR_PAT_NONE,
    LG_STR_COUNT
};

/* 设置当前语言 */
void lg_i18n_set_lang(lg_lang lang);

/* 获取当前语言 */
lg_lang lg_i18n_get_lang(void);

/* 由语言名设置（"zh"/"en"，未知默认中文） */
void lg_i18n_set_lang_name(const char *name);

/* 返回指定键的当前语言文案（宽字符串，格式串用 %hs 表示窄字符串、%d 整数） */
const wchar_t *lg_tr(int key);

#endif /* LIGHTGIT_I18N_H */
