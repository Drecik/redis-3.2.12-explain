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

#ifndef _ZIPLIST_H
#define _ZIPLIST_H

// 压缩链表存储
// 跟zipmap一样，为了节约内存，redis提供了一种压缩更狠的双向链表存储结构。以节省内存为第一优先级，每次插入和删除元素都会导致内存的重新申请。
// ziplist也是一个char数组，使用约定好的格式来存放每个元素
// ziplist有比zipmap更高的内存压缩，但同样的格式也比zipmap复杂点，特别是插入和删除可能会造成一系列瀑布式修改后面的entry内存。
// redis在元素比较少的时候使用该结构来替代hash表
// ziplist二进制格式：
// <zlbytes><zltail><zllen><entry><entry><zlend>
// zlbytes：4 bytes，unsigned int，存放ziplist实际使用的内存大小，通过它能够迅速的定位到ziplist的尾部
// zltail：4 bytes，unsigned int，存放尾部元素entry地址的偏移量，通过它能够迅速定位到尾部元素的地址
// zllen：2 bytes，存放ziplist中的元素个数，如果ziplist实际元素超过2^16-2的话，就需要遍历整个ziplist来获取元素个数，这个跟zipmap中的zmlen，但zmlen是1 byte
// zlend：1 byte，值为255，ziplist结束标记位
// entry：每个entry里也有固定的二进制格式
//      <prevlen><encode+len><value>
//      prevlen：1 byte/5 bytes，前一个元素的entry占用大小，如果前一个entry长度<254则，使用1 byte来存储，否则第一个byte赋值为254，后面4 bytes存放前一个entry的长度。
//              有了该变量，ziplist就可以做到从后往前遍历
//      encode+len：value编码方式和实际占用的长度，value传入的时候为字符串，但redis会尝试将它转为整形，以节约内存。所以最终存储的value可能是字符串或者是整形
//          |00pppppp|：1 byte，value为字符串，且字符串长度<=63(2^6-1)，长度存放在低6位中
//          |01pppppp|qqqqqqqq|：2 bytes，value为字符串，且长度<=16384(2^14-1)，长度存放在这两个bytes的低14位中
//          |10______|qqqqqqqq|rrrrrrrr|ssssssss|tttttttt|： 5 bytes，value为字符串，且长度>=16384，长度存放在后4 bytes中
//          |11000000|：1 byte，value为整形，值>=2^8且<2^16，值存放在跟在后面的两个bytes中
//          |11010000|：1 byte，value为整形，值>=2^32且<2^32，值存放在跟在后面的四个bytes中
//          |11100000|：1 byte，value为整形，值>=2^32且<2^64，值存放在跟在后面的八个bytes中
//          |11110000|：1 byte，value为整形，值>=2^16且<2^24，值存放在跟在后面的三个bytes中
//          |11111110|：1 byte，value为整形，值>=13且<2^8，值存放在跟在后面的一个byte中
//          |1111xxxx|：1 byte，除去上面5中整形编码，xxxx可以存放0001到1101，即1-13，redis对这种编码做了下加减1的偏移处理，所以这种编码可以存放的范围为0-12，这样可以完整的存放所有大小的整形

#define ZIPLIST_HEAD 0
#define ZIPLIST_TAIL 1

// 创建一个空的ziplist
unsigned char *ziplistNew(void);

// 合并两个ziplist, second会被追加到first的ziplist中
// 合并的时候采用长度较长的ziplist进行relloc，具体是哪个通过first或者second返回，没有被选择relloc的将会返回NULL
unsigned char *ziplistMerge(unsigned char **first, unsigned char **second);

// 插入元素到ziplist中，where表示插入的位置ZIPLIST_HEAD or ZIPLIST_TAIL
unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where);

// 返回ziplist中index为下标的元素，如果index为正值，则从头往后开始，下标从0开始
// 如果index为负值，则从后往前开始小标从-1开始，index超出范围或者ziplist为空，则会返回NULL
unsigned char *ziplistIndex(unsigned char *zl, int index);

// 返回p指向的entry的后一个节点
// 如果p已经是尾部，或者p下一个元素就是尾部，则返回NULL
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p);

// 返回p指向的entry的前一个节点
// 如果p已经是头部，则返回NULL
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p);

// 获取p指向的entry里面的值，根据encoding方式，返回的值可能存储在sstr/sval中，如果编码为整形的话，*sstr将设置为NULL
// 如果p指向尾部，则返回0，否则返回1
unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval);

// 插入数据到ziplist中的p位置
unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen);

// 删除ziplist中p指向的entry
// 因为删除操作会造成ziplist内存重新分配，为了循环方便，p将重新指向ziplist中原p指向的内存，即原p的下一个entry的地址
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p);

// 从ziplist的index下标开始，删除num个数量的entry
unsigned char *ziplistDeleteRange(unsigned char *zl, int index, unsigned int num);

// 比较p指向的entry的内容与指定值相等，不等返回0，否则返回1
unsigned int ziplistCompare(unsigned char *p, unsigned char *s, unsigned int slen);

// 从p指向的entry开始查找指定的值，找到返回entry的地址，否则返回NULL
// skip参数表示，每次比较之间忽略多少个entry，例如如果是1的话，且p的小标为1的话，则会比较1，3，5，7，9小标的entry
unsigned char *ziplistFind(unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip);

// 返回ziplist中元素数量
unsigned int ziplistLen(unsigned char *zl);

// 返回ziplist实际占用的内存大小
size_t ziplistBlobLen(unsigned char *zl);

#ifdef REDIS_TEST
int ziplistTest(int argc, char *argv[]);
#endif

#endif /* _ZIPLIST_H */
