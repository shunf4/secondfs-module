/* UNIXV6PP 文件系统(主要是Buffer管理)代码裁剪. */
#include "../secondfs.h"
#include "../common.h"
#include "BufferManager.hh"
#include <stdio.h>
// @Feng Shun: 以下为 C++ 部分

/*======================class Devtab======================*/
Devtab::Devtab()
{
	this->d_active = 0;
	this->d_errcnt = 0;
	// 两对双向循环列表
	this->b_forw = (Buf *)this;
	this->b_back = (Buf *)this;
	// Devtab 的 I/O 请求队列, 我们不用
	this->d_actf = NULL;
	this->d_actl = NULL;
}

Devtab::~Devtab()
{
	//nothing to do here
}

/*======================class BufferManager======================*/
BufferManager::BufferManager()
{
	secondfs_c_helper_spin_lock_init(&this->b_queue_lock);
	secondfs_c_helper_sema_init(&this->b_bFreeList_lock, 1);
	//nothing to do here
}

BufferManager::~BufferManager()
{
	//nothing to do here
}

extern "C" void BufferManager_Initialize(BufferManager *bm) { bm->Initialize(); }
void BufferManager::Initialize()
{
	int i;
	Buf* bp;

	this->bFreeList.b_forw = this->bFreeList.b_back = &(this->bFreeList);
	this->bFreeList.av_forw = this->bFreeList.av_back = &(this->bFreeList);

	for(i = 0; i < SECONDFS_NBUF; i++)
	{
		bp = &(this->m_Buf[i]);
		bp->b_index = i;
		// Initially all Buf belongs to NODEV(NULL)
		// 最开始, 所有 Buf 的设备都是 NODEV (NULL)
		bp->b_dev = NULL;
		bp->b_addr = this->Buffer[i];
		/* Link them all into NODEV(bFreeList) */
		/* 初始化NODEV队列 */
		bp->b_back = &(this->bFreeList);
		bp->b_forw = this->bFreeList.b_forw;
		this->bFreeList.b_forw->b_back = bp;
		this->bFreeList.b_forw = bp;
		/* TODO: remove B_BUSY init? */
		/* 初始化自由队列 */
		bp->b_flags = Buf::B_BUSY;
		/* Initialize the MUTEXs */
		/* 初始化每 Buf 的两个 MUTEX */
		secondfs_c_helper_mutex_init(&bp->b_modify_lock);
		secondfs_c_helper_mutex_init(&bp->b_wait_free_lock);

		/* clear B_BUSY and other flags and put into bFreeList */
		Brelse(bp);
	}
	//this->m_DeviceManager = &Kernel::Instance().GetDeviceManager();
	return;
}

