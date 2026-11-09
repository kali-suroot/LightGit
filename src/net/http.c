/*
 * http.c — WinHTTP HTTPS 客户端实现
 */
#include "lightgit/net/http.h"
#include "lightgit/common/buffer.h"

#include <winhttp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 应用 User-Agent（GitHub API 要求必须带 User-Agent） */
#define LG_HTTP_USER_AGENT L"LightGit/0.1"

/* 超时时间（毫秒） */
#define LG_HTTP_RESOLVE_TIMEOUT 15000
#define LG_HTTP_CONNECT_TIMEOUT 15000
#define LG_HTTP_SEND_TIMEOUT    60000
#define LG_HTTP_RECEIVE_TIMEOUT 60000

/* 线程本地的最后一次 WinHTTP 错误码 */
static __declspec(thread) DWORD g_lg_http_last_error = 0;

/* 记录当前 WinHTTP 错误并跳到 cleanup */
#define LG_HTTP_FAIL() do { g_lg_http_last_error = GetLastError(); goto cleanup; } while (0)

DWORD lg_http_last_error(void)
{
    return g_lg_http_last_error;
}

const char *lg_http_error_desc(DWORD code)
{
    switch (code) {
    case ERROR_WINHTTP_TIMEOUT:              return "Timeout";
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:    return "DNS name not resolved";
    case ERROR_WINHTTP_CANNOT_CONNECT:       return "Cannot connect to server";
    case ERROR_WINHTTP_CONNECTION_ERROR:     return "Connection error";
    case ERROR_WINHTTP_SECURE_FAILURE:       return "TLS/SSL handshake failed";
    case ERROR_WINHTTP_INVALID_SERVER_RESPONSE: return "Invalid server response";
    case ERROR_WINHTTP_OPERATION_CANCELLED:  return "Operation cancelled";
    case ERROR_WINHTTP_RESEND_REQUEST:       return "Resend request";
    default:                                 return "WinHTTP error";
    }
}

/* --------------------------------------------------------------------------
 * Base64 编码（用于 HTTP Basic 认证头）
 * -------------------------------------------------------------------------- */
static const char g_b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t LgBase64Encode(const unsigned char *in, size_t in_len,
                             char *out, size_t out_cap)
{
    size_t i, o = 0;

    for (i = 0; i < in_len; i += 3) {
        unsigned int n = (unsigned int)in[i] << 16;
        if (i + 1 < in_len) n |= (unsigned int)in[i + 1] << 8;
        if (i + 2 < in_len) n |= (unsigned int)in[i + 2];

        out[o++] = g_b64_table[(n >> 18) & 0x3F];
        out[o++] = g_b64_table[(n >> 12) & 0x3F];
        out[o++] = (i + 1 < in_len) ? g_b64_table[(n >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < in_len) ? g_b64_table[n & 0x3F] : '=';
    }
    out[o] = '\0';
    return o;
}

/* 构建 "Authorization: Basic base64(user:token)\r\n"，返回宽字符串（malloc） */
static wchar_t *LgBuildBasicAuthHeader(const char *user, const char *token)
{
    char plain[1024];
    char b64[1536];
    char header[1600];
    wchar_t *wide;
    int plain_len, header_len, wide_len;

    if (user == NULL) user = "x-access-token";
    if (token == NULL) token = "";

    plain_len = snprintf(plain, sizeof(plain), "%s:%s", user, token);
    if (plain_len < 0 || (size_t)plain_len >= sizeof(plain)) {
        return NULL;
    }

    LgBase64Encode((const unsigned char *)plain, (size_t)plain_len, b64, sizeof(b64));

    header_len = snprintf(header, sizeof(header), "Authorization: Basic %s\r\n", b64);
    if (header_len < 0 || (size_t)header_len >= (int)sizeof(header)) {
        return NULL;
    }

    wide_len = MultiByteToWideChar(CP_UTF8, 0, header, header_len, NULL, 0);
    if (wide_len <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)(wide_len + 1) * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, header, header_len, wide, wide_len);
    wide[wide_len] = L'\0';
    return wide;
}

