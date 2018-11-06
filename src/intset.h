/*
 * Copyright (c) 2009-2012, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef __INTSET_H
#define __INTSET_H
#include <stdint.h>

/*
- 有序整数集合，使用连续内存存储，所以每次插入和删除都会触发内存的重新分配
- 适合数量较小的时候，插入和删除不频繁的时候使用，不太影响效率的同时节省内存，同时可以利用它的有序性进行高效的查找
*/

typedef struct intset {
    uint32_t encoding;          // 当前集合的编码格式，例如INT16, INT32, INT64，可以理解为每个元素的大小
    uint32_t length;            // set长度
    int8_t contents[];
} intset;

intset *intsetNew(void);        // 创建一个空的intset，长度为0，encode为INT16
intset *intsetAdd(intset *is, int64_t value, uint8_t *success);     // 插入一个元素，如果value比原来编码大，就会扩大原来的编码
intset *intsetRemove(intset *is, int64_t value, int *success);      // 移除一个元素
uint8_t intsetFind(intset *is, int64_t value);                      // 查找一个元素，找不到返回0，找到返回1
int64_t intsetRandom(intset *is);                                   // 从set中随机取一个元素，如果length为0，会导致宕机
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);        // 从指定位置中取一个元素，如果pos在范围内，返回1，否则返回0
uint32_t intsetLen(intset *is);                                     // 返回当前set的长度
size_t intsetBlobLen(intset *is);                                   // 返回当前set的内存占用空间大小，单位Byte

#ifdef REDIS_TEST
int intsetTest(int argc, char *argv[]);
#endif

#endif // __INTSET_H
