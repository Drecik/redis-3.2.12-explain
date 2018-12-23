/* quicklist.h - A generic doubly linked quicklist implementation
 *
 * Copyright (c) 2014, Matt Stancliff <matt@genges.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this quicklist of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this quicklist of conditions and the following disclaimer in the
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

#ifndef __QUICKLIST_H__
#define __QUICKLIST_H__

/* Node, quicklist, and Iterator are the only data structures used currently. */

/* quicklistNode is a 32 byte struct describing a ziplist for a quicklist.
 * We use bit fields keep the quicklistNode at 32 bytes.
 * count: 16 bits, max 65536 (max zl bytes is 65k, so max count actually < 32k). 存放该节点的元素个数
 * encoding: 2 bits, RAW=1, LZF=2. 表示当前节点的数据是以什么编码的，RAW：原始数据，LZF：通过LZF压缩
 * container: 2 bits, NONE=1, ZIPLIST=2. 表示当前节点通过什么类型数据结构存储的，NONE：没有数据，ZIPLIST：通过ziplist存储
 * recompress: 1 bit, bool, 如果该值为true，表示只是临时存放的数据被解压了，需要再次被压缩
 * attempted_compress: 1 bit, boolean, used for verifying during testing. 用于测试
 * extra: 12 bits, free for future use; pads out the remainder of 32 bits 暂未使用*/
typedef struct quicklistNode {
    struct quicklistNode *prev;
    struct quicklistNode *next;
    unsigned char *zl;
    unsigned int sz;             /* ziplist size in bytes */
    unsigned int count : 16;     /* count of items in ziplist */
    unsigned int encoding : 2;   /* RAW==1 or LZF==2 */
    unsigned int container : 2;  /* NONE==1 or ZIPLIST==2 */
    unsigned int recompress : 1; /* was this node previous compressed? */
    unsigned int attempted_compress : 1; /* node can't compress; too small */
    unsigned int extra : 10; /* more bits to steal for future usage */
} quicklistNode;

// quicklistNode中的ziplist压缩之后使用该结构体存储，quicklistNode将会指向该结构体
typedef struct quicklistLZF {
    unsigned int sz;        // 通过LZF压缩后的数据长度
    char compressed[];      // 压缩后的数据
} quicklistLZF;

/* quicklist is a 32 byte struct (on 64-bit systems) describing a quicklist.
 * 'count': quiklist中所有元素数量
 * 'len'：quicklist中quicklistNode数量
 * 'compress' is: 0 if compression disabled, otherwise it's the number
 *                of quicklistNodes to leave uncompressed at ends of quicklist.
 *                作用：如果非0，表示头尾多少个节点不需要压缩，例如如果是2的话，表示除头尾各2个quicklistNode不被压缩外，其他都会被压缩，以减少内存
 * 'fill' is the user-requested (or default) fill factor. 
 *                作用：控制每个quicklistNode中的ziplist大小，如果为正值，则表示以ziplist中元素个数为限制，但总的ziplist占用内存不能大于8K，
 *                      如果未负值，则以ziplist占用内存大小来控制，-1：4k，-2：8k(redis默认)，-3：16k，-4：32k，-5，64k*/
typedef struct quicklist {
    quicklistNode *head;
    quicklistNode *tail;
    unsigned long count;        /* total count of all entries in all ziplists */
    unsigned int len;           /* number of quicklistNodes */
    int fill : 16;              /* fill factor for individual nodes */
    unsigned int compress : 16; /* depth of end nodes not to compress;0=off */
} quicklist;

// quicklist迭代器
typedef struct quicklistIter {
    const quicklist *quicklist;
    quicklistNode *current;
    unsigned char *zi;              // ziplist中当前的entry地址
    long offset;                    // ziplist中当前entry的偏移量
    int direction;                  // 迭代方向
} quicklistIter;

// quicklist中的一个元素，实际指向的是ziplist中的一个entry，及该entry的值
typedef struct quicklistEntry {
    const quicklist *quicklist;
    quicklistNode *node;        // 当前quicklistNode
    unsigned char *zi;          // ziplist当前的entry地址
    unsigned char *value;       // ziplist中字符串的value值
    long long longval;          // ziplist中整数值
    unsigned int sz;            // ziplist中字符串的value长度
    int offset;                 // ziplist中的offset
} quicklistEntry;

#define QUICKLIST_HEAD 0
#define QUICKLIST_TAIL -1

/* quicklist node encodings */
#define QUICKLIST_NODE_ENCODING_RAW 1
#define QUICKLIST_NODE_ENCODING_LZF 2

/* quicklist compression disable */
#define QUICKLIST_NOCOMPRESS 0

/* quicklist container formats */
#define QUICKLIST_NODE_CONTAINER_NONE 1
#define QUICKLIST_NODE_CONTAINER_ZIPLIST 2

#define quicklistNodeIsCompressed(node)                                        \
    ((node)->encoding == QUICKLIST_NODE_ENCODING_LZF)

// 创建一个空的quicklist，需要通过quicklistRelease释放
quicklist *quicklistCreate(void);

// 通过自定义fill和compress创建quicklist，同样需要通过quicklistRelease释放
quicklist *quicklistNew(int fill, int compress);

