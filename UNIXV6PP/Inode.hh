/* UNIXV6PP 文件系统(主要是 Inode 操作)头文件裁剪. */
#ifndef __INODE_HH__
#define __INODE_HH__

#include <cstdint>
#include <cstddef>
#include "../common_c_cpp_types.h"
#include "../c_helper_for_cc.h"
#include "Inode_c_wrapper.h"
#include "FileSystem.hh"
#include "BufferManager.hh"

/*
 * 内存索引节点(INode)的定义
 * 系统中每一个打开的文件、当前访问目录、
 * 挂载的子文件系统都对应唯一的内存inode。
 * 每个内存inode通过外存inode所在存储设备的设备号(i_dev)
 * 以及该设备外存inode区中的编号(i_number)来确定
 * 其对应的外存inode。
 */

/*
 * @Feng Shun:
 *   这里的 Inode 作为文件系统特定的 Inode 内存内表示.
 *   与大多数其他文件系统一样, 此 Inode 结构包含一个 VFS Inode
 *   结构, 在 alloc_inode 函数中申请的是 Inode, 而返回 VFS
 *   Inode 的指针, 这样就能保证每一个 UnixV6++ 文件系统的 VFS
 *   Inode 都能伴随一个此 Inode 结构出现.
 * 
 * 注: i_flag 的 ILOCK, i_count 等锁机制和引用计数机制
 * 在本工程中不用, 交由系统管理
*/
class Inode
{
public:
	/* i_flag中标志位 */
	enum INodeFlag
	{
		ILOCK = 0x1,		/* 索引节点上锁 */
		IUPD  = 0x2,		/* 内存inode被修改过，需要更新相应外存inode */
		IACC  = 0x4,		/* 内存inode被访问过，需要修改最近一次访问时间 */
		IMOUNT = 0x8,		/* 内存inode用于挂载子文件系统 */
		IWANT = 0x10,		/* 有进程正在等待该内存inode被解锁，清ILOCK标志时，要唤醒这种进程 */
		ITEXT = 0x20		/* 内存inode对应进程图像的正文段 */
	};
	
	/* static const member */
	static const u32 IALLOC = 0x8000;		/* 文件被使用 */
	static const u32 IFMT = 0x6000;		/* 文件类型掩码 */
	static const u32 IFDIR = 0x4000;		/* 文件类型：目录文件 */
	static const u32 IFCHR = 0x2000;		/* 字符设备特殊类型文件 */
	static const u32 IFBLK = 0x6000;		/* 块设备特殊类型文件，为0表示常规数据文件 */
	static const u32 ILARG = 0x1000;		/* 文件长度类型：大型或巨型文件 */
	static const u32 ISUID = 0x800;		/* 执行时文件时将用户的有效用户ID修改为文件所有者的User ID */
	static const u32 ISGID = 0x400;		/* 执行时文件时将用户的有效组ID修改为文件所有者的Group ID */
	static const u32 ISVTX = 0x200;		/* 使用后仍然位于交换区上的正文段 */
	static const u32 IREAD = 0x100;		/* 对文件的读权限 */
	static const u32 IWRITE = 0x80;		/* 对文件的写权限 */
	static const u32 IEXEC = 0x40;			/* 对文件的执行权限 */
	static const u32 IRWXU = (IREAD|IWRITE|IEXEC);		/* 文件主对文件的读、写、执行权限 */
	static const u32 IRWXG = ((IRWXU) >> 3);			/* 文件主同组用户对文件的读、写、执行权限 */
	static const u32 IRWXO = ((IRWXU) >> 6);			/* 其他用户对文件的读、写、执行权限 */
	
	static const s32 BLOCK_SIZE = 512;		/* 文件逻辑块大小: 512字节 */
	static const s32 ADDRESS_PER_INDEX_BLOCK = BLOCK_SIZE / sizeof(int);	/* 每个间接索引表(或索引块)包含的物理盘块号 */

	static const s32 SMALL_FILE_BLOCK = 6;	/* 小型文件：直接索引表最多可寻址的逻辑块号 */
	static const s32 LARGE_FILE_BLOCK = 128 * 2 + 6;	/* 大型文件：经一次间接索引表最多可寻址的逻辑块号 */
	static const s32 HUGE_FILE_BLOCK = 128 * 128 * 2 + 128 * 2 + 6;	/* 巨型文件：经二次间接索引最大可寻址文件逻辑块号 */

	static const s32 PIPSIZ = SMALL_FILE_BLOCK * BLOCK_SIZE;

