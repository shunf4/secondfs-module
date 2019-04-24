#ifndef __INODE_C_WRAPPER_H__
#define __INODE_C_WRAPPER_H__

#ifndef __cplusplus
#include <linux/types.h>
#endif // __cplusplus

#ifdef __cplusplus
#include <cstdint>
typedef uint32_t u32;
typedef int32_t s32;
typedef int16_t s16;
extern "C" {
#endif // __cplusplus

typedef struct Inode Inode;
extern const u32 SECONDFS_INODE_SIZE;

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

Inode *newInode(void);
void deleteInode(Inode *);
void Inode_ReadI(Inode *);




typedef struct DiskInode DiskInode;
extern const u32 SECONDFS_DISKINODE_SIZE;

DiskInode *newDiskInode(void);
void deleteDiskInode(DiskInode *);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __INODE_C_WRAPPER_H__