extern "C" Buf* BufferManager_GetBlk(BufferManager *bm, Devtab *dev, int blkno) { return bm->GetBlk(dev, blkno); }
Buf* BufferManager::GetBlk(Devtab *dev, int blkno)
{
	Buf* bp;
	int ret;

loop:
	secondfs_dbg(BUFFER, "searching Buf that matches dev %p and blkno %d", dev, blkno);
	/* Search block cache that match blkno in dev queue */
	/* 首先在该设备队列中搜索是否有相应的缓存 */

	// CLI/SLI in original UnixV6++ is changed to per Buf mutex here
	// Because in linux, kernel code is pre-emptive, we must lock the
	// whole queue first
	// 将这里的 CLI/SLI 改造成每 Buf 的 MUTEX
	
	for(bp = dev->b_forw; bp != (Buf *)dev; bp = bp->b_forw)
	{
		/* Not that one */
		/* 不是要找的缓存，则继续 */
		if(bp->b_blkno != blkno || bp->b_dev != dev)
			continue;

		secondfs_c_helper_mutex_lock(&bp->b_modify_lock);	// 这个是快锁
		// If the Buf is used for now, we must wait for it be freed
		// 我们在这里锁 wait_free_lock, 是为了等待其他正在
		// 使用该 Buf 的进程使用完毕.
		
		if(!secondfs_c_helper_mutex_trylock(&bp->b_wait_free_lock))
		{
			// If locked now
			secondfs_dbg(BUFFER, "searching Buf(%p/%d): found buf free-locked; wait", dev, blkno);
			// bp->b_flags |= Buf::B_WANTED;
			secondfs_c_helper_mutex_unlock(&bp->b_modify_lock);

			secondfs_c_helper_mutex_lock(&bp->b_wait_free_lock);	// 这个是慢锁
			// goto loop;

			// When we finally get the lock, blkno or dev of
			// this Buf may change. (Other free Buf may be used
			// for current blkno)
			// One more verification needed.
			// 拿到锁之后, 有可能这个 Buf 所属的设备和盘块都已发生变化.
			// 此时要加一次检测

			secondfs_dbg(BUFFER, "searching Buf(%p/%d): get lock after wait", dev, blkno);

			if (bp->b_blkno != blkno || bp->b_dev != dev || (bp->b_flags & Buf::B_BUSY)) {
				secondfs_dbg(BUFFER, "searching Buf(%p/%d): blkno/dev not matching anymore; loop", dev, blkno);
				secondfs_c_helper_mutex_unlock(&bp->b_wait_free_lock);
				goto loop;
			}
		} else {
			secondfs_dbg(BUFFER, "searching Buf(%p/%d): found buf free(not locked); acquired", dev, blkno);
			// The Buf has been locked successfully
		}
		/* Pull out from freeList */
		/* 从自由队列中抽取出来 */
		this->NotAvail(bp, 1);

		if (SFDBG_ENA(BUFFERQ)) {
			Print(dev);
		}

		secondfs_c_helper_mutex_unlock(&bp->b_modify_lock);
		
		return bp;
	}

	secondfs_dbg(BUFFER, "searching Buf(%p/%d): not found", dev, blkno);
	secondfs_c_helper_mutex_unlock(&this->bFreeList.b_modify_lock);

	/* 如果自由队列为空 */
	if(this->bFreeList.av_forw == &this->bFreeList)
	{
		secondfs_dbg(BUFFER, "allocating Buf(%p/%d): down() the semaphore", dev, blkno);
		//this->bFreeList.b_flags |= Buf::B_WANTED;

		// Take the freeList semaphore
		// Note: down_trylock returns 0 if succeed;
		// But mutex_trylock returns non-zero in the same case!

		// The semaphore has value 1 when there are freeBuf(s).
		// When the execution comes here, there is no freeBuf now;
		// thus trydown(assign it 0 if it is 1) then down(wait for
		// another process to wake).
		// In Brelse(), the semaphore is trydown(assign it 0 if 1)
		// then up(so that the value will not be greater than 1).
		// This will wake up processes waiting. If any, the down()
		// of that process will instantly decrease it to 0; If none,
		// the semaphore remains to be 1.
		secondfs_c_helper_down_trylock(&this->b_bFreeList_lock);
		secondfs_c_helper_down(&this->b_bFreeList_lock);

		// When successfully down the semaphore, a Buf must
		// be freed somewhere. Up it and Re-search.
		// Unlock (up) is not needed; b_bFreeList_lock
		// need to keep 0.
		// 拿到锁之后, bFreeList 的 av_forw, 以及各设备的 Buf 队列仍可能发生变化.
		// 要回到 loop 重新搜索设备缓存
		secondfs_dbg(BUFFER, "allocating Buf(%p/%d): after down(), go loop", dev, blkno);
		goto loop;
	}
	

	/* Fetch the first free Buf */
	/* 取自由队列第一个空闲块 */
	bp = this->bFreeList.av_forw;
	this->NotAvail(bp, 1);
	secondfs_dbg(BUFFER, "allocating Buf(%p/%d): NotAvail() Buf[%d]", dev, blkno, bp->b_index);

	if (SFDBG_ENA(BUFFERQ)) {
		Print(dev);
	}

	/* Write it to disk if it was dirty */
	/* 如果该字符块是延迟写，将其异步写到磁盘上 */
	// 改: 同步写到磁盘上
	if(bp->b_flags & Buf::B_DELWRI)
	{
		secondfs_dbg(BUFFER, "allocating Buf(%p/%d): Buf[%d] Busy", dev, blkno, bp->b_index);
		// bp->b_flags |= Buf::B_ASYNC;
		ret = this->Bwrite(bp);
		if (ret < 0) {
			secondfs_err("writing dirty Buf[%d/%p/%d] failed! errno: %d", bp->b_index, bp->b_dev, bp->b_blkno, ret);
		}
		// 由于是同步写, 这里还要加一个释放缓存块的操作
		this->Brelse(bp);
		secondfs_dbg(BUFFER, "allocating Buf(%p/%d): Buf[%d] Busy -> loop", dev, blkno, bp->b_index);
		goto loop;
	}

	// @Feng Shun : Linux 中这里也是临界区, 需要保护
	secondfs_c_helper_spin_lock(&this->b_queue_lock);

	/* 注意: 这里清除了所有其他位，只设了B_BUSY */
	//bp->b_flags = Buf::B_BUSY;
	bp->b_flags = 0;

	/* 从原设备队列中抽出 */
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
	/* 加入新的设备队列 */
	bp->b_forw = dev->b_forw;
	bp->b_back = (Buf *)dev;
	dev->b_forw->b_back = bp;
	dev->b_forw = bp;

	bp->b_dev = dev;
	bp->b_blkno = blkno;

	secondfs_c_helper_spin_unlock(&this->b_queue_lock);

	if (SFDBG_ENA(BUFFERQ)) {
		secondfs_dbg(BUFFERQ, "allocating Buf(%p/%d/[%d]): queue changed", dev, blkno, bp->b_index);
		Print(dev);
	}

	return bp;
}

