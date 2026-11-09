/*
 * github_api.h — GitHub REST API 公共层
 *
 * 封装对 api.github.com 的 HTTPS 请求（Bearer 认证 + JSON），
 * 统一做错误分类（网络/鉴权/权限/限流/未找到/服务端）。
 * 上层模块（auth / repository / tree）在此之上实现具体端点。
 */
#ifndef LIGHTGIT_GITHUB_GITHUB_API_H
#define LIGHTGIT_GITHUB_GITHUB_API_H

#include "lightgit/common/json.h"

/* 错误分类码 */
typedef enum lg_github_error_code {
    LG_GITHUB_OK = 0,
    LG_GITHUB_ERR_NETWORK,      /* 网络/DNS/连接/超时失败 */
    LG_GITHUB_ERR_AUTH,         /* 401：Token 无效 */
    LG_GITHUB_ERR_FORBIDDEN,    /* 403：权限不足或限流 */
    LG_GITHUB_ERR_NOT_FOUND,    /* 404：资源不存在 */
    LG_GITHUB_ERR_SERVER,       /* 5xx：GitHub 服务端错误 */
    LG_GITHUB_ERR_OTHER         /* 其他 HTTP 错误 */
} lg_github_error_code;

typedef struct lg_github_error {
    lg_github_error_code code;
    char                 message[256];   /* 可读错误信息（不含 Token） */
} lg_github_error;

/*
 * GET 请求并解析 JSON 响应。
 * path 如 "/user"、"/user/repos?per_page=100&page=1"。
 * 成功（2xx）返回 0 并把根节点写入 *out（调用方 lg_json_free）；
 * 失败返回 -1 并通过 err 描述原因。
 */
int lg_github_get_json(const char *path, const char *token,
                       lg_json_value **out, lg_github_error *err);

/*
 * POST 请求（json_body 为 JSON 文本或 NULL）并解析响应。
 * 成功（2xx）返回 0 并写入 *out；失败返回 -1 并通过 err 描述。
 */
int lg_github_post_json(const char *path, const char *token,
                        const char *json_body,
                        lg_json_value **out, lg_github_error *err);

/* 把 GitHub 响应体中的 "message" 字段提取为错误信息（尽力而为） */
void lg_github_error_from_body(lg_github_error *err, lg_github_error_code code,
                               const unsigned char *body, size_t body_len);

#endif /* LIGHTGIT_GITHUB_GITHUB_API_H */
