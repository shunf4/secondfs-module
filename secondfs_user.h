#ifndef __SECONDFS_USER_H__
#define __SECONDFS_USER_H__

/* 声明 SecondFS 文件系统自己使用的函数原型, 变量, 宏等 */
/* Declare or define self-owned functions, variables and macros of SecondFS. */

// 仅 C 部分
#ifndef __cplusplus

#include <linux/limits.h>
#include <linux/types.h>

#define SECONDFS_BITS_PER_BYTE CHAR_BIT

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 DiskInode
extern struct kmem_cache *secondfs_diskinode_cachep;




#endif // __cplusplus

// C 和 C++ 皆生效部分

#include "UNIXV6PP/Inode_c_wrapper.h"
#include "UNIXV6PP/BufferManager_c_wrapper.h"
#include "UNIXV6PP/FileOperations_c_wrapper.h"
#include "UNIXV6PP/FileSystem_c_wrapper.h"

// 内部的私有函数
int secondfs_submit_bio_sync_read(void * /* struct block_device * */ bdev, u32 sector,
				void *buf);
int secondfs_submit_bio_sync_write(void * /* struct block_device * */ bdev, u32 sector,
				void *buf);

// 一次性 C++ 对象
extern BufferManager *secondfs_buffermanagerp;
extern FileSystem *secondfs_filesystemp;

#endif // __SECONDFS_USER_H__