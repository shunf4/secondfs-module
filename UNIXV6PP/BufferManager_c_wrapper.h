#ifndef __BUFFERMANAGER_C_WRAPPER_H__
#define __BUFFERMANAGER_C_WRAPPER_H__

#include "../common_c_cpp_types.h"
#include <linux/blkdev.h>
#include <linux/mutex.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Devtab 类的 C 包装
#ifndef __cplusplus
typedef struct{
	s32	d_active;
	s32	d_errcnt;
	Buf*	b_forw;
	Buf*	b_back;
	Buf*	d_actf;
	Buf*	d_actl;

	struct block_device*	d_bdev;
} Devtab;
#else // __cplusplus
class Devtab;
#endif // __cplusplus

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(Devtab)

// Buf 类的 C 包装

#ifndef __cplusplus
typedef struct
{
	u32	b_flags;	/* 缓存控制块标志位 */
	
	s32	padding;	/* 4字节填充，使得b_forw和b_back在Buf类中与Devtab类
				* 中的字段顺序能够一致，否则强制转换会出错。
				* @Feng Shun: 此处不需要 */
	/* 缓存控制块队列勾连指针 */
	/* @Feng Shun: 其中, 只有 av_forw 和 av_back 有用.
	*  并且, av_forw 和 av_back 在原 UnixV6++ 中还会作 I/O 请求队列串联用, 此处不用  */
	Buf*	b_forw;
	Buf*	b_back;
	Buf*	av_forw;
	Buf*	av_back;
	
	Devtab		*b_dev;		/* 所属的设备 */
	s32		b_wcount;	/* 需传送的字节数 */
	u8* 		b_addr;		/* 指向该缓存控制块所管理的缓冲区的首地址 */
	s32		b_blkno;	/* 磁盘逻辑块号 */
	s32		b_error;	/* I/O出错时信息 */
	s32		b_resid;	/* I/O出错时尚未传送的剩余字节数 */

	struct mutex	modify_lock;
	struct mutex	wait_free_lock;
} Buf;

// static size_t x = sizeof(Buf);

#else // __cplusplus
class Buf;
#endif // __cplusplus

extern const u32
	SECONDFS_B_WRITE,	/* 写操作。将缓存中的信息写到硬盘上去 */
	SECONDFS_B_READ,		/* 读操作。从盘读取信息到缓存中 */
	SECONDFS_B_DONE,		/* I/O操作结束 */
	SECONDFS_B_ERROR,	/* I/O因出错而终止 */
	SECONDFS_B_BUSY,		/* 相应缓存正在使用中 */
	SECONDFS_B_WANTED,	/* 有进程正在等待使用该buf管理的资源，清B_BUSY标志SECONDFS_时，要唤醒这种进程 */
	SECONDFS_B_ASYNC,	/* 异步I/O，不需要等待其结束 */
	SECONDFS_B_DELWRI	/* 延迟写，在相应缓存要移做他用时，再将其内容写到相应块设备上 */
;

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(Buf)

// BufferManager 类的 C 包装

// 总共可以分配多少个缓冲块
#define SECONDFS_NBUF 60
// 缓冲块的大小, 应该等于扇区大小
#define SECONDFS_BUFFER_SIZE 512

#ifndef __cplusplus
typedef struct
{
	Buf bFreeList;					/* 自由缓存队列控制块 */
	Buf SwBuf;					/* 进程图像传送请求块 */
	Buf m_Buf[SECONDFS_NBUF];			/* 缓存控制块数组 */
	u8 Buffer[SECONDFS_NBUF][SECONDFS_BUFFER_SIZE];	/* 缓冲区数组 */
	
	//DeviceManager* m_DeviceManager;		/* 指向设备管理模块全局对象 */

	spinlock_t	lock;		// 保护整个缓存块队列的自旋锁
	semaphore	bFreeLock;	// 表征是否有自由缓存的信号量
} BufferManager;
#else // __cplusplus
class BufferManager;
#endif // __cplusplus


SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(BufferManager)

void BufferManager_Initialize(BufferManager *bm);
Buf* BufferManager_GetBlk(BufferManager *bm, Devtab *dev, int blkno);
void BufferManager_Brelse(BufferManager *bm, Buf* bp);
void BufferManager_IODone(BufferManager *bm, Buf* bp);
Buf* BufferManager_Bread(BufferManager *bm, Devtab *dev, int blkno);
void BufferManager_Bwrite(BufferManager *bm, Buf *bp);
void BufferManager_NotAvail(BufferManager *bm, Buf *bp);
Buf* BufferManager_InCore(BufferManager *bm, Devtab *adev, int blkno);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __BUFFERMANAGER_C_WRAPPER_H__