extern "C" void BufferManager_Brelse(BufferManager *bm, Buf* bp) { bm->Brelse(bp); }
void BufferManager::Brelse(Buf* bp)
{
	/* 临界资源，比如：在同步读末期会调用这个函数，
	 * 此时很有可能会产生磁盘中断，同样会调用这个函数。
	 */

	// Spinlock for substitution of CLI/SLI
	// 替代 CLI/SLI 的方式 : BufferManager 内的自旋锁, 当然这不是等价的.
	secondfs_c_helper_spin_lock(&secondfs_buffermanagerp->b_queue_lock);

	/* 注意以下操作并没有清除B_DELWRI、B_WRITE、B_READ、B_DONE标志
	 * B_DELWRI表示虽然将该控制块释放到自由队列里面，但是有可能还没有些到磁盘上。
	 * B_DONE则是指该缓存的内容正确地反映了存储在或应存储在磁盘上的信息 
	 */
	bp->b_flags &= ~(Buf::B_WANTED | Buf::B_BUSY | Buf::B_ASYNC);
	(this->bFreeList.av_back)->av_forw = bp;
	bp->av_back = this->bFreeList.av_back;
	bp->av_forw = &(this->bFreeList);
	this->bFreeList.av_back = bp;
	
	secondfs_c_helper_spin_unlock(&secondfs_buffermanagerp->b_queue_lock);

	secondfs_dbg(BUFFER, "Brelse Buf[%d/%p/%d]", bp->b_index, bp->b_dev, bp->b_blkno);
	if (SFDBG_ENA(BUFFERQ)) {
		Print(bp->b_dev);
	}
	// Wake up all processes waiting this Buf to be free
	// 唤醒等待该缓存块的进程
	if (secondfs_c_helper_mutex_is_locked(&bp->b_wait_free_lock)) {
		secondfs_c_helper_mutex_unlock(&bp->b_wait_free_lock);
	}

	// Wake up processes those are waiting for freeBuf
	// 唤醒等待空闲缓存块的进程
	// 尝试 P, 再 V : 结果就是, 若信号量非正, 递增信号量; 否则不递增.
	secondfs_c_helper_down_trylock(&this->b_bFreeList_lock);
	secondfs_c_helper_up(&this->b_bFreeList_lock);

	return;
}

