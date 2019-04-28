#ifndef __FILESYSTEM_C_WRAPPER_H__
#define __FILESYSTEM_C_WRAPPER_H__

#include "Common.hh"
#include "../common_c_cpp_types.h"


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Superblock 类的 C 包装

#ifndef __cplusplus
typedef struct
{
	s32	s_isize;		/* 外存Inode区占用的盘块数 */
	s32	s_fsize;		/* 盘块总数 */
	
	s32	s_nfree;		/* 直接管理的空闲盘块数量 */
	s32	s_free[100];		/* 直接管理的空闲盘块索引表 */
	
	s32	s_ninode;		/* 直接管理的空闲外存Inode数量 */
	s32	s_inode[100];		/* 直接管理的空闲外存Inode索引表 */
	
	s32	s_flock;		/* 封锁空闲盘块索引表标志 */
	s32	s_ilock;		/* 封锁空闲Inode表标志 */
	
	s32	s_fmod;			/* 内存中super block副本被修改标志，意味着需要更新外存对应的Super Block */
	s32	s_ronly;		/* 本文件系统只能读出 */
	s32	s_time;			/* 最近一次更新时间 */
	s32	padding[47];	/* 填充使SuperBlock块大小等于1024字节，占据2个扇区 */

	Inode*	s_inodep;		// SuperBlock 所在文件系统的根节点
	Devtab*	s_dev;			// SuperBlock 所在文件系统的设备
} SuperBlock;

//static size_t x = sizeof(Superblock);

#else // __cplusplus
class SuperBlock;
#endif // __cplusplus

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(SuperBlock)

// FileSystem 类的 C 包装

#ifndef __cplusplus
typedef struct FileSystem
{
	BufferManager* m_BufferManager;		/* FileSystem类需要缓存管理模块(BufferManager)提供的接口 */
	int updlock;				/* Update()函数的锁，该函数用于同步内存各个SuperBlock副本以及，
						被修改过的内存Inode。任一时刻只允许一个进程调用该函数 */
} FileSystem;
#else // __cplusplus
class FileSystem;
#endif // __cplusplus

extern const s32
	SECONDFS_NMOUNT,			/* 系统中用于挂载子文件系统的装配块数量 */
	SECONDFS_SUPER_BLOCK_SECTOR_NUMBER,	/* 定义SuperBlock位于磁盘上的扇区号，占据200，201两个扇区。 */
	SECONDFS_ROOTINO,			/* 文件系统根目录外存Inode编号 */
	SECONDFS_INODE_NUMBER_PER_SECTOR,	/* 外存INode对象长度为64字节，每个磁盘块可以存放512/64 = 8个外存Inode */
	SECONDFS_INODE_ZONE_START_SECTOR,	/* 外存Inode区位于磁盘上的起始扇区号 */
	SECONDFS_INODE_ZONE_SIZE,		/* 磁盘上外存Inode区占据的扇区数 */
	SECONDFS_DATA_ZONE_START_SECTOR,	/* 数据区的起始扇区号 */
	SECONDFS_DATA_ZONE_END_SECTOR,		/* 数据区的结束扇区号 */
	SECONDFS_DATA_ZONE_SIZE			/* 数据区占据的扇区数量 */
;

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(FileSystem)

void FileSystem_Initialize(FileSystem *fs);
void FileSystem_LoadSuperBlock(FileSystem *fs, SuperBlock *secsb);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __FILESYSTEM_C_WRAPPER_H__