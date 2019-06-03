#ifndef __SECONDFS_H__
#define __SECONDFS_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
#define SECONDFS_KERNEL_BEFORE_4_14
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
#define SECONDFS_KERNEL_BEFORE_4_9
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
#define SECONDFS_KERNEL_BEFORE_4_8
#endif

/* Declare or define self-owned functions, variables and macros of SecondFS. */
/* 声明 SecondFS 文件系统使用的函数原型, 变量, 宏等 */

#include "UNIXV6PP/BufferManager_c_wrapper.h"
#include "UNIXV6PP/Inode_c_wrapper.h"
#include "UNIXV6PP/FileSystem_c_wrapper.h"
#include "UNIXV6PP/FileOperations_c_wrapper.h"

/******* C-only part *******/
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
#include <linux/statfs.h>

#define SECONDFS_BITS_PER_BYTE CHAR_BIT

// Kernel cache descriptor for Inode struct
// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 Inode
extern struct kmem_cache *secondfs_icachep;

// Kernel cache descriptor for DiskInode struct
// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 DiskInode
extern struct kmem_cache *secondfs_diskinode_cachep;

/*** Functions(mostly internel & private) ***/
/*** 函数 ***/

// Similar to kernel iget_locked(): *Get* an (vfs) inode
// by (vfs) super_block and inode number (when cannot find
// in memory, an inode and a UnixV6++ Inode are allocated, 
// filled and they bidirectionally linked)
// 根据 VFS 超块和 Inode 编号, 获得 VFS Inode (顺带 UnixV6++ Inode; 
// 若内存中没有 Inode, 则载入 Inode)
struct inode *secondfs_iget(struct super_block *sb, unsigned long ino);

// Get the pointer to container Inode from pointer to inner vfs inode
// vfs inode 与 Inode 是包含关系 (vfs inode 包含于 Inode).
// 根据 VFS Inode 的指针, 往前减掉几个字节, 获得包含它的 Unix V6++
// 的 Inode 的指针
static inline Inode *SECONDFS_INODE(struct inode *inode)
{
	return container_of(inode, Inode, vfs_inode);
}

// Get the pointer to Superblock from pointer to vfs super_block
// 和 SECONDFS_INODE 的逻辑不同: VFS 超块的 s_fs_info 指向
// Unix V6++ 超块, 而 Inode 包含了 struct inode
// 根据 VFS 超块指针, 获得 Unix V6++ 的超块指针
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

/******* C++-only part *******/
/******* 仅 C++ 部分 *******/

#ifdef __cplusplus
#include "UNIXV6PP/Inode.hh"

/*** Functions(mostly internel & private) ***/
/*** 函数 ***/

static inline Inode *SECONDFS_INODE(struct inode *inode)
{
	return ({
		void *__mptr = (void *)(inode);
		((Inode *)((char *)__mptr - offsetof(Inode, vfs_inode)));
	});
}

#endif // __cplusplus

/******* Common part *******/
/******* C 和 C++ 都生效部分 *******/
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*** Functions(mostly internel & private) ***/
/*** 函数 ***/

extern int secondfs_submit_bio_sync_read(void * /* actually struct block_device * */ bdev, u32 sector,
				void *buf);
extern int secondfs_submit_bio_sync_write(void * /* actually struct block_device * */ bdev, u32 sector,
				void *buf);
extern Inode *secondfs_iget_forcc(SuperBlock *secsb, unsigned long ino);
extern Inode *secondfs_c_helper_new_inode(SuperBlock *ssb);

/*** One time C++ objects ***/
/*** 一次性 C++ 对象 ***/
extern BufferManager *secondfs_buffermanagerp;
extern FileSystem *secondfs_filesystemp;
extern FileManager *secondfs_filemanagerp;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __SECONDFS_H__