#if false
void BufferManager::IOWait(Buf* bp)
{
	User& u = Kernel::Instance().GetUser();

	/* 这里涉及到临界区
	 * 因为在执行这段程序的时候，很有可能出现硬盘中断，
	 * 在硬盘中断中，将会修改B_DONE如果此时已经进入循环
	 * 则将使得改进程永远睡眠
	 */
	X86Assembly::CLI();
	while( (bp->b_flags & Buf::B_DONE) == 0 )
	{
		u.u_procp->Sleep((unsigned long)bp, ProcessManager::PRIBIO);
	}
	X86Assembly::STI();

	this->GetError(bp);
	return;
}
#endif

extern "C" void BufferManager_IODone(BufferManager *bm, Buf* bp) { bm->IODone(bp); }
void BufferManager::IODone(Buf* bp)
{
	/* 置上I/O完成标志 */
	bp->b_flags |= Buf::B_DONE;
	if(bp->b_flags & Buf::B_ASYNC)
	{
		/* 如果是异步操作,立即释放缓存块 */
		// 不会被执行到, 这里没有异步操作
		// There is no asynchronized operations here
		// This branch will never be reached
		this->Brelse(bp);
	}
	else
	{
		/* 清除B_WANTED标志位 */
		// bp->b_flags &= (~Buf::B_WANTED);

		// No need to wake; IODone is called by current process
		// rather than DiskHandler.
		// 原 UNIXV6PP 中, IODone 由磁盘中断处理函数调用,
		// 会唤醒所有等待该 Buf I/O 完成的进程.
		// 本模块中, IODone 在每个 bio 请求后由进程调用,
		// 所以无需唤醒.
		// Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)bp);
	}
	secondfs_dbg(BUFFER, "IODone Buf[%d/%p/%d]", bp->b_index, bp->b_dev, bp->b_blkno);
	return;
}

extern "C" Buf* BufferManager_Bread(BufferManager *bm, Devtab *dev, int blkno) { return bm->Bread(dev, blkno); }
Buf* BufferManager::Bread(Devtab *dev, int blkno)
{
	Buf* bp;
	int ret;

	secondfs_dbg(BUFFER, "Bread Buf: %p/%d", dev, blkno);

	// Search for Buf in memory or allocate Buf in memory
	/* 根据设备号，字符块号申请缓存 */
	bp = this->GetBlk(dev, blkno);

	// Read done before
	/* 如果在设备队列中找到所需缓存，即B_DONE已设置，就不需进行I/O操作 */
	if(bp->b_flags & Buf::B_DONE)
	{
		return bp;
	}

	// Not found/Not read before; submit BIO
	/* 没有找到相应缓存，构成I/O读请求块 */
	bp->b_flags |= Buf::B_READ;
	bp->b_wcount = SECONDFS_BUFFER_SIZE;

	/* 
	 * 同步执行该 I/O 请求
	 */
	secondfs_dbg(BUFFER, "Bread Buf: %p/%d: submit bio", dev, blkno);

	ret = secondfs_submit_bio_sync_read(dev->d_bdev, bp->b_blkno, bp->b_addr);

	secondfs_dbg(BUFFER, "Bread Buf: %p/%d: after bio, ret=%d,"
	 	" content: %x %x %x %x %x %x %x %x ...", dev, blkno, ret,
		bp->b_addr[0],
		bp->b_addr[1],
		bp->b_addr[2],
		bp->b_addr[3],
		bp->b_addr[4],
		bp->b_addr[5],
		bp->b_addr[6],
		bp->b_addr[7]
	);

	this->IODone(bp);

	if (ret != 0) {
		// Returns the errno cast to a pointer
		// ERR_PTR(ret)
		Brelse(bp);
		return (Buf *)(intptr_t)ret;
	} else {
		return bp;
	}
}

