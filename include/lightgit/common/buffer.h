/*
 * buffer.h — 动态可增长字节缓冲区
 *
 * 供网络传输、pkt-line 解析等模块复用：在不确定最终大小时持续追加字节，
 * 内部按需扩容（2 倍增长）。
 */
#ifndef LIGHTGIT_COMMON_BUFFER_H
#define LIGHTGIT_COMMON_BUFFER_H

#include <stddef.h>

typedef struct lg_buffer {
    unsigned char *data;   /* 数据指针（可能为 NULL，当 len==0 且尚未分配） */
    size_t         len;    /* 已用字节数 */
    size_t         cap;    /* 已分配容量 */
} lg_buffer;

/* 初始化为空 */
void lg_buffer_init(lg_buffer *b);

/* 释放内部内存并重置为空 */
void lg_buffer_free(lg_buffer *b);

/* 确保能再容纳 extra 字节（内部按需扩容） */
int lg_buffer_reserve(lg_buffer *b, size_t extra);

/* 追加 len 字节（len==0 为无操作） */
int lg_buffer_append(lg_buffer *b, const void *data, size_t len);

#endif /* LIGHTGIT_COMMON_BUFFER_H */
