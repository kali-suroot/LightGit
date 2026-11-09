/*
 * publish_manager.c — 发布管理实现（一键发布）
 *
 * 核心流程见 publish_manager.h。合并规则（项目书 §5）：
 *   - 以远程树为基线，只替换“选中”的本地文件；
 *   - 保留远端存在但本地没有/未选中的文件；
 *   - 本地删除不传播到远端；
 *   - 旧对象 ID（old_oid）使用发现到的远端 HEAD，交给服务端做 non-fast-forward 检测。
 */
#include "lightgit/app/publish_manager.h"

#include "lightgit/file/file_scanner.h"
#include "lightgit/file/gitignore.h"
#include "lightgit/git/git_hash.h"
#include "lightgit/git/git_object.h"
#include "lightgit/git/git_blob.h"
#include "lightgit/git/git_tree.h"
#include "lightgit/git/git_commit.h"
#include "lightgit/git/git_pack.h"
#include "lightgit/git/git_transport.h"
#include "lightgit/github/github_tree.h"
#include "lightgit/github/github_api.h"
#include "lightgit/common/hex.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>   /* HANDLE/CreateFileW/GetFileSizeEx 等文件 API */

/* 单文件 100 MiB 上限（GitHub 普通 Git 推送限制） */
#define LG_MAX_FILE_SIZE (100ULL * 1024ULL * 1024ULL)

/* --------------------------------------------------------------------------
 * 对象暂存（用于生成 packfile）
 * -------------------------------------------------------------------------- */
typedef struct LgPObj {
    lg_object_type type;
    unsigned char *content;
    size_t         len;
    char           oid[LG_GIT_HASH_HEX_SIZE + 1];
} LgPObj;

typedef struct LgPStore {
    LgPObj *items;
    size_t  count;
    size_t  cap;
} LgPStore;

static int LgPStoreAdd(LgPStore *s, lg_object_type type,
                       const unsigned char *content, size_t len, const char *oid)
{
    LgPObj *it;
    if (s->count == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 16;
        LgPObj *ni = (LgPObj *)realloc(s->items, ncap * sizeof(*ni));
        if (ni == NULL) return -1;
        s->items = ni;
        s->cap = ncap;
    }
    it = &s->items[s->count++];
    it->type = type;
    it->content = (unsigned char *)malloc(len ? len : 1);
    if (it->content == NULL) return -1;
    if (len) memcpy(it->content, content, len);
    it->len = len;
    memcpy(it->oid, oid, LG_GIT_HASH_HEX_SIZE + 1);
    return 0;
}

static void LgPStoreFree(LgPStore *s)
{
    size_t i;
    for (i = 0; i < s->count; i++) free(s->items[i].content);
    free(s->items);
    memset(s, 0, sizeof(*s));
}

/* --------------------------------------------------------------------------
 * 扁平条目 map（path → mode/type/sha），用于合并
 * -------------------------------------------------------------------------- */
typedef struct LgFlatEntry {
    char *path;
    char  mode[8];
    char  type[8];
    char  sha[LG_GIT_HASH_HEX_SIZE + 1];
} LgFlatEntry;

typedef struct LgFlatMap {
    LgFlatEntry *items;
    size_t       count;
    size_t       cap;
} LgFlatMap;

static void LgFlatMapInit(LgFlatMap *m) { memset(m, 0, sizeof(*m)); }

static void LgFlatMapFree(LgFlatMap *m)
{
    size_t i;
    for (i = 0; i < m->count; i++) free(m->items[i].path);
    free(m->items);
    memset(m, 0, sizeof(*m));
}

static LgFlatEntry *LgFlatMapFind(LgFlatMap *m, const char *path)
{
    size_t i;
    for (i = 0; i < m->count; i++) {
        if (strcmp(m->items[i].path, path) == 0) return &m->items[i];
    }
    return NULL;
}

