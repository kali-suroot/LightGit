/*
 * gitignore.c — .gitignore 规则匹配实现
 */
#include "lightgit/file/gitignore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct LgGitignoreRule {
    char *pattern;   /* 去除转义、不含 ! 前缀与尾部 / 的模式 */
    int   negated;   /* 1 = ! 取反 */
    int   dir_only;  /* 1 = 以 / 结尾，仅匹配目录 */
    int   anchored;  /* 1 = 含 /（锚定到 .gitignore 目录） */
} LgGitignoreRule;

struct LgGitignore {
    LgGitignoreRule *rules;
    size_t           count;
};

/* --------------------------------------------------------------------------
 * glob 匹配：支持 * ? ** [...] 与 \ 转义
 * -------------------------------------------------------------------------- */
static int LgGlobMatch(const char *p, const char *t);

/* 字符类匹配：p 指向 '['，t 为当前字符；返回是否匹配并推进 p */
static int LgMatchClass(const char **pp, char c)
{
    const char *p = *pp + 1;   /* 跳过 '[' */
    int negate = 0, matched = 0, first = 1;

    if (*p == '!' || *p == '^') {
        negate = 1;
        p++;
    }

    while (*p && (*p != ']' || first)) {
        first = 0;
        if (p[1] == '-' && p[2] && p[2] != ']') {
            /* 范围 a-z */
            if (c >= p[0] && c <= p[2]) {
                matched = 1;
            }
            p += 3;
        } else {
            if (c == *p) {
                matched = 1;
            }
            p++;
        }
    }

    if (*p == ']') {
        p++;
    }
    *pp = p;
    return negate ? !matched : matched;
}

static int LgGlobMatch(const char *p, const char *t)
{
    while (*p) {
        if (*p == '*') {
            /* 判断是否 **（跨路径） */
            if (p[1] == '*') {
                p += 2;
                while (*p == '*') {
                    p++;
                }
                if (*p == '/') {
                    /* 双星加斜杠：匹配零个或多个目录层 */
                    p++;
                    for (const char *q = t; ; ) {
                        if (LgGlobMatch(p, q)) {
                            return 1;
                        }
                        const char *slash = strchr(q, '/');
                        if (slash == NULL) {
                            return 0;
                        }
                        q = slash + 1;
                    }
                }
                if (*p == '\0') {
                    return 1;   /* 尾部 ** 匹配一切 */
                }
                /* ** 匹配任意序列（含 /） */
                for (const char *q = t; ; q++) {
                    if (LgGlobMatch(p, q)) {
                        return 1;
                    }
                    if (*q == '\0') {
                        return 0;
                    }
                }
            } else {
                p++;
                if (*p == '\0') {
                    return 1;
                }
                /* * 匹配任意字符但不跨 / */
                while (*t && *t != '/') {
                    if (LgGlobMatch(p, t)) {
                        return 1;
                    }
                    t++;
                }
                return LgGlobMatch(p, t);
            }
        } else if (*p == '?') {
            if (*t == '\0' || *t == '/') {
                return 0;
            }
            p++;
            t++;
        } else if (*p == '[') {
            if (!LgMatchClass(&p, *t)) {
                return 0;
            }
            t++;
        } else if (*p == '\\') {
            p++;
            if (*t != *p) {
                return 0;
            }
            p++;
            t++;
        } else {
            if (*t != *p) {
                return 0;
            }
            p++;
            t++;
        }
    }
    return *t == '\0';
}

/* --------------------------------------------------------------------------
 * 解析
 * -------------------------------------------------------------------------- */
