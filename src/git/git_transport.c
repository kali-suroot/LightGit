/*
 * git_transport.c — Git Smart HTTP 传输实现（receive-pack 推送）
 */
#include "lightgit/git/git_transport.h"
#include "lightgit/git/git_pktline.h"
#include "lightgit/net/http.h"
#include "lightgit/common/buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LG_GIT_HOST L"github.com"
#define LG_GIT_PORT 443

/* --------------------------------------------------------------------------
 * 路径构造："/{owner}/{repo}.git{suffix}"
 * -------------------------------------------------------------------------- */
static wchar_t *LgBuildPathW(const char *owner, const char *repo, const char *suffix)
{
    char path[1024];
    wchar_t *wide;
    int n, wn;

    n = snprintf(path, sizeof(path), "/%s/%s.git%s", owner, repo, suffix);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return NULL;
    }
    wn = MultiByteToWideChar(CP_UTF8, 0, path, n, NULL, 0);
    if (wn <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)(wn + 1) * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, path, n, wide, wn);
    wide[wn] = L'\0';
    return wide;
}

/* --------------------------------------------------------------------------
 * 解析一条引用广告行："<40hex> <name>[NUL <caps>]\n"
 * 返回 0 成功；跳过 "capabilities^{}" 伪引用时返回 1。
 * -------------------------------------------------------------------------- */
static int LgParseRefLine(const unsigned char *payload, size_t len, lg_remote_ref *ref)
{
    size_t i, name_len;

    if (payload == NULL || len < 41 || ref == NULL) {
        return -1;
    }

    /* 前 40 字节为对象 ID */
    memcpy(ref->oid, payload, LG_GIT_HASH_HEX_SIZE);
    ref->oid[LG_GIT_HASH_HEX_SIZE] = '\0';

    /* 跳过空格 */
    if (payload[40] != ' ') {
        return -1;
    }
    i = 41;

    /* 名称直到 NUL 或换行 */
    name_len = 0;
    while (i < len && payload[i] != '\0' && payload[i] != '\n' && name_len < sizeof(ref->name) - 1) {
        ref->name[name_len++] = (char)payload[i++];
    }
    ref->name[name_len] = '\0';

    /* "capabilities^{}" 是空仓库的伪引用，不视为真实 ref */
    if (name_len >= 2 && strcmp(ref->name + name_len - 2, "^{}") == 0) {
        return 1;
    }
    return 0;
}

int lg_transport_discover(const char *owner, const char *repo,
                          const char *auth_user, const char *auth_token,
                          lg_remote_ref **refs, size_t *ref_count,
                          char *error, size_t error_cap)
{
    wchar_t *path = NULL;
    lg_http_result resp;
    size_t offset = 0, count = 0, cap = 0;
    lg_remote_ref *list = NULL;
    int result = -1;

    if (error != NULL && error_cap > 0) error[0] = '\0';

    if (owner == NULL || repo == NULL || refs == NULL || ref_count == NULL) {
        if (error != NULL && error_cap > 0) snprintf(error, error_cap, "Invalid arguments.");
        return -1;
    }
    *refs = NULL;
    *ref_count = 0;

    path = LgBuildPathW(owner, repo, "/info/refs?service=git-receive-pack");
    if (path == NULL) {
        if (error != NULL && error_cap > 0) snprintf(error, error_cap, "Out of memory.");
        return -1;
    }

    if (lg_http_get(LG_GIT_HOST, LG_GIT_PORT, path, auth_user, auth_token,
                    NULL, &resp) != 0) {
        DWORD we = lg_http_last_error();
        if (error != NULL && error_cap > 0) {
            snprintf(error, error_cap, "Network failed: %s (WinHTTP %lu).",
                     lg_http_error_desc(we), (unsigned long)we);
        }
        free(path);
        return -1;
    }
    free(path);

    if (resp.status != 200) {
        if (error != NULL && error_cap > 0) {
            snprintf(error, error_cap, "HTTP %lu from info/refs.",
                     (unsigned long)resp.status);
        }
        lg_http_result_free(&resp);
        return -1;
    }

    /* ---- 解析 pkt-line 流 ---- */
    while (offset < resp.body_len) {
        const unsigned char *payload = NULL;
        size_t plen = 0;
        int r = lg_pktline_decode(resp.body, resp.body_len, offset, &payload, &plen);
        if (r < 0) {
            if (error != NULL && error_cap > 0) {
                snprintf(error, error_cap, "Failed to parse ref advertisement.");
            }
            goto cleanup;
        }
        offset += (size_t)r;

        if (plen == 0) {
            continue;   /* flush / delim */
        }

        /* 跳过 "# service=..." 公告行 */
        if (plen >= 9 && memcmp(payload, "# service=", 10) == 0) {
            continue;
        }

        /* 解析引用行 */
        if (count == cap) {
            size_t ncap = (cap == 0) ? 8 : cap * 2;
            lg_remote_ref *nl = (lg_remote_ref *)realloc(list, ncap * sizeof(*list));
            if (nl == NULL) {
                goto cleanup;
            }
            list = nl;
            cap = ncap;
        }
        {
            int pr = LgParseRefLine(payload, plen, &list[count]);
            if (pr == 0) {
                count++;
            }
            /* pr==1 为伪引用，跳过；pr==-1 非法，忽略 */
        }
    }

    *refs = list;
    *ref_count = count;
    list = NULL;   /* 所有权转移 */
    result = 0;

cleanup:
    free(list);
    lg_http_result_free(&resp);
    return result;
}