static int LgFlatMapUpsert(LgFlatMap *m, const char *path,
                           const char *mode, const char *type, const char *sha)
{
    LgFlatEntry *e = LgFlatMapFind(m, path);
    if (e != NULL) {
        snprintf(e->mode, sizeof(e->mode), "%s", mode ? mode : "");
        snprintf(e->type, sizeof(e->type), "%s", type ? type : "");
        snprintf(e->sha, sizeof(e->sha), "%s", sha ? sha : "");
        return 0;
    }
    if (m->count == m->cap) {
        size_t ncap = m->cap ? m->cap * 2 : 16;
        LgFlatEntry *ni = (LgFlatEntry *)realloc(m->items, ncap * sizeof(*ni));
        if (ni == NULL) return -1;
        m->items = ni;
        m->cap = ncap;
    }
    e = &m->items[m->count];
    memset(e, 0, sizeof(*e));
    e->path = _strdup(path);
    if (e->path == NULL) return -1;
    snprintf(e->mode, sizeof(e->mode), "%s", mode ? mode : "");
    snprintf(e->type, sizeof(e->type), "%s", type ? type : "");
    snprintf(e->sha, sizeof(e->sha), "%s", sha ? sha : "");
    m->count++;
    return 0;
}

/* --------------------------------------------------------------------------
 * 递归构建合并后的 Tree（含远程子树复用）
 * -------------------------------------------------------------------------- */
static unsigned int LgParseMode(const char *mode)
{
    if (mode == NULL || mode[0] == '\0') return LG_TREE_MODE_FILE;
    return (unsigned int)strtoul(mode, NULL, 8);
}

/* 判断路径是否在 dir 目录内，并返回 dir 之后的相对部分 */
static int LgPathInDir(const char *path, const char *dir, const char **rel)
{
    size_t dlen = strlen(dir);
    if (dlen == 0) {
        *rel = path;
        return 1;
    }
    if (strncmp(path, dir, dlen) == 0 && path[dlen] == '/') {
        *rel = path + dlen + 1;
        return 1;
    }
    return 0;
}

static int LgBuildTreeLevel(LgPStore *store, LgFlatMap *map, const char *dir,
                            char out_sha[LG_GIT_HASH_HEX_SIZE + 1])
{
    lg_tree_entry *entries = NULL;
    size_t ecount = 0, ecap = 0;
    unsigned char *tree_buf = NULL;
    size_t tree_len = 0;
    char tree_sha[LG_GIT_HASH_HEX_SIZE + 1];
    const LgFlatEntry *remote = NULL;
    int result = -1;
    size_t i;

    /* 该目录的远程 tree 条目（用于复用） */
    remote = LgFlatMapFind(map, dir);

    /* 遍历 map，收集 dir 的直接子项 */
    for (i = 0; i < map->count; i++) {
        const LgFlatEntry *e = &map->items[i];
        const char *rel;
        const char *slash;
        char name[512];
        size_t namelen;

        if (!LgPathInDir(e->path, dir, &rel)) continue;

        /* 取第一段作为直接子项名 */
        slash = strchr(rel, '/');
        namelen = slash ? (size_t)(slash - rel) : strlen(rel);
        if (namelen == 0 || namelen >= sizeof(name)) continue;
        memcpy(name, rel, namelen);
        name[namelen] = '\0';

        /* 去重：已收集则跳过（同名的 tree 条目与更深路径只处理一次） */
        {
            int dup = 0;
            size_t k;
            for (k = 0; k < ecount; k++) {
                if (strcmp(entries[k].name, name) == 0) { dup = 1; break; }
            }
            if (dup) continue;
        }

        /* 展开该子项 */
        {
            char full[512];
            const LgFlatEntry *child;
            lg_tree_entry ent;
            unsigned char *sha_copy;   /* 每条目独立分配，避免悬垂指针 */
            char *name_copy;

            if (dir[0] != '\0') snprintf(full, sizeof(full), "%s/%s", dir, name);
            else snprintf(full, sizeof(full), "%s", name);

            child = LgFlatMapFind(map, full);

            sha_copy = (unsigned char *)malloc(LG_GIT_HASH_RAW_SIZE);
            if (sha_copy == NULL) goto cleanup;

            if (child != NULL && strcmp(child->type, "tree") != 0) {
                /* 文件 / submodule：直接用其 sha */
                ent.mode = LgParseMode(child->mode);
                lg_hex_decode(child->sha, sha_copy, LG_GIT_HASH_RAW_SIZE);
                ent.sha1 = sha_copy;
            } else {
                /* 目录：递归构建子树 */
                char subtree_sha[LG_GIT_HASH_HEX_SIZE + 1];
                if (LgBuildTreeLevel(store, map, full, subtree_sha) != 0) {
                    free(sha_copy);
                    goto cleanup;
                }
                ent.mode = LG_TREE_MODE_DIR;
                lg_hex_decode(subtree_sha, sha_copy, LG_GIT_HASH_RAW_SIZE);
                ent.sha1 = sha_copy;
            }

            name_copy = _strdup(name);
            if (name_copy == NULL) { free(sha_copy); goto cleanup; }
            ent.name = name_copy;

            if (ecount == ecap) {
                size_t ncap = ecap ? ecap * 2 : 16;
                lg_tree_entry *ne = (lg_tree_entry *)realloc(entries, ncap * sizeof(*ne));
                if (ne == NULL) { free(name_copy); free(sha_copy); goto cleanup; }
                entries = ne;
                ecap = ncap;
            }
            entries[ecount++] = ent;
        }
    }

    /* 序列化并计算 tree 哈希 */
    if (ecount > 0) {
        size_t need = lg_tree_content_size(entries, ecount);
        tree_buf = (unsigned char *)malloc(need ? need : 1);
        if (tree_buf == NULL) goto cleanup;
        if (lg_tree_build(entries, ecount, tree_buf, need, &tree_len) != 0) goto cleanup;
    }
    if (lg_object_id_compute(LG_OBJECT_TREE, tree_buf, tree_len, tree_sha) != 0) {
        goto cleanup;
    }

    /* 与远程同目录 tree 相同则复用（不再产生新对象） */
    if (remote != NULL && strcmp(remote->type, "tree") == 0 &&
        strcmp(remote->sha, tree_sha) == 0) {
        memcpy(out_sha, remote->sha, LG_GIT_HASH_HEX_SIZE + 1);
        result = 0;
        goto cleanup;
    }

    if (LgPStoreAdd(store, LG_OBJECT_TREE, tree_buf, tree_len, tree_sha) != 0) {
        goto cleanup;
    }
    memcpy(out_sha, tree_sha, LG_GIT_HASH_HEX_SIZE + 1);
    result = 0;

cleanup:
    for (i = 0; i < ecount; i++) {
        free((void *)entries[i].name);
        free((void *)entries[i].sha1);
    }
    free(entries);
    free(tree_buf);
    return result;
}