#if false
Buf* BufferManager::Breada(short adev, int blkno, int rablkno)
{
	Buf* bp = NULL;	/* 非预读字符块的缓存Buf */
	Buf* abp;		/* 预读字符块的缓存Buf */
	short major = Utility::GetMajor(adev);	/* 主设备号 */

	/* 当前字符块是否已在设备Buf队列中 */
	if( !this->InCore(adev, blkno) )
	{
		bp = this->GetBlk(adev, blkno);		/* 若没找到，GetBlk()分配缓存 */
		
		/* 如果分配到缓存的B_DONE标志已设置，意味着在InCore()检查之后，
		 * 其它进程碰巧读取同一字符块，因而在GetBlk()中再次搜索的时候
		 * 发现该字符块已在设备Buf队列缓冲区中，本进程重用该缓存。*/
		if( (bp->b_flags & Buf::B_DONE) == 0 )
		{
			/* 构成读请求块 */
			bp->b_flags |= Buf::B_READ;
			bp->b_wcount = BufferManager::BUFFER_SIZE;
			/* 驱动块设备进行I/O操作 */
			this->m_DeviceManager->GetBlockDevice(major).Strategy(bp);
		}
	}
	else
		/*UNIX V6没这条语句。加入的原因：如果当前块在缓存池中，放弃预读
		 * 这是因为，预读的价值在于利用当前块和预读块磁盘位置大概率邻近的事实，
		 * 用预读操作减少磁臂移动次数提高有效磁盘读带宽。若当前块在缓存池，磁头不一定在当前块所在的位置，
		 * 此时预读，收益有限*/
		rablkno = 0;

	/* 预读操作有2点值得注意：
	 * 1、rablkno为0，说明UNIX打算放弃预读。
	 *      这是开销与收益的权衡
	 * 2、若预读字符块在设备Buf队列中，针对预读块的操作已经成功
	 * 		这是因为：
	 * 		作为预读块，并非是进程此次读盘的目的。
	 * 		所以如果不及时释放，将使得预读块一直得不到释放。
	 * 		而将其释放它依然存在在设备队列中，如果在短时间内
	 * 		使用这一块，那么依然可以找到。
	 * */
	if( rablkno && !this->InCore(adev, rablkno) )
	{
		abp = this->GetBlk(adev, rablkno);	/* 若没找到，GetBlk()分配缓存 */
		
		/* 检查B_DONE标志位，理由同上。 */
		if(abp->b_flags & Buf::B_DONE)
		{
			/* 预读字符块已在缓存中，释放占用的缓存。
			 * 因为进程未必后面一定会使用预读的字符块，
			 * 也就不会去释放该缓存，有可能导致缓存资源
			 * 的长时间占用。
			 */
			this->Brelse(abp);
		}
		else
		{
			/* 异步读预读字符块 */
			abp->b_flags |= (Buf::B_READ | Buf::B_ASYNC);
			abp->b_wcount = BufferManager::BUFFER_SIZE;
			/* 驱动块设备进行I/O操作 */
			this->m_DeviceManager->GetBlockDevice(major).Strategy(abp);
		}
	}
	
	/* bp == NULL意味着InCore()函数检查时刻，非预读块在设备队列中，
	 * 但是InCore()只是“检查”，并不“摘取”。经过一段时间执行到此处，
	 * 有可能该字符块已经重新分配它用。
	 * 因而重新调用Bread()重读字符块，Bread()中调用GetBlk()将字符块“摘取”
	 * 过来。短时间内该字符块仍在设备队列中，所以此处Bread()一般也就是将
	 * 缓存重用，而不必重新执行一次I/O读取操作。
	 */
	if(NULL == bp)
	{
		return (this->Bread(adev, blkno));
	}
	
	/* InCore()函数检查时刻未找到非预读字符块，等待I/O操作完成 */
	this->IOWait(bp);
	return bp;
}
#endif