	/* static member */
	static s32 rablock;		/* 顺序读时，使用预读技术读入文件的下一字符块，rablock记录了下一逻辑块号
							经过bmap转换得到的物理盘块号。将rablock作为静态变量的原因：调用一次bmap的开销
							对当前块和预读块的逻辑块号进行转换，bmap返回当前块的物理盘块号，并且将预读块
							的物理盘块号保存在rablock中。 */

	/* Functions */
public:
	/* Constructors */
	Inode();
	/* Destructors */
	~Inode();
	
	/* 
	 * @comment 根据Inode对象中的物理磁盘块索引表，读取相应
	 * 的文件数据
	 */
	void ReadI();

	/* 
	 * @comment 根据Inode对象中的物理磁盘块索引表，将数据写入文件
	 */
	void WriteI();
	/* 
	 * @comment 将文件的逻辑块号转换成对应的物理盘块号
	 */
	int Bmap(int lbn);
	
	/* 
	 * @comment 对特殊字符设备、块设备文件，调用该设备注册在块设备开关表
	 * 中的设备初始化程序
	 */
	void OpenI(int mode);
	/* 
	 * @comment 对特殊字符设备、块设备文件。如果对该设备的引用计数为0，
	 * 则调用该设备的关闭程序
	 */
	void CloseI(int mode);
	
	/* 
	 * @comment 更新/写回外存Inode的最后的访问时间、修改时间
	 * 检查 IUPD 或 IACC 是否置位, 置为才更新并且写回
	 */
	void IUpdate(int time);
	/* 
	 * @comment 释放Inode对应文件占用的磁盘块
	 */
	void ITrunc();

	/* 
	 * @comment 对Pipe或者Inode解锁，并且唤醒因等待锁而睡眠的进程
	 */
	void Prele();

	/* 
	 * @comment 对Pipe上锁，如果Pipe已经被上锁，则增设IWANT标志并睡眠等待直至解锁
	 */
	void Plock();

	/*
	 * @comment 对Pipe或者Inode解锁，并且唤醒因等待锁而睡眠的进程
	 */
	void NFrele();

	/*
	 * @comment 对Pipe上锁，如果Pipe已经被上锁，则增设IWANT标志并睡眠等待直至解锁
	 */
	void NFlock();

	/* 
	 * @comment 清空Inode对象中的数据
	 */
	void Clean();

	/* 
	 * @comment 将包含外存Inode字符块中信息拷贝到内存Inode中
	 * 并将临时的 DiskInode 指针回传
	 */
	void ICopy(Buf* bp, int inumber);

	/* Members */
public:
	u32 i_flag;	/* 状态的标志位，定义见enum INodeFlag */
	u32 i_mode;	/* 文件工作方式信息 */
	
	s32		i_count;		/* 引用计数 */
	s32		i_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 */
	
	SuperBlock*	i_ssb;			/* 外存inode所在 SuperBlock */
	s32		i_number;		/* 外存inode区中的编号 */
	
	u16		i_uid;			/* 文件所有者的用户标识数 */
	u16		i_gid;			/* 文件所有者的组标识数 */
	
	s32		i_size;			/* 文件大小，字节为单位 */
	s32		i_addr[10];		/* 用于文件逻辑块好和物理块好转换的基本索引表 */
	
	s32		i_lastr;		/* 存放最近一次读取文件的逻辑块号，用于判断是否需要预读 */

	s32		i_atime;		/* 最后访问时间 */
	s32		i_mtime;		/* 最后修改时间 */

	struct {u8 data[SECONDFS_INODE_SIZE];}	vfs_inode;	/* 包含的 VFS Inode 数据结构. */
};


/*
 * 外存索引节点(DiskINode)的定义
 * 外存Inode位于文件存储设备上的
 * 外存Inode区中。每个文件有唯一对应
 * 的外存Inode，其作用是记录了该文件
 * 对应的控制信息。
 * 外存Inode中许多字段和内存Inode中字段
 * 相对应。外存INode对象长度为64字节，
 * 每个磁盘块可以存放512/64 = 8个外存Inode
 */

class DiskInode
{
	/* Functions */
public:
	/* Constructors */
	DiskInode();
	/* Destructors */
	~DiskInode();

	/* Members */
public:
	u32		d_mode;			/* 状态的标志位，定义见enum INodeFlag */
	s32		d_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 */
	
	u16		d_uid;			/* 文件所有者的用户标识数 */
	u16		d_gid;			/* 文件所有者的组标识数 */
	
	s32		d_size;			/* 文件大小，字节为单位 */
	s32		d_addr[10];		/* 用于文件逻辑块号和物理块好转换的基本索引表 */

	s32		d_atime;		/* 最后访问时间 */
	s32		d_mtime;		/* 最后修改时间 */
};
#endif // __INODE_HH__