/* 构建指定头 "<name>: <value>\r\n"（宽字符串，malloc），name/value 为 ASCII */
static wchar_t *LgBuildHeader(const char *name, const char *value)
{
    char buf[512];
    wchar_t *wide;
    int n, wn;

    n = snprintf(buf, sizeof(buf), "%s: %s\r\n", name, value ? value : "");
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

/* 读取响应体到 lg_buffer */
static int LgReadResponseBody(HINTERNET hRequest, lg_buffer *out)
{
    unsigned char buf[16384];
    DWORD read = 0;

    for (;;) {
        if (!WinHttpReadData(hRequest, buf, sizeof(buf), &read)) {
            return -1;
        }
        if (read == 0) {
            return 0;   /* 读取完毕 */
        }
        if (lg_buffer_append(out, buf, read) != 0) {
            return -1;
        }
    }
}

/* 查询 HTTP 状态码 */
static DWORD LgQueryStatusCode(HINTERNET hRequest)
{
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return status;
}

/* 内部：执行一次请求（GET 或 POST） */
static int LgHttpRequest(const wchar_t *host, unsigned short port, const wchar_t *path,
                         const wchar_t *method,
                         const char *auth_user, const char *auth_token,
                         const wchar_t *extra_headers,
                         const unsigned char *body, size_t body_len,
                         lg_http_result *out)
{
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    wchar_t *auth_header = NULL;
    wchar_t content_length[64];
    DWORD total = (DWORD)body_len;
    size_t sent = 0;
    lg_buffer resp;
    int ok = -1;

    if (out == NULL) {
        return -1;
    }
    out->status = 0;
    out->body = NULL;
    out->body_len = 0;
    lg_buffer_init(&resp);

    hSession = WinHttpOpen(LG_HTTP_USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession == NULL) {
        LG_HTTP_FAIL();
    }
    WinHttpSetTimeouts(hSession, LG_HTTP_RESOLVE_TIMEOUT, LG_HTTP_CONNECT_TIMEOUT,
                       LG_HTTP_SEND_TIMEOUT, LG_HTTP_RECEIVE_TIMEOUT);

    hConnect = WinHttpConnect(hSession, host, port, 0);
    if (hConnect == NULL) {
        LG_HTTP_FAIL();
    }

    /* HTTPS：WINHTTP_FLAG_SECURE；证书校验保持默认开启 */
    hRequest = WinHttpOpenRequest(hConnect, method, path, NULL,
                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  WINHTTP_FLAG_SECURE);
    if (hRequest == NULL) {
        LG_HTTP_FAIL();
    }

    /* 附加请求头 */
    if (auth_token != NULL) {
        auth_header = LgBuildBasicAuthHeader(auth_user, auth_token);
        if (auth_header == NULL ||
            !WinHttpAddRequestHeaders(hRequest, auth_header, (DWORD)-1L,
                                      WINHTTP_ADDREQ_FLAG_ADD)) {
            LG_HTTP_FAIL();
        }
    }
    if (extra_headers != NULL && extra_headers[0] != L'\0') {
        if (!WinHttpAddRequestHeaders(hRequest, extra_headers, (DWORD)-1L,
                                      WINHTTP_ADDREQ_FLAG_ADD)) {
            LG_HTTP_FAIL();
        }
    }

    if (body_len > 0) {
        swprintf(content_length, 64, L"Content-Length: %lu\r\n", (unsigned long)body_len);
        if (!WinHttpAddRequestHeaders(hRequest, content_length, (DWORD)-1L,
                                      WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
            LG_HTTP_FAIL();
        }
    }

    /* 发送请求：body 较大时通过 WriteData 分块发送 */
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, total, 0)) {
        LG_HTTP_FAIL();
    }

    if (body_len > 0 && body != NULL) {
        while (sent < body_len) {
            DWORD chunk = (DWORD)((body_len - sent) > 65536 ? 65536 : (body_len - sent));
            DWORD written = 0;
            if (!WinHttpWriteData(hRequest, body + sent, chunk, &written)) {
                LG_HTTP_FAIL();
            }
            sent += written;
            if (written == 0) {
                LG_HTTP_FAIL();
            }
        }
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        LG_HTTP_FAIL();
    }

    if (LgReadResponseBody(hRequest, &resp) != 0) {
        LG_HTTP_FAIL();
    }

    out->status = LgQueryStatusCode(hRequest);
    out->body = resp.data;
    out->body_len = resp.len;
    resp.data = NULL;   /* 所有权转移给 out */
    resp.len = 0;
    ok = 0;

cleanup:
    free(auth_header);
    lg_buffer_free(&resp);
    if (hRequest != NULL) WinHttpCloseHandle(hRequest);
    if (hConnect != NULL) WinHttpCloseHandle(hConnect);
    if (hSession != NULL) WinHttpCloseHandle(hSession);
    return ok;
}

void lg_http_result_free(lg_http_result *r)
{
    if (r == NULL) {
        return;
    }
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
    r->status = 0;
}

int lg_http_get(const wchar_t *host, unsigned short port, const wchar_t *path,
                const char *auth_user, const char *auth_token,
                const wchar_t *extra_headers,
                lg_http_result *out)
{
    return LgHttpRequest(host, port, path, L"GET", auth_user, auth_token,
                         extra_headers, NULL, 0, out);
}

int lg_http_post(const wchar_t *host, unsigned short port, const wchar_t *path,
                 const char *auth_user, const char *auth_token,
                 const char *content_type, const char *accept,
                 const unsigned char *body, size_t body_len,
                 lg_http_result *out)
{
    wchar_t *ct = NULL, *ac = NULL, *hdr = NULL;
    size_t hdr_len = 0;
    int result;

    /* 组装 Content-Type / Accept 头 */
    if (content_type != NULL) {
        ct = LgBuildHeader("Content-Type", content_type);
    }
    if (accept != NULL) {
        ac = LgBuildHeader("Accept", accept);
    }
    if (ct != NULL || ac != NULL) {
        size_t a = ct ? wcslen(ct) : 0;
        size_t b = ac ? wcslen(ac) : 0;
        hdr = (wchar_t *)malloc((a + b + 1) * sizeof(wchar_t));
        if (hdr == NULL) {
            free(ct); free(ac);
            return -1;
        }
        hdr[0] = L'\0';
        if (ct != NULL) { memcpy(hdr, ct, a * sizeof(wchar_t)); hdr_len += a; }
        if (ac != NULL) { memcpy(hdr + hdr_len, ac, b * sizeof(wchar_t)); hdr_len += b; }
        hdr[hdr_len] = L'\0';
    }

    result = LgHttpRequest(host, port, path, L"POST", auth_user, auth_token,
                           hdr, body, body_len, out);
    free(ct);
    free(ac);
    free(hdr);
    return result;
}
