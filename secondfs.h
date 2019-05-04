#ifndef __SECONDFS_H__
#define __SECONDFS_H__

/* 声明 SecondFS 文件系统使用的函数原型, 变量, 宏等 */
/* Declare or define self-owned functions, variables and macros of SecondFS. */

#include "UNIXV6PP/BufferManager_c_wrapper.h"
#include "UNIXV6PP/Inode_c_wrapper.h"
#include "UNIXV6PP/FileSystem_c_wrapper.h"
#include "UNIXV6PP/FileOperations_c_wrapper.h"

/******* 仅 C 部分 *******/
#ifndef __cplusplus

#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/parser.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/limits.h>
#include <linux/types.h>
#include <linux/kernel.h>

#define SECONDFS_BITS_PER_BYTE CHAR_BIT

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 Inode
extern struct kmem_cache *secondfs_icachep;

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 DiskInode
extern struct kmem_cache *secondfs_diskinode_cachep;

/*** 内部的私有函数 ***/

// 根据 VFS 超块和 Inode 编号, 获得 VFS Inode (顺带 UnixV6++ Inode; 
// 若内存中没有 Inode, 则载入 Inode)
struct inode *secondfs_iget(struct super_block *sb, unsigned long ino);

// 根据 VFS Inode, 获得 Unix V6++ 的 Inode
static inline Inode *SECONDFS_INODE(struct inode *inode)
{
	return container_of(inode, Inode, vfs_inode);
}

// 根据 VFS 超块, 获得 Unix V6++ 的超块
// 和 SECONDFS_INODE 的逻辑不同: VFS 超块的 s_fs_info 指向
// Unix V6++ 超块, 而 Inode 包含了 struct inode
static inline SuperBlock *SECONDFS_SB(struct super_block *sb)
{
	return (SuperBlock *)sb->s_fs_info;
}

extern void secondfs_inode_conform_v2s(Inode *si, struct inode *inode);
extern void secondfs_inode_conform_s2v(struct inode *inode, Inode *si);

extern void secondfs_dirty_inode(struct inode *inode, int flags);
extern struct inode *secondfs_iget(struct super_block *sb, unsigned long ino);
extern struct inode *secondfs_alloc_inode(struct super_block *sb);
extern int secondfs_write_inode(struct inode *inode, struct writeback_control *wbc);
extern void secondfs_destroy_inode(struct inode *inode);
extern void secondfs_evict_inode(struct inode *inode);
extern int secondfs_sync_fs(struct super_block *sb, int wait);
extern int secondfs_fill_super(struct super_block *sb, void *data, int silent);
extern void secondfs_put_super(struct super_block *sb);
extern struct dentry *secondfs_mount(struct file_system_type *fs_type,
				int flags, const char *devname,
				void *data);
struct inode *secondfs_new_inode(struct inode *dir, umode_t mode,
				const struct qstr *str);

extern struct super_operations secondfs_sb_ops;

extern struct inode_operations secondfs_file_inode_operations;
extern struct file_operations secondfs_file_operations;
extern struct inode_operations secondfs_dir_inode_operations;
extern struct file_operations secondfs_dir_operations;


#endif // __cplusplus

/******* 仅 C++ 部分 *******/

#ifdef __cplusplus
#include "UNIXV6PP/Inode.hh"

/*** 内部的私有函数 ***/

static inline Inode *SECONDFS_INODE(struct inode *inode)
{
	return ({
		void *__mptr = (void *)(inode);
		((Inode *)((char *)__mptr - offsetof(Inode, vfs_inode)));
	});
}

#endif // __cplusplus

/******* C 和 C++ 都生效部分 *******/
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


/*** 内部的私有函数 ***/


extern int secondfs_submit_bio_sync_read(void * /* struct block_device * */ bdev, u32 sector,
				void *buf);
extern int secondfs_submit_bio_sync_write(void * /* struct block_device * */ bdev, u32 sector,
				void *buf);
extern Inode *secondfs_iget_forcc(SuperBlock *secsb, unsigned long ino);


/*** 一次性 C++ 对象 ***/
extern BufferManager *secondfs_buffermanagerp;
extern FileSystem *secondfs_filesystemp;
extern FileManager *secondfs_filemanagerp;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __SECONDFS_H__