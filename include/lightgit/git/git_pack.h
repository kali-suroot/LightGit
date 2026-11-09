/*
 * git_pack.h — Git Packfile 生成器
 *
 * Packfile 格式（pack-format）：
 *
 *   头     : "PACK" + 版本号(2, 大端) + 对象数(大端)         共 12 字节
 *   对象区 : 每个对象 = 变长对象头 + zlib 压缩的对象内容
 *   尾     : 20 字节 SHA-1（对前面所有字节）
 *
 * 变长对象头：首字节 bit7=后续标志, bits6..4=对象类型(3位), bits3..0=size 低 4 位；
 *             后续字节 bit7=后续标志, bits6..0=size 的后续 7 位（小端顺序）。
 *
 * 说明：第一版只生成“完整对象”packfile（不做 delta 压缩），完全合法。
 */
#ifndef LIGHTGIT_GIT_GIT_PACK_H
#define LIGHTGIT_GIT_GIT_PACK_H

#include <stddef.h>
#include "lightgit/git/git_object.h"

#define LG_PACK_HEADER_SIZE  12
#define LG_PACK_TRAILER_SIZE LG_GIT_HASH_RAW_SIZE

/* 待打包的单个对象（content 为不含 "type len\0" 头的原始对象内容） */
typedef struct lg_pack_object {
    lg_object_type      type;
    const unsigned char *content;
    size_t              content_len;
} lg_pack_object;

/*
 * 生成 packfile 到内存缓冲区。
 * objects / count : 按顺序打包的对象列表。
 * out_pack        : 成功时指向 malloc 的 packfile 缓冲区（调用方负责 free）。
 * out_len         : 成功时为 packfile 总字节数。
 *
 * 成功返回 0，失败返回 -1。
 */
int lg_pack_generate(const lg_pack_object *objects, size_t count,
                     unsigned char **out_pack, size_t *out_len);

#endif /* LIGHTGIT_GIT_GIT_PACK_H */
