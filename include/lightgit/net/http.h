/*
 * http.h — 基于 WinHTTP 的 HTTPS 客户端封装
 *
 * 用于 GitHub REST API（Bearer 认证）与 Git Smart HTTP（Basic 认证）。
 * 只使用 Windows 自带的 WinHTTP，不引入 libcurl。
 * 默认开启服务器证书校验（WinHTTP 默认行为，不关闭）。
 */
#ifndef LIGHTGIT_NET_HTTP_H
#define LIGHTGIT_NET_HTTP_H

#include <stddef.h>
#include <windows.h>

/* 一次 HTTP 请求的结果 */
typedef struct lg_http_result {
    DWORD          status;    /* HTTP 状态码（如 200/401/404） */
    unsigned char *body;      /* 响应体（malloc，可能为 NULL） */
    size_t         body_len;  /* 响应体字节数 */
} lg_http_result;

/* 释放 lg_http_result 内部内存 */
void lg_http_result_free(lg_http_result *r);

/*
 * GET 请求。
 * auth_user / auth_token 为 NULL 表示无认证；否则使用 HTTP Basic（token 作为密码）。
 * extra_headers 为可选附加头（宽字符，需以 \r\n 结尾，可为 NULL）。
 * 成功返回 0（无论 HTTP 状态码），失败返回 -1；状态码见 out->status。
 */
int lg_http_get(const wchar_t *host, unsigned short port, const wchar_t *path,
                const char *auth_user, const char *auth_token,
                const wchar_t *extra_headers,
                lg_http_result *out);

/*
 * POST 请求（二进制 body）。
 * content_type / accept 为对应头的值（可为 NULL）。
 * body 可为 NULL（body_len==0）。
 */
int lg_http_post(const wchar_t *host, unsigned short port, const wchar_t *path,
                 const char *auth_user, const char *auth_token,
                 const char *content_type, const char *accept,
                 const unsigned char *body, size_t body_len,
                 lg_http_result *out);

/*
 * 返回最近一次 WinHTTP 失败的错误码（线程本地）。
 * 上层在 lg_http_get/post 返回 -1 后调用，用于生成详细错误信息。
 */
DWORD lg_http_last_error(void);

/* 返回 WinHTTP 错误码的友好描述（ASCII） */
const char *lg_http_error_desc(DWORD code);

#endif /* LIGHTGIT_NET_HTTP_H */