// 设置quicklist depth属性
void quicklistSetCompressDepth(quicklist *quicklist, int depth);

// 设置quicklist fill属性
void quicklistSetFill(quicklist *quicklist, int fill);

// 设置quiicklist fill和depth属性
void quicklistSetOptions(quicklist *quicklist, int fill, int depth);

// 释放整个quicklist内存
void quicklistRelease(quicklist *quicklist);

// 将value插入到quicklist头部
// 如果创建新的quicklistNode返回1，否则0
int quicklistPushHead(quicklist *quicklist, void *value, const size_t sz);

// 将value插入到quicklist尾部
int quicklistPushTail(quicklist *quicklist, void *value, const size_t sz);

// 插入value到头部/尾部，通过where控制，QUICKLIST_HEAD/QUICKLIST_TAIL
void quicklistPush(quicklist *quicklist, void *value, const size_t sz,
                   int where);

// 创建新的quicklistNode来存放追加的ziplist，在rdb中使用
void quicklistAppendZiplist(quicklist *quicklist, unsigned char *zl);

// 将ziplist所有元素追加到quicklist中，并释放ziplist内存
quicklist *quicklistAppendValuesFromZiplist(quicklist *quicklist,
                                            unsigned char *zl);

// 通过ziplist所有元素创建一个新的quicklist，会释放原来的ziplist
quicklist *quicklistCreateFromZiplist(int fill, int compress,
                                      unsigned char *zl);

// 在元素之后插入value
void quicklistInsertAfter(quicklist *quicklist, quicklistEntry *node,
                          void *value, const size_t sz);

// 在元素之前插入value
void quicklistInsertBefore(quicklist *quicklist, quicklistEntry *node,
                           void *value, const size_t sz);

// 删除指定元素，iter按照迭代方向返回下一个元素
void quicklistDelEntry(quicklistIter *iter, quicklistEntry *entry);

// 替换quicklist中index下标的元素，替换成功返回1，否则返回0
int quicklistReplaceAtIndex(quicklist *quicklist, long index, void *data,
                            int sz);

// 删除quicklist中一定范围的所有元素，删除成功返回1，失败返回0
int quicklistDelRange(quicklist *quicklist, const long start, const long stop);

// 创建一个迭代器，该迭代器的方向通过direction指定，AL_START_HEAD/AL_START_TAIL
quicklistIter *quicklistGetIterator(const quicklist *quicklist, int direction);

// 从index开始创建一个迭代器，方向由direction指定，AL_START_HEAD/AL_START_TAIL，如果index无效返回NULL
quicklistIter *quicklistGetIteratorAtIdx(const quicklist *quicklist,
                                         int direction, const long long idx);

// 获取迭代器下一个元素，返回0表示没有元素，否则获取下个元素成功
int quicklistNext(quicklistIter *iter, quicklistEntry *node);

// 释放迭代器内存，如果当前迭代器指向了一个quicklistNode，则尝试重新压缩他
void quicklistReleaseIterator(quicklistIter *iter);

// 从orig中复制整一个quicklist
quicklist *quicklistDup(quicklist *orig);

// 获取quicklist中index为下标的元素的值，存储在entry中返回
// index>=0表示从头开始，<0表示从尾部开始
// 找到返回1，否则返回0
int quicklistIndex(const quicklist *quicklist, const long long index,
                   quicklistEntry *entry);

// 下面两个函数没有被实现
void quicklistRewind(quicklist *quicklist, quicklistIter *li);
void quicklistRewindTail(quicklist *quicklist, quicklistIter *li);

// 旋转quicklist，将最后一个元素插入到头部
void quicklistRotate(quicklist *quicklist);

// 从quicklist头部/尾部pop元素，头部/尾部通过where控制，返回值根据是不是整形，通过data/sval返回。
// 如果pop的元素不是整形，则会调用saver函数用来存储该元素的内容，返回值将会赋值给data，因为pop之后quicklist中该元素的内容将会被清除
// 所以需要调用用户自定义的函数进行保存该内容
// 返回0表示没有元素，返回1表示pop元素成功，可以使用data/sval
// 如果返回1，且data为NULL，表示该元素是个整形，可以使用sval，否则的话data和sz表示当前的元素内容
int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data,
                       unsigned int *sz, long long *sval,
                       void *(*saver)(unsigned char *data, unsigned int sz));

// 默认quicklistpop函数（不可以自定义saver，其他跟quicklistPopCustom一样）
int quicklistPop(quicklist *quicklist, int where, unsigned char **data,
                 unsigned int *sz, long long *slong);

// 返回quicklist总的元素个数
unsigned int quicklistCount(quicklist *ql);

// 比较p指向的ziplist的entry的内容与指定值相等，不等返回0，否则返回1
int quicklistCompare(unsigned char *p1, unsigned char *p2, int p2_len);

// 获取quicklistNode中的lzf原始数据
// data存储lzf压缩后的数据，返回值为压缩后数据的长度
size_t quicklistGetLzf(const quicklistNode *node, void **data);

#ifdef REDIS_TEST
int quicklistTest(int argc, char *argv[]);
#endif

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __QUICKLIST_H__ */
