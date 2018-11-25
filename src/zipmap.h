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
// 为了节约内存，redis提供了zipmap这样的结构来存放key-value类型数据。
// 这个结构只在内存上有优势，在查找/插入/删除/更新上跟hash表或者红黑树实现的map没有任何优势。所以只适用于元素数量比较少的情况。
// zipmap实际上是一个char数组，使用约定好的格式来存放这些key-value
// 下面是一个zipmap，存放"foo" => "bar", "hello" => "world"两个键值对
// <zmlen><len>"foo"<len><free>"bar"<len>"hello"<len><free>"world"<ZIPMAP_END>
// zmlen：1 byte：存放zipmap拥有的key-value对数量，最大为253，如果zmlen>=254，则表示该值无效，需要遍历整个zipmap才能拿到元素数量
// len：1/5 byte：存放后面key/value的长度，第一个byte值<254则该byte就是接下去的长度，如果=254则接下去4 byte组成的unsinged int才是长度，如果=255(ZIPMAP_END)，则表示到达zipmap尾部
// free：1 byte：存放value之后剩余的空间长度，有这个字段的好处就是，value长度改变可以利用剩余的空间来减少内存重复申请的次数，该字段为1 byte，所以剩余空间只能<=255，如果剩余空间大于这个范围，就会强制重新申请整个zipmap的内存
// 对于上面例子，实际的二进制数值为：
// "\x02\x03foo\x03\x00bar\x05hello\x05\x00world\xff"
// 如果将foo => bar改为 foo => hi，则二进制数值为
// \x02\x03foo\x02\x01hir\x05hello\x05\x00world\xff
// 可以看到foo对于的value len更新为2，free更新为1，只是利用更改长度的方式避免了重新申请整个zipmap的内存

unsigned char *zipmapNew(void);             // 创建一个空的zipmap内存，只有两个byte：\x00\xff
// 设置key和value到zipmap中，如果key不存在就会创建一个，否则更新key对应的value；参数update用于返回是更新操作还是插入操作
unsigned char *zipmapSet(unsigned char *zm, unsigned char *key, unsigned int klen, unsigned char *val, unsigned int vlen, int *update);
// 从zipmap中删除key；deleted用来返回是否删除元素
unsigned char *zipmapDel(unsigned char *zm, unsigned char *key, unsigned int klen, int *deleted);
// 在迭代之前调用一起，类似于其他结构返回一个迭代器指针
unsigned char *zipmapRewind(unsigned char *zm);
// 返回下一个key，value的entry，key，klen，value，vlen用来返回当前迭代到的key，value值；如果已经到尾部，则返回NULL
// 返回的key和value只是简单指向到zipmap内部的内存
unsigned char *zipmapNext(unsigned char *zm, unsigned char **key, unsigned int *klen, unsigned char **value, unsigned int *vlen);
// 搜索zipmap中key对应的的value，如果找到返回1，找不到返回0
// 返回的value只是简单指向到zipmap内部的内存
int zipmapGet(unsigned char *zm, unsigned char *key, unsigned int klen, unsigned char **value, unsigned int *vlen);
// 搜索zipmap中是否存在该key，如果找到返回1，找不到返回0
int zipmapExists(unsigned char *zm, unsigned char *key, unsigned int klen);
// 返回zipmap键值对个数
unsigned int zipmapLen(unsigned char *zm);
// 返回zipmap占用的二进制内存大小
size_t zipmapBlobLen(unsigned char *zm);
void zipmapRepr(unsigned char *p);      // for test

#ifdef REDIS_TEST
int zipmapTest(int argc, char *argv[]);
#endif

#endif