extern "C" int BufferManager_Bwrite(BufferManager *bm, Buf *bp) { return bm->Bwrite(bp); }
int BufferManager::Bwrite(Buf *bp)
{
	unsigned int flags;
	int ret;

	flags = bp->b_flags;
	bp->b_flags &= ~(Buf::B_READ | Buf::B_DONE | Buf::B_ERROR | Buf::B_DELWRI);
	bp->b_wcount = SECONDFS_BUFFER_SIZE;		/* 512字节 */

	// Sync write
	// 同步写

	secondfs_dbg(BUFFER, "Bwrite Buf[%d/%p/%d]: submit bio", bp->b_index, bp->b_dev, bp->b_blkno);

	ret = secondfs_submit_bio_sync_write(bp->b_dev->d_bdev, bp->b_blkno, bp->b_addr);

	secondfs_dbg(BUFFER, "Bwrite Buf[%d/%p/%d]: after bio, ret=%d", bp->b_index, bp->b_dev, bp->b_blkno, ret);

	this->IODone(bp);

	if( (flags & Buf::B_ASYNC) == 0 )
	{
		// After synchronized writing, 
		// the Buf is released.
		this->Brelse(bp);
	}
	else if( (flags & Buf::B_DELWRI) == 0)
	{
		// We do not check error here.
		// Instead we return the errno.
	/* 
	 * 如果不是延迟写，则检查错误；否则不检查。
	 * 这是因为如果延迟写，则很有可能当前进程不是
	 * 操作这一缓存块的进程，而在GetError()主要是
	 * 给当前进程附上错误标志。
	 */
	// 为简化, 这里不再置错误标志
		// this->GetError(bp);
	}

	return ret;
}

extern "C" void BufferManager_Bdwrite(BufferManager *bm, Buf *bp) { bm->Bdwrite(bp); }
void BufferManager::Bdwrite(Buf *bp)
{
	/* 置上B_DONE允许其它进程使用该磁盘块内容 */
	secondfs_dbg(BUFFER, "Bdwrite Buf[%d/%p/%d]", bp->b_index, bp->b_dev, bp->b_blkno);
	bp->b_flags |= (Buf::B_DELWRI | Buf::B_DONE);
	this->Brelse(bp);
	return;
}

#if false
void BufferManager::Bawrite(Buf *bp)
{
	/* 标记为异步写 */
	bp->b_flags |= Buf::B_ASYNC;
	this->Bwrite(bp);
	return;
}
#endif

extern "C" void BufferManager_ClrBuf(BufferManager *bm, Buf *bp) { bm->ClrBuf(bp); }
void BufferManager::ClrBuf(Buf *bp)
{
	s32* pInt = (s32 *)bp->b_addr;

	/* 将缓冲区中数据清零 */
	for(unsigned int i = 0; i < SECONDFS_BUFFER_SIZE / sizeof(s32); i++)
	{
		pInt[i] = 0;
	}
	return;
}