int lg_transport_push(const char *owner, const char *repo,
                      const char *branch,
                      const char *old_oid, const char *new_oid,
                      const unsigned char *packfile, size_t pack_len,
                      const char *auth_user, const char *auth_token,
                      lg_push_result *result)
{
    wchar_t *path = NULL;
    lg_buffer body;
    unsigned char pkt[LG_PKTLINE_MAX_TOTAL];
    unsigned char flush[4];
    char cmd[512];
    size_t cmd_len, pkt_len;
    lg_http_result resp;
    int rc = -1;

    if (result == NULL) {
        return -1;
    }
    result->success = 0;
    result->error[0] = '\0';

    if (owner == NULL || repo == NULL || branch == NULL ||
        old_oid == NULL || new_oid == NULL || packfile == NULL) {
        return -1;
    }

    /* ---- 组装命令 pkt-line："<old> <new> refs/heads/<branch>\0report-status\n" ---- */
    cmd_len = (size_t)snprintf(cmd, sizeof(cmd), "%s %s refs/heads/%s",
                               old_oid, new_oid, branch);
    if (cmd_len + 1 + strlen("report-status") + 1 >= sizeof(cmd)) {
        return -1;
    }
    cmd[cmd_len++] = '\0';                                   /* 能力分隔 NUL */
    memcpy(cmd + cmd_len, "report-status\n", strlen("report-status\n"));
    cmd_len += strlen("report-status\n");

    /* ---- 组装 POST body：命令 pkt-line + flush + packfile ---- */
    lg_buffer_init(&body);
    pkt_len = lg_pktline_encode(pkt, sizeof(pkt), cmd, cmd_len);
    if (pkt_len == 0) {
        goto cleanup;
    }
    if (lg_buffer_append(&body, pkt, pkt_len) != 0) {
        goto cleanup;
    }
    lg_pktline_encode_flush(flush);
    if (lg_buffer_append(&body, flush, 4) != 0) {
        goto cleanup;
    }
    if (lg_buffer_append(&body, packfile, pack_len) != 0) {
        goto cleanup;
    }

    path = LgBuildPathW(owner, repo, "/git-receive-pack");
    if (path == NULL) {
        goto cleanup;
    }

    if (lg_http_post(LG_GIT_HOST, LG_GIT_PORT, path, auth_user, auth_token,
                     "application/x-git-receive-pack-request",
                     "application/x-git-receive-pack-result",
                     body.data, body.len, &resp) != 0) {
        DWORD we = lg_http_last_error();
        snprintf(result->error, sizeof(result->error),
                 "Network failed: %s (WinHTTP %lu).",
                 lg_http_error_desc(we), (unsigned long)we);
        goto cleanup;
    }

    if (resp.status != 200) {
        snprintf(result->error, sizeof(result->error),
                 "HTTP %lu", (unsigned long)resp.status);
        goto cleanup;
    }

    /* ---- 解析 report-status 响应 ---- */
    {
        size_t off = 0;
        while (off < resp.body_len) {
            const unsigned char *payload = NULL;
            size_t plen = 0;
            int r = lg_pktline_decode(resp.body, resp.body_len, off, &payload, &plen);
            if (r < 0) {
                break;
            }
            off += (size_t)r;
            if (plen == 0) {
                break;   /* flush：结束 */
            }

            if (plen >= 7 && memcmp(payload, "unpack ", 7) == 0) {
                if (!(plen >= 10 && memcmp(payload, "unpack ok", 9) == 0)) {
                    size_t n = plen < sizeof(result->error) - 1 ? plen : sizeof(result->error) - 1;
                    memcpy(result->error, payload, n);
                    result->error[n] = '\0';
                }
            } else if (plen >= 3 && memcmp(payload, "ng ", 3) == 0) {
                size_t n = plen < sizeof(result->error) - 1 ? plen : sizeof(result->error) - 1;
                memcpy(result->error, payload, n);
                result->error[n] = '\0';
            }
            /* "ok <ref>" 无需处理，继续 */
        }
    }

    /* 无任何错误信息即视为成功 */
    result->success = (result->error[0] == '\0') ? 1 : 0;
    rc = 0;

cleanup:
    lg_buffer_free(&body);
    free(path);
    lg_http_result_free(&resp);
    return rc;
}
