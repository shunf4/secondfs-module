#ifndef __BUFFERMANAGER_HH__
#define __BUFFERMANAGER_HH__

#include <cstdint>
#include <cstddef>
#include "../common_c_cpp_types.h"
#include "../c_helper_for_cc.h"

#include "BufferManager_c_wrapper.h"

class Buf;

/* 块设备表devtab定义 */
class Devtab
{
public:
	Devtab();
	~Devtab();
	
public:
	s32	d_active;
	s32	d_errcnt;
	Buf*	b_forw;
	Buf*	b_back;
	Buf*	d_actf;
	Buf*	d_actl;

	void * /* struct block_device* */	d_bdev;
};

/*
 * 缓存控制块buf定义
 * 记录了相应缓存的使用情况等信息；
 * 同时兼任I/O请求块，记录该缓存
 * 相关的I/O请求和执行结果。
 */
class Buf
{
public:
	enum BufFlag	/* b_flags中标志位 */
	{
		B_WRITE = 0x1,		/* 写操作。将缓存中的信息写到硬盘上去 */
		B_READ	= 0x2,		/* 读操作。从盘读取信息到缓存中 */
		B_DONE	= 0x4,		/* I/O操作结束 */
		B_ERROR	= 0x8,		/* I/O因出错而终止 */
		B_BUSY	= 0x10,		/* 相应缓存正在使用中 */
		B_WANTED = 0x20,	/* 有进程正在等待使用该buf管理的资源，清B_BUSY标志时，要唤醒这种进程 */
		B_ASYNC	= 0x40,		/* 异步I/O，不需要等待其结束 */
		B_DELWRI = 0x80		/* 延迟写，在相应缓存要移做他用时，再将其内容写到相应块设备上 */
	};
	
public:
	unsigned int b_flags;	/* 缓存控制块标志位 */
	
	int		padding;		/* 4字节填充，使得b_forw和b_back在Buf类中与Devtab类
							 * 中的字段顺序能够一致，否则强制转换会出错。 */
	/* 缓存控制块队列勾连指针 */

	/* @Feng Shun: 
	 * 从整个 struct 开始, 直到勾连指针的部分, Buf 和 Devtab 的结构都是一样的.
	 * 因此, 可以将 Buf * 和 Devtab 相互强制转换, 以进行前面部分的读取和写入.
	 * 
	 * 对于 Devtab 来说, 这四个(两对)指针的前一对串联起该设备对应指针;
	 * 后一对串联起该设备的 I/O 请求队列 (在本项目中不用).
	 * 
	 * 对于 bFreeList 来说, 前一对串联起 NODEV 设备对应指针 (本项目中不用到 NODEV);
	 * 后一对串联起自由缓存队列.
	 */
	Buf*	b_forw;
	Buf*	b_back;
	Buf*	av_forw;
	Buf*	av_back;
	
	Devtab*		b_dev;			/* 所属的设备 */
	s32		b_wcount;		/* 需传送的字节数 */
	u8* 		b_addr;			/* 指向该缓存控制块所管理的缓冲区的首地址 */
	s32		b_blkno;		/* 磁盘逻辑块号 */
	s32		b_error;		/* I/O出错时信息 */
	s32		b_resid;		/* I/O出错时尚未传送的剩余字节数 */

	struct {u8 data[SECONDFS_MUTEX_SIZE];}	b_modify_lock;
	struct {u8 data[SECONDFS_MUTEX_SIZE];}	b_wait_free_lock;
};

class BufferManager
{
public:
#if false
	/* static const member */
	static const int NBUF = 15;			/* 缓存控制块、缓冲区的数量 */
	static const int BUFFER_SIZE = 512;	/* 缓冲区大小。 以字节为单位 */
#endif

public:
	BufferManager();
	~BufferManager();
	
	void Initialize();			/* 缓存控制块队列的初始化。将缓存控制块中b_addr指向相应缓冲区首地址。*/
	
	Buf* GetBlk(Devtab *dev, int blkno);	/* 申请一块缓存，用于读写设备dev上的字符块blkno。*/
	void Brelse(Buf* bp);			/* 释放缓存控制块buf */
	void IOWait(Buf* bp);			/* 同步方式I/O，等待I/O操作结束 */
	void IODone(Buf* bp);			/* I/O操作结束善后处理 */

	Buf* Bread(Devtab *dev, int blkno);	/* 读一个磁盘块。dev为主、次设备号，blkno为目标磁盘块逻辑块号。 */
	Buf* Breada(short adev, int blkno, int rablkno);	/* 读一个磁盘块，带有预读方式。
								* adev为主、次设备号。blkno为目标磁盘块逻辑块号，同步方式读blkno。
								* rablkno为预读磁盘块逻辑块号，异步方式读rablkno。 */
	void Bwrite(Buf* bp);			/* 写一个磁盘块 */
	void Bdwrite(Buf* bp);			/* 延迟写磁盘块 */
	void Bawrite(Buf* bp);			/* 异步写磁盘块 */

	void ClrBuf(Buf* bp);			/* 清空缓冲区内容 */
	void Bflush(Devtab *dev);			/* 将dev指定设备队列中延迟写的缓存全部输出到磁盘 */
	bool Swap(Devtab *blkno, unsigned long addr, int count, enum Buf::BufFlag flag);
						/* Swap I/O 用于进程图像在内存和盘交换区之间传输
							* blkno: 交换区中盘块号；addr:  进程图像(传送部分)内存起始地址；
							* count: 进行传输字节数，byte为单位；传输方向flag: 内存->交换区 or 交换区->内存。 */
	Buf& GetSwapBuf();			/* 获取进程图像传送请求块Buf对象引用 */
	Buf& GetBFreeList();			/* 获取自由缓存队列控制块Buf对象引用 */

public:
	void GetError(Buf* bp);			/* 获取I/O操作中发生的错误信息 */
	void NotAvail(Buf* bp, u32 lockFirst);	/* 从自由队列中摘下指定的缓存控制块buf */
	Buf* InCore(Devtab *adev, int blkno);	/* 检查指定字符块是否已在缓存中 */
	
public:
	Buf bFreeList;					/* 自由缓存队列控制块 */
	Buf SwBuf;					/* 进程图像传送请求块 */
	Buf m_Buf[SECONDFS_NBUF];			/* 缓存控制块数组 */
	u8 Buffer[SECONDFS_NBUF][SECONDFS_BUFFER_SIZE];	/* 缓冲区数组 */
	
	//DeviceManager* m_DeviceManager;		/* 指向设备管理模块全局对象 */

	struct {u8 data[SECONDFS_SPINLOCK_T_SIZE];}	b_queue_lock;		// 保护整个缓存块队列的自旋锁
	struct {u8 data[SECONDFS_SEMAPHORE_SIZE];}	b_bFreeList_lock;	// 表征是否有自由缓存的信号量
};

#endif // __BUFFERMANAGER_HH__