extern "C" void BufferManager_Bflush(BufferManager *bm, Devtab *dev) { bm->Bflush(dev); }
void BufferManager::Bflush(Devtab *dev)
{
	Buf* bp;
	/* 注意：这里之所以要在搜索到一个块之后重新开始搜索，
	 * 因为在bwite()进入到驱动程序中时有开中断的操作，所以
	 * 等到bwrite执行完成后，CPU已处于开中断状态，所以很
	 * 有可能在这期间产生磁盘中断，使得bfreelist队列出现变化，
	 * 如果这里继续往下搜索，而不是重新开始搜索那么很可能在
	 * 操作bfreelist队列的时候出现错误。
	 */
	int ret;
loop:
	// 替代 CLI/SLI 的方式 : BufferManager 内的自旋锁, 当然这不是等价的.
	secondfs_c_helper_spin_lock(&secondfs_buffermanagerp->b_queue_lock);
	secondfs_dbg(BUFFER, "BufferManager::Bflush: finding dirty Buf...");
	for(bp = this->bFreeList.av_forw; bp != &(this->bFreeList); bp = bp->av_forw)
	{
		/* Find all dirty buffers */
		/* 找出自由队列中所有延迟写的块 */
		if( (bp->b_flags & Buf::B_DELWRI) && (dev == 0 || dev == bp->b_dev) )
		{
			secondfs_dbg(BUFFER, "writing dirty Buf[%d/%p/%d|%X]...", bp->b_index, bp->b_dev, bp->b_blkno, bp->b_flags);
			/* Sync write them to disk
			 */
			/* @Feng Shun: 在原 Unix V6++ 中, 对于所有脏块,
			 * 采取的操作仅为"异步写"
			 * 此处改为 "同步写"
			 */
			// bp->b_flags |= Buf::B_ASYNC;

			// NotAvail will unlock queue lock
			// 在 NotAvail 中会把自旋锁解锁
			this->NotAvail(bp, 0);
			ret = this->Bwrite(bp);

			if (ret < 0) {
				secondfs_err("writing dirty Buf[%d/%p/%d] failed! errno: %d", bp->b_index, bp->b_dev, bp->b_blkno, ret);
			}

			goto loop;
		}
	}
	secondfs_c_helper_spin_unlock(&secondfs_buffermanagerp->b_queue_lock);
	return;
}

#if false
bool BufferManager::Swap(int blkno, unsigned long addr, int count, enum Buf::BufFlag flag)
{
	User& u = Kernel::Instance().GetUser();

	X86Assembly::CLI();

	/* swbuf正在被其它进程使用，则睡眠等待 */
	while ( this->SwBuf.b_flags & Buf::B_BUSY )
	{
		this->SwBuf.b_flags |= Buf::B_WANTED;
		u.u_procp->Sleep((unsigned long)&SwBuf, ProcessManager::PSWP);
	}

	this->SwBuf.b_flags = Buf::B_BUSY | flag;
	this->SwBuf.b_dev = DeviceManager::ROOTDEV;
	this->SwBuf.b_wcount = count;
	this->SwBuf.b_blkno = blkno;
	/* b_addr指向要传输部分的内存首地址 */
	this->SwBuf.b_addr = (unsigned char *)addr;
	this->m_DeviceManager->GetBlockDevice(Utility::GetMajor(this->SwBuf.b_dev)).Strategy(&this->SwBuf);

	/* 关中断进行B_DONE标志的检查 */
	X86Assembly::CLI();
	/* 这里Sleep()等同于同步I/O中IOWait()的效果 */
	while ( (this->SwBuf.b_flags & Buf::B_DONE) == 0 )
	{
		u.u_procp->Sleep((unsigned long)&SwBuf, ProcessManager::PSWP);
	}

	/* 这里Wakeup()等同于Brelse()的效果 */
	if ( this->SwBuf.b_flags & Buf::B_WANTED )
	{
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)&SwBuf);
	}
	X86Assembly::STI();
	this->SwBuf.b_flags &= ~(Buf::B_BUSY | Buf::B_WANTED);

	if ( this->SwBuf.b_flags & Buf::B_ERROR )
	{
		return false;
	}
	return true;
}

void BufferManager::GetError(Buf* bp)
{
	User& u = Kernel::Instance().GetUser();

	if (bp->b_flags & Buf::B_ERROR)
	{
		u.u_error = User::EIO;
	}
	return;
}
#endif

extern "C" void BufferManager_NotAvail(BufferManager *bm, Buf *bp, u32 lockFirst) { bm->NotAvail(bp, lockFirst); }
void BufferManager::NotAvail(Buf *bp, u32 lockFirst)
{
	if (lockFirst) 
		secondfs_c_helper_spin_lock(&this->b_queue_lock);
	/* 从自由队列中取出 */
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
	/* 设置B_BUSY标志 */
	//bp->b_flags |= Buf::B_BUSY;
	secondfs_c_helper_spin_unlock(&this->b_queue_lock);

	return;
}

