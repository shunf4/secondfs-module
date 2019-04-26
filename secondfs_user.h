#ifndef __SECONDFS_USER_H__
#define __SECONDFS_USER_H__

#include <linux/limits.h>
#include <linux/types.h>

#include "UNIXV6PP/Inode_c_wrapper.h"

/* 声明 SecondFS 文件系统自己使用的函数原型, 变量, 宏等 */
/* Declare or define self-owned functions, variables and macros of SecondFS. */

#define SECONDFS_BITS_PER_BYTE CHAR_BIT

#define SECONDFS_INODE_SIZE sizeof(Inode)

extern const sector_t secondfs_superblock_blockno;

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 DiskInode
extern struct kmem_cache *secondfs_diskinode_cachep;

#endif // __SECONDFS_USER_H__