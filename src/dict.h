/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

// hash表的一个节点
typedef struct dictEntry {
    void *key;              // 节点key
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;                    // 节点value，使用了union来节省内存同时方便访问
    struct dictEntry *next; // hash冲突的时候使用链表来串联起所有冲突的节点
} dictEntry;

// 用来存放某些自定义函数的结构体，实现hash表多态功能
typedef struct dictType {
    unsigned int (*hashFunction)(const void *key);                          // hash计算函数
    void *(*keyDup)(void *privdata, const void *key);                       // key复制函数
    void *(*valDup)(void *privdata, const void *obj);                       // value复制函数
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);  // key比较函数
    void (*keyDestructor)(void *privdata, void *key);                       // key销毁函数
    void (*valDestructor)(void *privdata, void *obj);                       // value销毁函数
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
// hash表结构
typedef struct dictht {
    dictEntry **table;          // entry数组
    unsigned long size;         // 数组大小
    unsigned long sizemask;     // 数组掩码，一般为size-1，用于取模计算hash表桶的下标
    unsigned long used;         // 当前存放的entry数量
} dictht;

// 对外的hash表结构
// 因为redish是单线程模型，而rehash是一个比较耗时的操作，redis使用了分步的操作来使这一比较复杂的过程分摊到每次操作该hash表的时候
// 内部有两个dictht，常规情况下数据都是放在ht[0]中，当触发rehash时，先将ht[1]申请合适大小的hash表，并将rehashidx置为0
// 在每次访问或者更新该hash表的时候会触发rehash操作：
// 1. 检测是否有正在遍历的safe iterator
// 2. 如果有直接返回不进行rehash迭代
// 3. 从上次rehashidx位置开始往后遍历一个不为空的桶，对这个桶中所有entry进行rehash操作
// 4. 如果遍历的时空的桶，则会计算累计遍历空的桶是否超过10个，超过则进行返回
// 5. 查看是否还有元素需要rehash，是的话返回1
// 6. 释放ht[0]的table，将ht[1]的table赋值给ht[0]，重置ht[1]
// 7. rehashidx置为-1
typedef struct dict {
    dictType *type;
    void *privdata;     // 用来存放用户变量指针
    dictht ht[2];
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    int iterators; /* number of iterators currently running */
} dict;

// 遍历hash表的迭代器
// 如果safe为1表示该迭代器是一个安全迭代器，这会确保在遍历过程中hash表不会被resize，所以可以在遍历过程中操作hash表
// 如果safe为0，则在使用过程中应该确保该hash表不应该会保证，内部也使用fingerprint这个指纹变量来在debug模式下检测
typedef struct dictIterator {
    dict *d;
    long index;
    int table, safe;
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

// hash表初始大小
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
// 一些简单的宏定义，包括释放key，value，设置key，value，比较key
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { entry->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)    // 计算key的hash值
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)      // hash表桶数量
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)       // hash表元素数量
#define dictIsRehashing(d) ((d)->rehashidx != -1)           // 是否正在分步resh操作

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);                            // 创建一个空的hash表
int dictExpand(dict *d, unsigned long size);                                    // 将hash表扩容到能容纳size的最小2次幂大小
int dictAdd(dict *d, void *key, void *val);                                     // 增加键值对到hash表中，如果key已经错在，返回错误
dictEntry *dictAddRaw(dict *d, void *key);                                      // hash表增加元素的原始接口，只增加了一个对应key的entry到hash表中，但不对值进行设置，如果key已经存在返回NULL
int dictReplace(dict *d, void *key, void *val);                                 // 更新hash表的key对应的value值，如果key不存在则插入一个新的，操作成功返回1，失败返回0
dictEntry *dictReplaceRaw(dict *d, void *key);                                  // dictAddRaw的升级版，如果key存在，返回当前key对应的value，如果不存在，返回新插入的entry
int dictDelete(dict *d, const void *key);                                       // 删除hash表key所对应的元素，并调用对应的回调去释放key和value
int dictDeleteNoFree(dict *d, const void *key);                                 // 删除hash表key所对应的元素，但不调用对应释放key和value的回调
void dictRelease(dict *d);                                                      // 释放并清除整个hash表的所有内存
dictEntry * dictFind(dict *d, const void *key);                                 // hash表中查找key对应的entry，如果找不到返回NULL
void *dictFetchValue(dict *d, const void *key);                                 // hash表中查找key对应的value，如果找不到返回NULL
int dictResize(dict *d);                                                        // 将hash表的大小减少到能容纳里面元素的最小值，最小不能小过DICT_HT_INITIAL_SIZE，如果当前禁止resize操作或者当前正在rehash，返回出错
dictIterator *dictGetIterator(dict *d);                                         // 获取遍历该hash表的迭代器，遍历过程中应该确保该hash表不能被改变
dictIterator *dictGetSafeIterator(dict *d);                                     // 获取遍历该hash表的安全迭代器，遍历过程中能确保不会触发rehash操作，但遍历过程中新加的元素可能会不被遍历
dictEntry *dictNext(dictIterator *iter);                                        // 获取迭代器下一个该遍历的元素
void dictReleaseIterator(dictIterator *iter);                                   // 释放迭代器内存
dictEntry *dictGetRandomKey(dict *d);                                           // 从hash表中随机获取一个entry
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);     // 从hash表中随机选取count个元素，存放在des指向的数组中，返回实际获取的元素个数，可能会小于count
void dictGetStats(char *buf, size_t bufsize, dict *d);                          // for debug
unsigned int dictGenHashFunction(const void *key, int len);                     // hash函数
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);        // 不区分大小写的hash函数
void dictEmpty(dict *d, void(callback)(void*));                                 // 清空hash表，清空之前会调用callback，具体是一次还是两次取决于是否在rehash
void dictEnableResize(void);                                                    // 开启hash表resize操作
void dictDisableResize(void);                                                   // 关闭hash表resize操作，注意即使关闭的情况下，当比率大于5:1的情况下还是会触发resize
int dictRehash(dict *d, int n);                                                 // 执行n步rehash操作，返回1表示还有元素需要再一次进行rehash，否则返回0
int dictRehashMilliseconds(dict *d, int ms);                                    // rehash一定时间
void dictSetHashFunctionSeed(unsigned int initval);                             // 设置hash函数种子
unsigned int dictGetHashFunctionSeed(void);                                     // 获取hash函数种子
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);     // 触发一次遍历hash表操作，外层需要套用循环来遍历整个hash表

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
