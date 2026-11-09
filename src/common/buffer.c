/*
 * buffer.c — 动态可增长字节缓冲区实现
 */
#include "lightgit/common/buffer.h"

#include <stdlib.h>
#include <string.h>

void lg_buffer_init(lg_buffer *b)
{
    if (b == NULL) {
        return;
    }
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void lg_buffer_free(lg_buffer *b)
{
    if (b == NULL) {
        return;
    }
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

int lg_buffer_reserve(lg_buffer *b, size_t extra)
{
    size_t need, newcap;
    unsigned char *nd;

    if (b == NULL) {
        return -1;
    }

    need = b->len + extra;
    if (need <= b->cap) {
        return 0;
    }

    newcap = (b->cap == 0) ? 4096 : b->cap;
    while (newcap < need) {
        newcap *= 2;
    }

    nd = (unsigned char *)realloc(b->data, newcap);
    if (nd == NULL) {
        return -1;
    }
    b->data = nd;
    b->cap = newcap;
    return 0;
}

int lg_buffer_append(lg_buffer *b, const void *data, size_t len)
{
    if (b == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (lg_buffer_reserve(b, len) != 0) {
        return -1;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    return 0;
}
