/* UNIXV6PP 文件系统(主要是超块操作)头文件裁剪. */
#ifndef __FILESYSTEM_HH__
#define __FILESYSTEM_HH__

#include <cstdint>
#include <cstddef>
#include "../common.h"

#include "Inode.hh"
#include "BufferManager.hh"

#include "FileSystem_c_wrapper.h"

/*
 * 文件系统存储资源管理块(Super Block)的定义。
 * @Feng Shun: 注意, SuperBlock 在 V6PP 的内存中和磁盘中是统一的.
 * 但在 Linux 中, 不可能统一. 要将 V6PP 的 SuperBlock 格式和上层抽象
 * VFS 层的 super_block 做转换协调.
 * 
 * 在这里, 我们让 SuperBlock 兼任原 UNIXV6PP 中 Mount 结构的职能.
 * 这个时候, SuperBlock 已经超过了 1024 字节, 但从磁盘取时, 只需填满
 * 前 1024 字节, 然后再填充后面两个指针即可.
 */

class SuperBlock
{
	/* Functions */
public:
	/* Constructors */
	SuperBlock();
	/* Destructors */
	~SuperBlock();
	
	/* Members */
public:
	s32	s_isize;		/* 外存Inode区占用的盘块数 */
	s32	s_fsize;		/* 盘块总数 */
	
	s32	s_nfree;		/* 直接管理的空闲盘块数量 */
	s32	s_free[100];		/* 直接管理的空闲盘块索引表 */
	
	s32	s_ninode;		/* 直接管理的空闲外存Inode数量 */
	s32	s_inode[100];		/* 直接管理的空闲外存Inode索引表 */
	
	//s32	s_flock_obsolete;	/* 封锁空闲盘块索引表标志 */
	s32	s_has_dots;		/* 我们用两个 lock 弃置后的空间来表示 "文件系统是否有 . 和 .. 目录项"吧. */
	/* 这个项只有为全满(0xffffffff)时才表示"有目录项"! */

	s32	s_ilock_obsolete;	/* 封锁空闲Inode表标志 */
	
	s32	s_fmod;			/* 内存中super block副本被修改标志，意味着需要更新外存对应的Super Block */
	s32	s_ronly;		/* 本文件系统只能读出 */
	s32	s_time;			/* 最近一次更新时间 */
	s32	padding[47];		/* 填充使SuperBlock块大小等于1024字节，占据2个扇区 */

	Inode*	s_inodep;		// SuperBlock 所在文件系统的根节点
	Devtab*	s_dev;			// SuperBlock 所在文件系统的设备
	void * /* struct super_block * */s_vsb;		// VFS 超块

	struct {u8 data[SECONDFS_MUTEX_SIZE];}	s_update_lock;
	struct {u8 data[SECONDFS_MUTEX_SIZE];}	s_flock;
	struct {u8 data[SECONDFS_MUTEX_SIZE];}	s_ilock;
};

/*
 * 文件系统类(FileSystem)管理文件存储设备中
 * 的各类存储资源，磁盘块、外存INode的分配、
 * 释放。
 */
class FileSystem
{
public:
	/* static consts */
	static const s32 NMOUNT = 5;			/* 系统中用于挂载子文件系统的装配块数量 */

	static const s32 SUPER_BLOCK_SECTOR_NUMBER = 200;	/* 定义SuperBlock位于磁盘上的扇区号，占据200，201两个扇区。 */

	static const s32 ROOTINO = 0;			/* 文件系统根目录外存Inode编号 */

	static const s32 INODE_NUMBER_PER_SECTOR = 8;		/* 外存INode对象长度为64字节，每个磁盘块可以存放512/64 = 8个外存Inode */
	static const s32 INODE_ZONE_START_SECTOR = 202;		/* 外存Inode区位于磁盘上的起始扇区号 */
	static const s32 INODE_ZONE_SIZE = 1024 - 202;		/* 磁盘上外存Inode区占据的扇区数 */

	static const s32 DATA_ZONE_START_SECTOR = 1024;		/* 数据区的起始扇区号 */
	static const s32 DATA_ZONE_END_SECTOR = 18000 - 1;	/* 数据区的结束扇区号 */
	static const s32 DATA_ZONE_SIZE = 18000 - DATA_ZONE_START_SECTOR;	/* 数据区占据的扇区数量 */

	/* Functions */
public:
	/* Constructors */
	FileSystem();
	/* Destructors */
	~FileSystem();

	/* 
	* @comment 从磁盘读入SuperBlock.
	* 不读入 s_inodep 成员和 s_dev成员.
	* 不进行端序的转换.
	*/
	void LoadSuperBlock(SuperBlock *secsb);


	/* 
	 * @comment 初始化成员变量
	 */
	void Initialize();

#if false

	/* 
	 * @comment 根据文件存储设备的设备号dev获取
	 * 该文件系统的SuperBlock
	 */
	SuperBlock* GetFS(short dev);
#endif

	/* 
	 * @comment 将SuperBlock对象的内存副本更新到
	 * 存储设备的SuperBlock中去
	 */
	void Update(SuperBlock *secsb);

	/* 
	 * @comment  在存储设备dev上分配一个空闲
	 * 外存INode，一般用于创建新的文件。
	 */
	Inode* IAlloc(SuperBlock *secsb);

	/* 
	 * @comment  释放超级块secsb所在文件系统中编号为number
	 * 的外存INode，一般用于删除文件。
	 */
	void IFree(SuperBlock *secsb, int number);

	/* 
	 * @comment 在存储设备dev上分配空闲磁盘块
	 */
	Buf* Alloc(SuperBlock *secsb);
	/* 
	 * @comment 释放secsb所在文件系统编号为blkno的磁盘块
	 */
	void Free(SuperBlock *secsb, int blkno);
#if false
	/* 
	 * @comment 查找文件系统装配表，搜索指定Inode对应的Mount装配块
	 */
	Mount* GetMount(Inode* pInode);


	/* 
	 * @comment 检查设备dev上编号blkno的磁盘块是否属于
	 * 数据盘块区
	 */
	bool BadBlock(SuperBlock* spb, short dev, int blkno);
#endif

	/* Members */
	// Mount m_Mount[NMOUNT];		/* 文件系统装配块表，Mount[0]用于根文件系统 */

	BufferManager* m_BufferManager;		/* FileSystem类需要缓存管理模块(BufferManager)提供的接口 */
	struct {u8 data[SECONDFS_MUTEX_SIZE];} updlock;	/* Update()函数的锁，该函数用于同步内存各个SuperBlock副本以及，
						被修改过的内存Inode。任一时刻只允许一个进程调用该函数 */
};


#endif // __FILESYSTEM_HH__