extern "C" Buf* BufferManager_InCore(BufferManager *bm, Devtab *adev, int blkno) { return bm->InCore(adev, blkno); }
Buf* BufferManager::InCore(Devtab *adev, int blkno)
{
	Buf* bp;
	Devtab* dp;
	if (adev)
		dp = adev;
	else
		dp = (Devtab *)&this->bFreeList;

	for(bp = dp->b_forw; bp != (Buf *)dp; bp = bp->b_forw)
	{
		if(bp->b_blkno == blkno && bp->b_dev == adev)
			return bp;
	}
	return NULL;
}

extern "C" void BufferManager_Print(BufferManager *bm, Devtab *dev) { bm->Print(dev); }
void BufferManager::Print(Devtab *dev)
{
	const int buflen = 1000;
	char buf[buflen];

	int length = 0;

	length += secondfs_c_helper_sprintf(buf + length, "bFreeList NODEV:");
	Buf *bp = bFreeList.b_forw;
	do {
		if (!bp) {
			length += secondfs_c_helper_sprintf(buf + length, "(NULL)");
			break;
		}
		length += secondfs_c_helper_sprintf(buf + length, "[%d/%p/%u]->", bp->b_index, bp->b_dev, bp->b_blkno);
		bp = bp->b_forw;
	} while (bp != &bFreeList);
	length += secondfs_c_helper_sprintf(buf + length, "\n");

	length += secondfs_c_helper_sprintf(buf + length, "bFreeList FREE:");
	bp = bFreeList.av_forw;
	do {
		if (!bp) {
			length += secondfs_c_helper_sprintf(buf + length, "(NULL)");
			break;
		}
		length += secondfs_c_helper_sprintf(buf + length, "[%d/%p/%u]->", bp->b_index, bp->b_dev, bp->b_blkno);
		bp = bp->av_forw;
	} while (bp != &bFreeList);
	length += secondfs_c_helper_sprintf(buf + length, "\n");

	length += secondfs_c_helper_sprintf(buf + length, "Devtab %p DEVBUFS:", dev);
	if (dev) {
		bp = dev->b_forw;
		do {
			if (!bp) {
				length += secondfs_c_helper_sprintf(buf + length, "(NULL)");
				break;
			}
			length += secondfs_c_helper_sprintf(buf + length, "[%d/%p/%u]->", bp->b_index, bp->b_dev, bp->b_blkno);
			bp = bp->b_forw;
		} while (bp != (Buf *)dev);
	}

	secondfs_dbg(BUFFERQ, "%s", buf);
}


#if false
Buf& BufferManager::GetSwapBuf()
{
	return this->SwBuf;
}

Buf& BufferManager::GetBFreeList()
{
	return this->bFreeList;
}
#endif


// @Feng Shun: 以下为 C wrapping 部分
extern "C" {
	const u32
		SECONDFS_B_WRITE = Buf::BufFlag::B_WRITE,	/* 写操作。将缓存中的信息写到硬盘上去 */
		SECONDFS_B_READ = Buf::BufFlag::B_READ,		/* 读操作。从盘读取信息到缓存中 */
		SECONDFS_B_DONE = Buf::BufFlag::B_DONE,		/* I/O操作结束 */
		SECONDFS_B_ERROR = Buf::BufFlag::B_ERROR,	/* I/O因出错而终止 */
		SECONDFS_B_BUSY = Buf::BufFlag::B_BUSY,		/* 相应缓存正在使用中 Not used */
		SECONDFS_B_WANTED = Buf::BufFlag::B_WANTED,	/* 有进程正在等待使用该buf管理的资源，清B_BUSY标志SECONDFS_时，要唤醒这种进程 */
		SECONDFS_B_ASYNC = Buf::BufFlag::B_ASYNC,	/* 异步I/O，不需要等待其结束 Not used */
		SECONDFS_B_DELWRI = Buf::BufFlag::B_DELWRI	/* 延迟写，在相应缓存要移做他用时，再将其内容写到相应块设备上 Dirty */
	;

	SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(Buf);
	SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(BufferManager);
	SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(Devtab);


}