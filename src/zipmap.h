/* String -> String Map data structure optimized for size.
 *
 * See zipmap.c for more info.
 *
 * --------------------------------------------------------------------------
 *
 * Copyright (c) 2009-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef _ZIPMAP_H
#define _ZIPMAP_H

// 压缩键值对存储
// 为了节约内存，redis提供了zipmap这样的结构来存放key-value类型数据
// zipmap实际上是一个char数组，使用约定好的格式来存放这些key-value
// 下面是一个zipmap，存放"foo" => "bar", "hello" => "world"两个键值对
// <zmlen><len>"foo"<len><free>"bar"<len>"hello"<len><free>"world"<ZIPMAP_END>
// zmlen：1 byte：存放zipmap拥有的key-value对数量，最大为253，如果zmlen>=254，则表示该值无效，需要遍历整个zipmap才能拿到元素数量
// len：1/5 byte：存放后面key/value的长度，第一个byte值<254则该byte就是接下去的长度，如果=254则接下去4 byte组成的unsinged int才是长度，如果=255，则表示到达zipmap尾部
// free：1 byte：

unsigned char *zipmapNew(void);
unsigned char *zipmapSet(unsigned char *zm, unsigned char *key, unsigned int klen, unsigned char *val, unsigned int vlen, int *update);
unsigned char *zipmapDel(unsigned char *zm, unsigned char *key, unsigned int klen, int *deleted);
unsigned char *zipmapRewind(unsigned char *zm);
unsigned char *zipmapNext(unsigned char *zm, unsigned char **key, unsigned int *klen, unsigned char **value, unsigned int *vlen);
int zipmapGet(unsigned char *zm, unsigned char *key, unsigned int klen, unsigned char **value, unsigned int *vlen);
int zipmapExists(unsigned char *zm, unsigned char *key, unsigned int klen);
unsigned int zipmapLen(unsigned char *zm);
size_t zipmapBlobLen(unsigned char *zm);
void zipmapRepr(unsigned char *p);

#ifdef REDIS_TEST
int zipmapTest(int argc, char *argv[]);
#endif

#endif
