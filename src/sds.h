/* SDSLib 2.0 -- A C dynamic strings library
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SDS_H
#define __SDS_H

#define SDS_MAX_PREALLOC (1024*1024)

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

// redis字符串是一个任何时候都以'\0'结束，并且可以兼容二进制的字符串
// 实现方式是在实际操作的字符串指针前面有一个描述该字符串信息的header
// 为了节约内存，redis按照字符串长度，会使用不同的header结构体来存放信息
// 这些结构体分别为sdshdr5, sdshdr8, sdshdr16, sdshdr32, sdshdr64，后面的数字即为该结构体能存放的字符串长度二进制位数
// 例如，sdshdr8表示该结构体能容纳2^8-1长度的字符串
// 如果需要知道该字符串使用的header是什么类型，则只需要获取指针之前一位的flags，因为所有header结构buf前面一个字段都为flags
// 从flags知道是什么类型的header之后，就可以根据header长度，获取到指向header的指针进行操作
// 除了sdshdr5外，其他类型都有相似的变量，len表示实际字符串长度，alloc表示实际分配的长度
// sdshdr5不会预先分配长度

typedef char *sds;

/* Note: sdshdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
struct __attribute__ ((__packed__)) sdshdr5 {
    unsigned char flags; // 低三位存放type，高5位存放长度
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

// sds类型，分别代表使用什么类型的header描述
#define SDS_TYPE_5  0
#define SDS_TYPE_8  1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4
#define SDS_TYPE_MASK 7
#define SDS_TYPE_BITS 3

#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (void*)((s)-(sizeof(struct sdshdr##T)));    // 获取header指针
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))                 // 获取字符串指针
#define SDS_TYPE_5_LEN(f) ((f)>>SDS_TYPE_BITS)          // 用于计算SDS_TYPE_5的实际长度

// 根据类型获取字符串长度
static inline size_t sdslen(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->len;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->len;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->len;
    }
    return 0;
}

// 返回字符串剩余未使用的空间
static inline size_t sdsavail(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            return 0;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

// 设置字符串长度，注意，这里未进行任何安全性检查，直接赋值
static inline void sdssetlen(sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char*)s)-1;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len = newlen;
            break;
    }
}

// 增加字符串长度，注意，这里未进行任何安全性检查，直接增加
static inline void sdsinclen(sds s, size_t inc) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char*)s)-1;
                unsigned char newlen = SDS_TYPE_5_LEN(flags)+inc;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len += inc;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len += inc;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len += inc;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len += inc;
            break;
    }
}

// 返回字符串实际分配的大小，SDS_TYPE_5类型不会有预留空间，所以之间返回长度
static inline size_t sdsalloc(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->alloc;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->alloc;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->alloc;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->alloc;
    }
    return 0;
}

// 设置字符串的分配大小，SDS_TYPE_5类型不会有预留空间，所以直接忽略
static inline void sdssetalloc(sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->alloc = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->alloc = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->alloc = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

sds sdsnewlen(const void *init, size_t initlen);    // 根据传入的init指针，和initlen创建一个合适的sds，为了兼容c字符串函数，sds总是会以'\0'结尾
sds sdsnew(const char *init);                       // 直接根据传入的init字符串指针，创建合适的sds
sds sdsempty(void);                                 // 创建一个空的sds
sds sdsdup(const sds s);                            // 复制一个sds
void sdsfree(sds s);                                // 释放sds内存
sds sdsgrowzero(sds s, size_t len);                 // 增长s到能容纳len长度，增长的空间初始化为0，并且更新s长度为len，如果s实际已经比len长，则不进行任何操作
sds sdscatlen(sds s, const void *t, size_t len);    // 拼接函数
sds sdscat(sds s, const char *t);
sds sdscatsds(sds s, const sds t);
sds sdscpylen(sds s, const char *t, size_t len);    // 拷贝函数
sds sdscpy(sds s, const char *t);

// 格式化fmt，并拼接到s中
sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

sds sdscatfmt(sds s, char const *fmt, ...);         // redis自定义的简化版格式化fmt并拼接到s，速度会比*printf快很多
sds sdstrim(sds s, const char *cset);               // 去除头尾指定的字符集合
void sdsrange(sds s, int start, int end);           // 用s的start到end重新赋值s，start和end可以为负值
void sdsupdatelen(sds s);                           // 使用strlen(s)更新s长度
void sdsclear(sds s);                               // 清空s，只设置了下长度为0，不会释放内存
int sdscmp(const sds s1, const sds s2);             // 比较两个sds
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);      // 分割字符串到返回的sds数组中，数组长度在count中返回。二进制安全
void sdsfreesplitres(sds *tokens, int count);       // 释放sdssplitlen和sdssplitargs返回的sds*数组
void sdstolower(sds s);                             // 转为小写
void sdstoupper(sds s);                             // 转为大写
sds sdsfromlonglong(long long value);               // 从long long类型初始化sds
sds sdscatrepr(sds s, const char *p, size_t len);   // 将p对应的字符串转义后用'"'包裹后，连接到s中
sds *sdssplitargs(const char *line, int *argc);     // 分割命令行到返回的sds数组中，数组长度在argc参数返回
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);    // 替换sds中的字符，如果s[i] = from[j] 则将s[i]替换为to[j]
sds sdsjoin(char **argv, int argc, char *sep);      // 拼接C风格字符串数组到新的sds中
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen);    // 拼接sds数组到新的sds中

/* Low level functions exposed to the user API */
sds sdsMakeRoomFor(sds s, size_t addlen);           // 扩容s，使能再容纳addlen长度，只会修改alloc参数，不会改变len
void sdsIncrLen(sds s, int incr);                   // 增加s长度，需要跟sdsMakeRoomFor配合使用
sds sdsRemoveFreeSpace(sds s);                      // 移除s所有分配的预留空间
size_t sdsAllocSize(sds s);                         // s实际占用的内存大小
void *sdsAllocPtr(sds s);                           // 返回s header指针

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);

#ifdef REDIS_TEST
int sdsTest(int argc, char *argv[]);
#endif

#endif