/* --------------------------------------------------------------------------
 * 读取整个文件（含大小检查）
 * -------------------------------------------------------------------------- */
static int LgReadFileWhole(const wchar_t *path, unsigned char **out, size_t *out_len,
                           char *err, size_t err_cap)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER sz;
    unsigned char *buf;
    DWORD read = 0;

    if (h == INVALID_HANDLE_VALUE) return -1;
    if (!GetFileSizeEx(h, &sz)) { CloseHandle(h); return -1; }

    if ((unsigned long long)sz.QuadPart > LG_MAX_FILE_SIZE) {
        snprintf(err, err_cap, "File exceeds 100 MiB limit.");
        CloseHandle(h);
        return -2;
    }

    buf = (unsigned char *)malloc((size_t)sz.QuadPart ? (size_t)sz.QuadPart : 1);
    if (buf == NULL) { CloseHandle(h); return -1; }

    if (sz.QuadPart > 0) {
        if (!ReadFile(h, buf, (DWORD)sz.QuadPart, &read, NULL) ||
            read != (DWORD)sz.QuadPart) {
            free(buf);
            CloseHandle(h);
            return -1;
        }
    }
    CloseHandle(h);
    *out = buf;
    *out_len = (size_t)sz.QuadPart;
    return 0;
}

