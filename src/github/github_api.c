/*
 * github_api.c — GitHub REST API 公共层实现
 */
#include "lightgit/github/github_api.h"
#include "lightgit/net/http.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LG_GITHUB_API_HOST L"api.github.com"
#define LG_GITHUB_API_PORT 443

/* 组装 API 请求头（宽字符串，malloc）：Bearer 认证 + Accept + API 版本 */
static wchar_t *LgBuildApiHeaders(const char *token, const char *extra_ascii_headers)
{
    char buf[2048];
    int n = 0;
    wchar_t *wide;
    int wn;

    buf[0] = '\0';

    if (token != NULL && token[0] != '\0') {
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "Authorization: Bearer %s\r\n", token);
    }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n,
                  "Accept: application/vnd.github+json\r\n"
                  "X-GitHub-Api-Version: 2022-11-28\r\n");
    if (extra_ascii_headers != NULL && extra_ascii_headers[0] != '\0') {
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s", extra_ascii_headers);
    }
    if (n < 0 || (size_t)n >= sizeof(buf)) {
        return NULL;
    }

    wn = MultiByteToWideChar(CP_UTF8, 0, buf, n, NULL, 0);
    if (wn <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)(wn + 1) * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, buf, n, wide, wn);
    wide[wn] = L'\0';
    return wide;
}

/* 根据 HTTP 状态码分类错误 */
static lg_github_error_code LgClassifyStatus(DWORD status)
{
    if (status == 401) return LG_GITHUB_ERR_AUTH;
    if (status == 403) return LG_GITHUB_ERR_FORBIDDEN;
    if (status == 404) return LG_GITHUB_ERR_NOT_FOUND;
    if (status >= 500) return LG_GITHUB_ERR_SERVER;
    return LG_GITHUB_ERR_OTHER;
}

void lg_github_error_from_body(lg_github_error *err, lg_github_error_code code,
                               const unsigned char *body, size_t body_len)
{
    lg_json_value *root = NULL;

    if (err == NULL) {
        return;
    }
    err->code = code;
    err->message[0] = '\0';

    if (body != NULL && body_len > 0 &&
        lg_json_parse((const char *)body, body_len, &root) == 0) {
        const char *msg = lg_json_get_string(root, "message");
        if (msg != NULL && msg[0] != '\0') {
            snprintf(err->message, sizeof(err->message), "%s", msg);
        }
        lg_json_free(root);
    }
}

/* 内部：GET/POST 公共流程 */
static int LgGithubRequest(const wchar_t *method, const char *path, const char *token,
                           const char *json_body, const char *content_type,
                           lg_json_value **out, lg_github_error *err)
{
    wchar_t *headers = NULL;
    wchar_t *wpath = NULL;
    lg_http_result resp;
    int wn;
    int rc = -1;

    if (out != NULL) *out = NULL;

    /* path 为 ASCII（GitHub API 路径），转宽 */
    wn = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wn <= 0) {
        return -1;
    }
    wpath = (wchar_t *)malloc((size_t)wn * sizeof(wchar_t));
    if (wpath == NULL) {
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wn);

    headers = LgBuildApiHeaders(token, NULL);
    if (headers == NULL) {
        free(wpath);
        return -1;
    }

    memset(&resp, 0, sizeof(resp));
    if (wcscmp(method, L"GET") == 0) {
        if (lg_http_get(LG_GITHUB_API_HOST, LG_GITHUB_API_PORT, wpath,
                        NULL, NULL, headers, &resp) != 0) {
            if (err != NULL) {
                DWORD we = lg_http_last_error();
                err->code = LG_GITHUB_ERR_NETWORK;
                snprintf(err->message, sizeof(err->message),
                         "Network failed: %s (WinHTTP %lu).",
                         lg_http_error_desc(we), (unsigned long)we);
            }
            goto cleanup;
        }
    } else {
        if (lg_http_post(LG_GITHUB_API_HOST, LG_GITHUB_API_PORT, wpath,
                         NULL, NULL, content_type, "application/vnd.github+json",
                         (const unsigned char *)json_body, json_body ? strlen(json_body) : 0,
                         &resp) != 0) {
            if (err != NULL) {
                DWORD we = lg_http_last_error();
                err->code = LG_GITHUB_ERR_NETWORK;
                snprintf(err->message, sizeof(err->message),
                         "Network failed: %s (WinHTTP %lu).",
                         lg_http_error_desc(we), (unsigned long)we);
            }
            goto cleanup;
        }
    }

    /* 2xx 成功 */
    if (resp.status >= 200 && resp.status < 300) {
        if (out != NULL && resp.body != NULL && resp.body_len > 0) {
            if (lg_json_parse((const char *)resp.body, resp.body_len, out) != 0) {
                if (err != NULL) {
                    err->code = LG_GITHUB_ERR_OTHER;
                    snprintf(err->message, sizeof(err->message), "Invalid JSON response.");
                }
                goto cleanup;
            }
        }
        if (err != NULL) {
            err->code = LG_GITHUB_OK;
            err->message[0] = '\0';
        }
        rc = 0;
        goto cleanup;
    }

    /* 非 2xx：分类错误 */
    {
        lg_github_error_code code = LgClassifyStatus(resp.status);
        lg_github_error_from_body(err, code, resp.body, resp.body_len);
        if (err != NULL && err->message[0] == '\0') {
            snprintf(err->message, sizeof(err->message), "GitHub API error (HTTP %lu).",
                     (unsigned long)resp.status);
        }
    }

cleanup:
    free(headers);
    free(wpath);
    lg_http_result_free(&resp);
    return rc;
}

int lg_github_get_json(const char *path, const char *token,
                       lg_json_value **out, lg_github_error *err)
{
    return LgGithubRequest(L"GET", path, token, NULL, NULL, out, err);
}

int lg_github_post_json(const char *path, const char *token,
                        const char *json_body,
                        lg_json_value **out, lg_github_error *err)
{
    return LgGithubRequest(L"POST", path, token, json_body,
                           "application/json", out, err);
}