LgGitignore *lg_gitignore_parse(const char *text)
{
    LgGitignore *gi;
    const char *line = text;

    gi = (LgGitignore *)calloc(1, sizeof(*gi));
    if (gi == NULL) {
        return NULL;
    }

    while (line != NULL && *line) {
        const char *eol = strchr(line, '\n');
        size_t linelen = eol ? (size_t)(eol - line) : strlen(line);
        const char *end = line + linelen;
        const char *s = line;
        LgGitignoreRule rule;
        char *pat;
        size_t patlen, i;

        /* 去掉行尾 \r 与空白（忽略末尾未转义空格） */
        while (end > s && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        /* 空行与注释 */
        if (end == s || *s == '#') {
            goto next;
        }

        memset(&rule, 0, sizeof(rule));

        /* 取反 */
        if (*s == '!') {
            rule.negated = 1;
            s++;
        }

        /* 仅目录（以 / 结尾） */
        if (end > s && end[-1] == '/') {
            rule.dir_only = 1;
            end--;
        }

        /* 锚定判断：模式含 /（开头或中间）即锚定到 .gitignore 所在目录；
           否则可匹配任意层级。注意尾部 / 已在上面剥离，故剩余的 / 均表示锚定。 */
        if (memchr(s, '/', (size_t)(end - s)) != NULL) {
            rule.anchored = 1;
        }

        /* 拷贝模式并去除转义反斜杠（\X → X） */
        patlen = (size_t)(end - s);
        pat = (char *)malloc(patlen + 1);
        if (pat == NULL) {
            lg_gitignore_free(gi);
            return NULL;
        }
        {
            size_t o = 0;
            for (i = 0; s + i < end; i++) {
                if (s[i] == '\\' && s + i + 1 < end) {
                    i++;   /* 跳过转义符，保留下一个字符 */
                }
                pat[o++] = s[i];
            }
            patlen = o;
        }
        pat[patlen] = '\0';
        rule.pattern = pat;

        /* 追加规则 */
        {
            LgGitignoreRule *nr = (LgGitignoreRule *)realloc(gi->rules,
                                                             (gi->count + 1) * sizeof(*nr));
            if (nr == NULL) {
                free(pat);
                lg_gitignore_free(gi);
                return NULL;
            }
            gi->rules = nr;
            gi->rules[gi->count++] = rule;
        }

next:
        line = eol ? eol + 1 : NULL;
    }

    return gi;
}

void lg_gitignore_free(LgGitignore *gi)
{
    size_t i;
    if (gi == NULL) {
        return;
    }
    for (i = 0; i < gi->count; i++) {
        free(gi->rules[i].pattern);
    }
    free(gi->rules);
    free(gi);
}

/* --------------------------------------------------------------------------
 * 匹配
 * -------------------------------------------------------------------------- */
/* 规则按路径匹配（含锚定与非锚定处理） */
static int LgRuleMatchesPath(const LgGitignoreRule *rule, const char *path)
{
    const char *p = rule->pattern;

    if (rule->anchored) {
        while (*p == '/') {
            p++;
        }
        return LgGlobMatch(p, path);
    }

    /* 非锚定：匹配整条路径或最后一段（任意层级） */
    if (LgGlobMatch(p, path)) {
        return 1;
    }
    {
        const char *base = strrchr(path, '/');
        return base != NULL && LgGlobMatch(p, base + 1);
    }
}

static int LgRuleMatches(const LgGitignoreRule *rule, const char *path, int is_dir)
{
    if (rule->dir_only && !is_dir) {
        /* 目录专用规则 vs 文件：检查各祖先目录是否被该规则忽略 */
        char buf[512];
        char *slash;
        snprintf(buf, sizeof(buf), "%s", path);
        while ((slash = strrchr(buf, '/')) != NULL) {
            *slash = '\0';
            if (buf[0] == '\0') {
                break;
            }
            if (LgRuleMatchesPath(rule, buf)) {
                return 1;
            }
        }
        return 0;
    }

    return LgRuleMatchesPath(rule, path);
}

int lg_gitignore_match(const LgGitignore *gi, const char *path, int is_dir)
{
    size_t i;
    int ignored = 0;

    if (gi == NULL || path == NULL) {
        return 0;
    }

    for (i = 0; i < gi->count; i++) {
        if (LgRuleMatches(&gi->rules[i], path, is_dir)) {
            ignored = gi->rules[i].negated ? 0 : 1;   /* 后匹配者胜 */
        }
    }
    return ignored;
}