/* 判断 path 是否在显式选中列表内 */
static int LgIsSelected(const char *path, const char *const *list, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (list[i] != NULL && strcmp(list[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * 发布主流程
 * -------------------------------------------------------------------------- */
int lg_publish(const lg_publish_input *in, lg_publish_result *out)
{
    lg_scan_result scan;
    LgGitignore *gi = NULL;
    LgPStore store;
    LgFlatMap map;
    lg_remote_ref *refs = NULL;
    size_t ref_count = 0;
    const char *old_oid = "0000000000000000000000000000000000000000";
    const char *parent_oid = NULL;   /* NULL = root commit */
    lg_github_tree remote_tree;
    lg_github_error gerr;
    char root_tree_sha[LG_GIT_HASH_HEX_SIZE + 1];
    lg_commit_author author;
    char *commit_content = NULL;
    size_t commit_len = 0;
    char commit_oid[LG_GIT_HASH_HEX_SIZE + 1];
    lg_pack_object *pobjs = NULL;
    unsigned char *pack = NULL;
    size_t pack_len = 0;
    lg_push_result pres;
    lg_manifest new_manifest;
    int uploaded = 0;
    size_t i;
    int rc = -1;

    memset(&scan, 0, sizeof(scan));
    memset(&store, 0, sizeof(store));
    LgFlatMapInit(&map);
    memset(&remote_tree, 0, sizeof(remote_tree));
    memset(&new_manifest, 0, sizeof(new_manifest));

    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->success = 0;
    }
    if (in == NULL || in->token == NULL || in->owner == NULL || in->repo == NULL ||
        in->branch == NULL || in->local_dir == NULL || in->commit_message == NULL ||
        out == NULL) {
        return -1;
    }

    /* 1. 扫描本地目录 */
    if (lg_scan_directory(in->local_dir, &scan) != 0) {
        snprintf(out->error, sizeof(out->error), "Failed to scan directory.");
        return -1;
    }

    /* 2. 解析 .gitignore */
    if (in->gitignore_text != NULL && in->gitignore_text[0] != '\0') {
        gi = lg_gitignore_parse(in->gitignore_text);
    }

    /* 3. 发现远程引用，确定目标分支状态 */
    {
        char derr[256];
        if (lg_transport_discover(in->owner, in->repo, NULL, in->token,
                                  &refs, &ref_count, derr, sizeof(derr)) != 0) {
            snprintf(out->error, sizeof(out->error),
                     "Failed to discover remote refs: %s", derr[0] ? derr : "unknown reason");
            goto cleanup;
        }
    }
    for (i = 0; i < ref_count; i++) {
        char full[300];
        snprintf(full, sizeof(full), "refs/heads/%s", in->branch);
        if (strcmp(refs[i].name, full) == 0) {
            old_oid = refs[i].oid;
            parent_oid = refs[i].oid;
        }
    }

    /* 4. 读取远程树（若非空仓库） */
    if (parent_oid != NULL) {
        if (lg_github_get_tree_recursive(in->token, in->owner, in->repo, in->branch,
                                         &remote_tree, &gerr) != 0) {
            snprintf(out->error, sizeof(out->error), "Read remote tree failed: %s", gerr.message);
            goto cleanup;
        }
        if (remote_tree.truncated) {
            snprintf(out->error, sizeof(out->error),
                     "Remote tree too large (truncated). Not supported yet.");
            goto cleanup;
        }
        for (i = 0; i < remote_tree.count; i++) {
            lg_github_tree_entry *e = &remote_tree.entries[i];
            LgFlatMapUpsert(&map, e->path, e->mode, e->type, e->sha);
        }
    }

    /* 5. 计算本地 blob 哈希，确定选中文件，更新合并 map */
    for (i = 0; i < scan.count; i++) {
        lg_scanned_file *f = &scan.files[i];
        wchar_t full_path[MAX_PATH * 2];
        char blob_sha[LG_GIT_HASH_HEX_SIZE + 1];
        unsigned char *content = NULL;
        size_t content_len = 0;
        int selected = 0;
        const char *old_hash;
        const LgFlatEntry *remote_entry;

        /* 应用 .gitignore */
        if (gi != NULL && lg_gitignore_match(gi, f->path, 0)) {
            continue;
        }

        /* 构建完整宽路径：local_dir + "\\" + 相对路径（UTF-8 → 宽字符） */
        {
            wchar_t rel[MAX_PATH];
            int rn = MultiByteToWideChar(CP_UTF8, 0, f->path, -1, rel, MAX_PATH);
            if (rn <= 0) {
                snprintf(out->error, sizeof(out->error), "Invalid path encoding.");
                goto cleanup;
            }
            swprintf(full_path, MAX_PATH * 2, L"%s\\%s", in->local_dir, rel);
        }

        if (LgReadFileWhole(full_path, &content, &content_len,
                            out->error, sizeof(out->error)) != 0) {
            goto cleanup;
        }
        if (lg_blob_hash_memory(content, content_len, blob_sha) != 0) {
            free(content);
            goto cleanup;
        }

        if (in->selected_paths != NULL) {
            /* 显式选择：仅发布用户勾选的文件 */
            selected = LgIsSelected(f->path, in->selected_paths, in->selected_count);
        } else {
            /* 默认：发布所有“相对 manifest 有变化、且相对远程有变化”的文件 */
            old_hash = (in->manifest != NULL) ? lg_manifest_find(in->manifest, f->path) : NULL;
            if (old_hash == NULL || strcmp(old_hash, blob_sha) != 0) {
                selected = 1;
            }

            /* 与远程比较：若远程已有相同 blob，则实际无变化，跳过 */
            remote_entry = LgFlatMapFind(&map, f->path);
            if (remote_entry != NULL && strcmp(remote_entry->type, "blob") == 0 &&
                strcmp(remote_entry->sha, blob_sha) == 0) {
                selected = 0;
            }
        }

        if (selected) {
            if (LgPStoreAdd(&store, LG_OBJECT_BLOB, content, content_len, blob_sha) != 0) {
                free(content);
                goto cleanup;
            }
            LgFlatMapUpsert(&map, f->path, "100644", "blob", blob_sha);
            uploaded++;
        }

        /* 记录到新 manifest（所有非忽略的本地文件） */
        lg_manifest_set(&new_manifest, f->path, blob_sha);
        free(content);
    }

    /* 无变化：不创建空提交 */
    if (uploaded == 0) {
        out->success = 1;
        out->no_changes = 1;
        rc = 0;
        goto cleanup;
    }

    /* 6. 构建合并后的根树 */
    if (LgBuildTreeLevel(&store, &map, "", root_tree_sha) != 0) {
        snprintf(out->error, sizeof(out->error), "Failed to build tree.");
        goto cleanup;
    }

    /* 7. 创建提交 */
    author.name = in->author_name ? in->author_name : "LightGit";
    author.email = in->author_email ? in->author_email : "lightgit@users.noreply.github.com";
    author.timestamp = (long long)time(NULL);
    author.tz_offset = "+0800";

    if (lg_commit_build(root_tree_sha, parent_oid, &author, &author,
                        in->commit_message, &commit_content, &commit_len) != 0) {
        snprintf(out->error, sizeof(out->error), "Failed to build commit.");
        goto cleanup;
    }
    if (lg_object_id_compute(LG_OBJECT_COMMIT, commit_content, commit_len, commit_oid) != 0) {
        goto cleanup;
    }
    if (LgPStoreAdd(&store, LG_OBJECT_COMMIT,
                    (const unsigned char *)commit_content, commit_len, commit_oid) != 0) {
        goto cleanup;
    }

    /* 8. 生成 packfile */
    pobjs = (lg_pack_object *)calloc(store.count ? store.count : 1, sizeof(*pobjs));
    if (pobjs == NULL) goto cleanup;
    for (i = 0; i < store.count; i++) {
        pobjs[i].type = store.items[i].type;
        pobjs[i].content = store.items[i].content;
        pobjs[i].content_len = store.items[i].len;
    }
    if (lg_pack_generate(pobjs, store.count, &pack, &pack_len) != 0) {
        snprintf(out->error, sizeof(out->error), "Failed to generate packfile.");
        goto cleanup;
    }

    /* 9. 推送 */
    if (lg_transport_push(in->owner, in->repo, in->branch,
                          old_oid, commit_oid, pack, pack_len,
                          NULL, in->token, &pres) != 0) {
        snprintf(out->error, sizeof(out->error),
                 "Push request failed: %s", pres.error[0] ? pres.error : "unknown reason");
        goto cleanup;
    }
    if (!pres.success) {
        snprintf(out->error, sizeof(out->error), "%s", pres.error);
        goto cleanup;
    }

    /* 10. 成功：更新 manifest（原地替换为新状态） */
    if (in->manifest != NULL) {
        lg_manifest_clear(in->manifest);
        for (i = 0; i < new_manifest.count; i++) {
            lg_manifest_set(in->manifest, new_manifest.entries[i].path,
                            new_manifest.entries[i].blob_sha);
        }
    }

    out->success = 1;
    out->files_uploaded = uploaded;
    memcpy(out->commit_oid, commit_oid, LG_GIT_HASH_HEX_SIZE + 1);
    rc = 0;

cleanup:
    lg_manifest_clear(&new_manifest);
    free(pack);
    free(pobjs);
    free(commit_content);
    free(refs);
    lg_github_tree_free(&remote_tree);
    LgFlatMapFree(&map);
    LgPStoreFree(&store);
    lg_gitignore_free(gi);
    lg_scan_result_free(&scan);
    return rc;
}
