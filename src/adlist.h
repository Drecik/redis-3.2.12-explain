/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */
/*
- 双向链表，结构比较简单
- 提供自定义的dup, free, match操作接口
*/

// 双向链表节点，存放前后指针以及一个value
typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

// 链表迭代器，存放下一个节点以及一个代表方向(往前还是往后)的变量
typedef struct listIter {
    listNode *next;
    int direction;
} listIter;

// 双向链表结构体
typedef struct list {
    listNode *head;         // 指向链表头节点
    listNode *tail;         // 指向链表尾节点

    // dup, free, match操作的函数指针，可自定义设置
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);

    unsigned long len;      // 链表长度
} list;

/* Functions implemented as macros */
// 一些函数直接使用宏来实现，来增加效率
#define listLength(l) ((l)->len)            // 获取链表长度
#define listFirst(l) ((l)->head)            // 获取链表第一个节点
#define listLast(l) ((l)->tail)             // 获取链表最后一个节点
#define listPrevNode(n) ((n)->prev)         // 获取节点的前一个节点
#define listNextNode(n) ((n)->next)         // 获取节点的后一个节点
#define listNodeValue(n) ((n)->value)       // 获取节点的值

#define listSetDupMethod(l,m) ((l)->dup = (m))      // 设置自定义dup函数
#define listSetFreeMethod(l,m) ((l)->free = (m))    // 设置自定义free函数
#define listSetMatchMethod(l,m) ((l)->match = (m))  // 设置自定义match函数

#define listGetDupMethod(l) ((l)->dup)      // 获取dup函数
#define listGetFree(l) ((l)->free)          // 获取free函数
#define listGetMatchMethod(l) ((l)->match)  // 获取match函数

/* Prototypes */
list *listCreate(void);                     // 创建一个空的链表结构
void listRelease(list *list);               // 释放整个list，包括list内的各个节点
list *listAddNodeHead(list *list, void *value);     // 增加一个值为value的节点到list的头部
list *listAddNodeTail(list *list, void *value);     // 增加一个值为value的节点到list的尾部
list *listInsertNode(list *list, listNode *old_node, void *value, int after);       // 增加一个节点到指定节点的前/后，after表示插入前还是后
void listDelNode(list *list, listNode *node);           // 删除指定的节点，因为效率问题，没有判断node是否真的是该list的节点
listIter *listGetIterator(list *list, int direction);   // 创建指定list，指定方向的一个迭代器
listNode *listNext(listIter *iter);                     // 获取迭代器的下一个节点
void listReleaseIterator(listIter *iter);               // 释放迭代器内存
list *listDup(list *orig);                              // 完整的复制一个链表
listNode *listSearchKey(list *list, void *key);         // 从list中查找指定节点
listNode *listIndex(list *list, long index);            // 返回list中下标为index的节点，index如果为正值，表示从头开始的小标，且下标从0开始，即0为head，如果index为负值，表示从后往前的下标，且下标从-1开始，即-1表示tail
void listRewind(list *list, listIter *li);              // 重置迭代器为从头往后遍历
void listRewindTail(list *list, listIter *li);          // 重置迭代器为从后往前遍历
void listRotate(list *list);                            // 顺时针旋转list，将list的tail节点从尾部移除，插入到list头部

/* Directions for iterators */
// list迭代器方向
#define AL_START_HEAD 0     // 从头开始往后遍历
#define AL_START_TAIL 1     // 从尾部开始往前遍历

#endif /* __ADLIST_H__ */
