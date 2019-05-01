#ifndef __INODE_C_WRAPPER_H__
#define __INODE_C_WRAPPER_H__

#include "../common_c_cpp_types.h"
#include "Common.hh"

#ifndef __cplusplus
#include <linux/fs.h>
#endif // __cplusplus


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Inode 类的 C 包装
// 注: i_flag 的 ILOCK, i_count 等锁机制和引用计数机制
// 在本工程中不用, 交由系统管理
#ifndef __cplusplus
typedef struct Inode
{
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

	struct inode	vfs_inode;	/* 包含的 VFS Inode 数据结构. */
} Inode;
#else // __cplusplus
class Inode;
#endif // __cplusplus

extern const u32
	SECONDFS_ILOCK,		/* 索引节点上锁 */
	SECONDFS_IUPD,		/* 内存inode被修改过，需要更新相应外存inode */
	SECONDFS_IACC,		/* 内存inode被访问过，需要修改最近一次访问时间 */
	SECONDFS_IMOUNT,	/* 内存inode用于挂载子文件系统 */
	SECONDFS_IWANT,		/* 有进程正在等待该内存inode被解锁，清ILOCK标志时，要唤醒这种进程 */
	SECONDFS_ITEXT		/* 内存inode对应进程图像的正文段 */
;

/* static const member of INode:: */

extern const u32
	SECONDFS_IALLOC,	/* 文件被使用 */
	SECONDFS_IFMT,		/* 文件类型掩码 */
	SECONDFS_IFDIR,		/* 文件类型：目录文件 */
	SECONDFS_IFCHR,		/* 字符设备特殊类型文件 */
	SECONDFS_IFBLK,		/* 块设备特殊类型文件，为0表示常规数据文件 */
	SECONDFS_ILARG,		/* 文件长度类型：大型或巨型文件 */
	SECONDFS_ISUID,		/* 执行时文件时将用户的有效用户ID修改为文件所有者的User ID */
	SECONDFS_ISGID,		/* 执行时文件时将用户的有效组ID修改为文件所有者的Group ID */
	SECONDFS_ISVTX,		/* 使用后仍然位于交换区上的正文段 */
	SECONDFS_IREAD,		/* 对文件的读权限 */
	SECONDFS_IWRITE,	/* 对文件的写权限 */
	SECONDFS_IEXEC,		/* 对文件的执行权限 */
	SECONDFS_IRWXU,		/* 文件主对文件的读、写、执行权限 */
	SECONDFS_IRWXG,		/* 文件主同组用户对文件的读、写、执行权限 */
	SECONDFS_IRWXO		/* 其他用户对文件的读、写、执行权限 */
;

extern const s32
	SECONDFS_BLOCK_SIZE,	/* 文件逻辑块大小: 512字节 */
	SECONDFS_ADDRESS_PER_INDEX_BLOCK,	/* 每个间接索引表(或索引块)包含的物理盘块号 */
	SECONDFS_SMALL_FILE_BLOCK,	/* 小型文件：直接索引表最多可寻址的逻辑块号 */
	SECONDFS_LARGE_FILE_BLOCK,	/* 大型文件：经一次间接索引表最多可寻址的逻辑块号 */
	SECONDFS_HUGE_FILE_BLOCK,	/* 巨型文件：经二次间接索引最大可寻址文件逻辑块号 */
	SECONDFS_PIPSIZ
;

/* static member */
// 注意这是一个指针
extern s32 *secondfs_inode_rablockp;	/* 顺序读时，使用预读技术读入文件的下一字符块，rablock记录了下一逻辑块号
					经过bmap转换得到的物理盘块号。将rablock作为静态变量的原因：调用一次bmap的开销
					对当前块和预读块的逻辑块号进行转换，bmap返回当前块的物理盘块号，并且将预读块
					的物理盘块号保存在rablock中。 */

#define secondfs_inode_rablock (*secondfs_inode_rablockp)

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(Inode)
void Inode_ReadI(Inode *);
void Inode_IUpdate(Inode *i, int time);
void Inode_ICopy(Inode *i, Buf *bp, int inumber);


// DiskInode 类的 C 包装

#ifndef __cplusplus
typedef struct DiskInode
{
	u32		d_mode;			/* 状态的标志位，定义见enum INodeFlag */
	s32		d_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 */
	
	u16		d_uid;			/* 文件所有者的用户标识数 */
	u16		d_gid;			/* 文件所有者的组标识数 */
	
	s32		d_size;			/* 文件大小，字节为单位 */
	s32		d_addr[10];		/* 用于文件逻辑块号和物理块好转换的基本索引表 */

	s32		d_atime;		/* 最后访问时间 */
	s32		d_mtime;		/* 最后修改时间 */
} DiskInode;
#else // __cplusplus
class DiskInode;
#endif // __cplusplus

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(DiskInode)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __INODE_C_WRAPPER_H__