/* UNIXV6PP 文件系统(主要是超块操作)代码裁剪. */
#include <cstring>
#include <cstdio>
#include "../common.h"
#include "../secondfs.h"

#include "BufferManager.hh"
#include "Inode.hh"
#include "FileSystem.hh"

// @Feng Shun: 以下为 C++ 部分

/*======================class Superblock======================*/
SuperBlock::SuperBlock()
{
	secondfs_c_helper_mutex_init(&this->s_update_lock);
	secondfs_c_helper_mutex_init(&this->s_flock);
	secondfs_c_helper_mutex_init(&this->s_ilock);
	//nothing to do here
}

SuperBlock::~SuperBlock()
{
	//nothing to do here
}

FileSystem::FileSystem()
{
	//nothing to do here
}

FileSystem::~FileSystem()
{
	//nothing to do here
}

/*======================class FileSystem======================*/
extern "C" void FileSystem_Initialize(FileSystem *fs) { fs->Initialize(); }
void FileSystem::Initialize()
{
	this->m_BufferManager = this->m_BufferManager;
	//this->updlock = 0;
}

extern "C" int FileSystem_LoadSuperBlock(FileSystem *fs, SuperBlock *secsb) { return fs->LoadSuperBlock(secsb); }
// 注: 只是把裸数据从外存获取到 SuperBlock 结构, 并未做任何
// Endian 的转换! Unix V6++ 卷的所有多字节数据都以小端序存放
int FileSystem::LoadSuperBlock(SuperBlock *secsb)
{
	BufferManager& bufMgr = *this->m_BufferManager;
	Buf* pBuf;

	for (int i = 0; i < 2; i++)
	{
		u8* p = (u8 *)secsb + i * SECONDFS_BLOCK_SIZE;

		pBuf = bufMgr.Bread(secsb->s_dev, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);

		// We just hard-code IS_ERR() macro here
		if ((uintptr_t)(pBuf) >= (uintptr_t)-4095) {
			secondfs_err("reading Buf: %p/%d failed! errno: %d", secsb->s_dev, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i, (int)(intptr_t)pBuf);
			return (int)(intptr_t)pBuf;
		}

		memcpy(p, pBuf->b_addr, SECONDFS_BLOCK_SIZE);

		bufMgr.Brelse(pBuf);
	}

	if ((s32)secondfs_c_helper_le32_to_cpu(secsb->s_nfree) < 0 || (s32)secondfs_c_helper_le32_to_cpu(secsb->s_nfree) > 100) {
		secondfs_err("Validating SuperBlock: secsb->s_nfree == %d, corrupted!", (s32)secondfs_c_helper_le32_to_cpu(secsb->s_nfree));
		return -EINVAL;
	}

	if ((s32)secondfs_c_helper_le32_to_cpu(secsb->s_ninode) < 0 || (s32)secondfs_c_helper_le32_to_cpu(secsb->s_ninode) > 100) {
		secondfs_err("Validating SuperBlock: secsb->s_ninode == %d, corrupted!", (s32)secondfs_c_helper_le32_to_cpu(secsb->s_ninode));
		return -EINVAL;
	}

	//secsb->s_flock = 0;
	//secsb->s_ilock = 0;
	//secsb->s_ronly = 0;

	secsb->s_time = cpu_to_le32(secondfs_c_helper_ktime_get_real_seconds());
	return 0;
}

extern "C" void FileSystem_PrintSuperBlock(FileSystem *fs, SuperBlock *secsb) { fs->PrintSuperBlock(secsb); }
void FileSystem::PrintSuperBlock(SuperBlock *secsb)
{
	const int buflen = 1000;
	char buf[buflen];

	int length = 0;

	length += secondfs_c_helper_sprintf(buf + length, "SuperBlock %p:\n", secsb);

	length += secondfs_c_helper_sprintf(buf + length, "s_isize(Inode area blocks): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_isize));
	length += secondfs_c_helper_sprintf(buf + length, "s_fsize(Data area blocks): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_fsize));
	length += secondfs_c_helper_sprintf(buf + length, "s_nfree(Freeblock stack height): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_nfree));
	length += secondfs_c_helper_sprintf(buf + length, "top elements of s_free: %d %d %d %d %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_free[secondfs_c_helper_le32_to_cpu(secsb->s_nfree) - 1]), secondfs_c_helper_le32_to_cpu(secsb->s_free[secondfs_c_helper_le32_to_cpu(secsb->s_nfree) - 2]), secondfs_c_helper_le32_to_cpu(secsb->s_free[secondfs_c_helper_le32_to_cpu(secsb->s_nfree) - 3]), secondfs_c_helper_le32_to_cpu(secsb->s_free[secondfs_c_helper_le32_to_cpu(secsb->s_nfree) - 4]), secondfs_c_helper_le32_to_cpu(secsb->s_free[secondfs_c_helper_le32_to_cpu(secsb->s_nfree) - 5]));
	length += secondfs_c_helper_sprintf(buf + length, "s_ninode(Freeinode stack height): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_ninode));
	length += secondfs_c_helper_sprintf(buf + length, "top elements of s_inode: %d %d %d %d %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_inode[secondfs_c_helper_le32_to_cpu(secsb->s_ninode) - 1]), secondfs_c_helper_le32_to_cpu(secsb->s_inode[secondfs_c_helper_le32_to_cpu(secsb->s_ninode) - 2]), secondfs_c_helper_le32_to_cpu(secsb->s_inode[secondfs_c_helper_le32_to_cpu(secsb->s_ninode) - 3]), secondfs_c_helper_le32_to_cpu(secsb->s_inode[secondfs_c_helper_le32_to_cpu(secsb->s_ninode) - 4]), secondfs_c_helper_le32_to_cpu(secsb->s_inode[secondfs_c_helper_le32_to_cpu(secsb->s_ninode) - 5]));

	length += secondfs_c_helper_sprintf(buf + length, "s_has_dots(This fs has . & ..?): 0x%X\n", secondfs_c_helper_le32_to_cpu(secsb->s_has_dots));

	length += secondfs_c_helper_sprintf(buf + length, "s_fmod(SuperBlock modified): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_fmod));

	length += secondfs_c_helper_sprintf(buf + length, "s_ronly(SuperBlock read-only): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_ronly));
	length += secondfs_c_helper_sprintf(buf + length, "s_time(Last update): %d\n", secondfs_c_helper_le32_to_cpu(secsb->s_time));

	length += secondfs_c_helper_sprintf(buf + length, "Root Inode: %p\n", secsb->s_inodep);
	length += secondfs_c_helper_sprintf(buf + length, "Devtab: %p\n", secsb->s_dev);
	length += secondfs_c_helper_sprintf(buf + length, "VFS super_block: %p\n", secsb->s_vsb);

	secondfs_dbg(SB_FILL, "%s", buf);
}

#if false
SuperBlock* FileSystem::GetFS(short dev)
{
	SuperBlock* sb;
	
	/* 遍历系统装配块表，寻找设备号为dev的设备中文件系统的SuperBlock */
	for(int i = 0; i < FileSystem::NMOUNT; i++)
	{
		if(this->m_Mount[i].m_spb != NULL && this->m_Mount[i].m_dev == dev)
		{
			/* 获取SuperBlock的地址 */
			sb = this->m_Mount[i].m_spb;
			if(sb->s_nfree > 100 || sb->s_ninode > 100)
			{
				sb->s_nfree = 0;
				sb->s_ninode = 0;
			}
			return sb;
		}
	}

	Utility::Panic("No File System!");
	return NULL;
}
#endif

extern "C" void FileSystem_Update(FileSystem *fs, SuperBlock *secsb) { fs->Update(secsb); }
void FileSystem::Update(SuperBlock *secsb)
{
	SuperBlock* sb = secsb;
	Buf* pBuf;
	int ret;

	/* 设置Update()函数的互斥锁，防止其它进程重入 */
	/* 另一进程正在进行同步，则直接返回 */
	// @Feng Shun: 注: 这里从 Univ V6++ 的每文件系统锁改为了每 SuperBlock 的锁
	if (!secondfs_c_helper_mutex_trylock(&secsb->s_update_lock))
	{
		secondfs_dbg(GENERAL, "FileSystem::Update: SB locked.");
		return;
	}

	/* 同步SuperBlock到磁盘 */
	/* 如果该SuperBlock内存副本没有被修改，直接管理inode和空闲盘块被上锁或该文件系统是只读文件系统 */
	if(le32_to_cpu(sb->s_fmod) == 0 || secondfs_c_helper_mutex_is_locked(&sb->s_ilock) || secondfs_c_helper_mutex_is_locked(&sb->s_flock) || le32_to_cpu(sb->s_ronly) != 0)
	{
		secondfs_dbg(GENERAL, "FileSystem::Update: SB not modified.");
		goto out;
	}

	/* Clear modify flag */
	/* 清SuperBlock修改标志 */
	sb->s_fmod = 0;
	/* Update last access time */
	/* 写入SuperBlock最后存访时间 */
	sb->s_time = cpu_to_le32(secondfs_c_helper_ktime_get_real_seconds());

	/* 
	* 为将要写回到磁盘上去的SuperBlock申请一块缓存，由于缓存块大小为512字节，
	* SuperBlock大小为1024字节，占据2个连续的扇区，所以需要2次写入操作。
	*/

	// Sync 1024 bytes(2 blocks) back to disk
	for(int j = 0; j < SECONDFS_SUPER_BLOCK_DISK_SIZE / SECONDFS_BLOCK_SIZE; j++)
	{
		/* 第一次p指向SuperBlock的第0字节，第二次p指向第512字节 */
		u8* p = (u8 *)sb + j * SECONDFS_BLOCK_SIZE;

		/* 将要写入到设备dev上的SUPER_BLOCK_SECTOR_NUMBER + j扇区中去 */
		pBuf = this->m_BufferManager->GetBlk(sb->s_dev, SECONDFS_SUPER_BLOCK_SECTOR_NUMBER + j);

		/* 将SuperBlock中第0 - 511字节写入缓存区 */
		memcpy(p, pBuf->b_addr, SECONDFS_BLOCK_SIZE);

		/* 将缓冲区中的数据写到磁盘上 */
		ret = this->m_BufferManager->Bwrite(pBuf);

		// After every (sync) write, the Buf will be released.

		if (ret != 0) {
			secondfs_err("FileSystem::Update: Bwrite() failed!");
			goto out;
		}
		goto flush_out;
	}
	
	// Synchronize all Inodes to disk (we won't do this)
	/* 同步修改过的内存Inode到对应外存Inode */
	// @Feng Shun: 我们不做这步了
	//g_InodeTable.UpdateInodeTable();

	/* Unlock update lock */
	/* 清除Update()函数锁 */	

flush_out:
	/* Flush the dirty buffers */
	/* 将延迟写的缓存块写到磁盘上 */
	this->m_BufferManager->Bflush(secsb->s_dev);

out:
	secondfs_c_helper_mutex_unlock(&secsb->s_update_lock);
}

extern "C" Inode *FileSystem_IAlloc(FileSystem *fs, SuperBlock *secsb) { return fs->IAlloc(secsb); }
Inode* FileSystem::IAlloc(SuperBlock *secsb)
{
	SuperBlock* sb = secsb;
	Buf* pBuf;
	Inode* pNode;
	int ino;	/* 分配到的空闲外存Inode编号 */

	/* 如果SuperBlock空闲Inode表被上锁，则睡眠等待至解锁 */
	secondfs_c_helper_mutex_lock(&sb->s_ilock);

	// When fast stack is empty, we have to start out
	// searching free Inode in Inode area on disk.
	/* 
	 * SuperBlock直接管理的空闲Inode索引表已空，
	 * 必须到磁盘上搜索空闲Inode。先对inode列表上锁，
	 * 因为在以下程序中会进行读盘操作可能会导致进程切换，
	 * 其他进程有可能访问该索引表，将会导致不一致性。
	 */
	if((s32)le32_to_cpu(sb->s_ninode) <= 0)
	{
		/* 空闲Inode索引表上锁 */
		// 前面已经上锁

		/* 外存Inode编号从0开始，这不同于Unix V6中外存Inode从1开始编号 */
		ino = -1;

		/* 依次读入磁盘Inode区中的磁盘块，搜索其中空闲外存Inode，记入空闲Inode索引表 */
		for(int i = 0; i < ((s32)le32_to_cpu(sb->s_isize)); i++)
		{
			pBuf = this->m_BufferManager->Bread(sb->s_dev, FileSystem::INODE_ZONE_START_SECTOR + i);

			/* 获取缓冲区首址 */
			s32* p = (s32 *)pBuf->b_addr;

			/* 检查该缓冲区中每个外存Inode的i_mode != 0，表示已经被占用 */
			for(int j = 0; j < FileSystem::INODE_NUMBER_PER_SECTOR; j++)
			{
				ino++;

				s32 mode = *( p + j * sizeof(DiskInode)/sizeof(s32) );

				/* 该外存Inode已被占用，不能记入空闲Inode索引表 */
				if(mode != 0)
				{
					continue;
				}

				secondfs_dbg(INODE, "IAlloc %p: found Inode %d free", secsb, ino);

				// If we met a free inode, we should lookup if its
				// inode number is being used by system now
				/* 
				 * 如果外存inode的i_mode==0，此时并不能确定
				 * 该inode是空闲的，因为有可能是内存inode没有写到
				 * 磁盘上,所以要继续搜索内存inode中是否有相应的项
				 */
				if( secondfs_c_helper_ilookup_without_iget(secsb->s_vsb, ino) == NULL )
				{

					/* 该外存Inode没有对应的内存拷贝，将其记入空闲Inode索引表 */
					sb->s_inode[le32_to_cpu(sb->s_ninode)] = cpu_to_le32(ino);
					secondfs_dbg(INODE, "IAlloc %p: s_inode[%d] = %d", le32_to_cpu(sb->s_ninode), ino);
					sb->s_ninode = cpu_to_le32(le32_to_cpu(sb->s_ninode) + 1);


					/* 如果空闲索引表已经装满，则不继续搜索 */
					if(le32_to_cpu(sb->s_ninode) >= 100)
					{
						break;
					}
				} else {
					secondfs_dbg(INODE, "IAlloc %p: though free Inode %d is being used in memory", secsb, ino);
				}
			}

			/* 至此已读完当前磁盘块，释放相应的缓存 */
			this->m_BufferManager->Brelse(pBuf);

			/* 如果空闲索引表已经装满，则不继续搜索 */
			if(le32_to_cpu(sb->s_ninode) >= 100)
			{
				break;
			}
		}
		/* 解除对空闲外存Inode索引表的锁，唤醒因为等待锁而睡眠的进程 */
		// 稍后解锁
		
		/* 如果在磁盘上没有搜索到任何可用外存Inode，返回NULL */
		if(le32_to_cpu(sb->s_ninode) <= 0)
		{
			secondfs_err("IAlloc %p: No Inode left!!", secsb);
			return NULL;
		}
	}
	secondfs_c_helper_mutex_unlock(&sb->s_ilock);

	// Above part ensured there is something in Inode fast stack.
	// Now we just pick the top Inode of it.
	/* 
	 * 上面部分已经保证，除非系统中没有可用外存Inode，
	 * 否则空闲Inode索引表中必定会记录可用外存Inode的编号。
	 */
	// This "while" is unused here (inner code only executed once)
	while(true)
	{
		/* 从索引表“栈顶”获取空闲外存Inode编号 */
		sb->s_ninode = cpu_to_le32(le32_to_cpu(sb->s_ninode) - 1);
		ino = le32_to_cpu(sb->s_inode[le32_to_cpu(sb->s_ninode)]);

		secondfs_dbg(INODE, "IAlloc %p: got Inode %d from top of fast stack", secsb, ino);
		secondfs_dbg(INODE, "IAlloc %p: now sb->s_ninode == %d", secsb, le32_to_cpu(sb->s_ninode));

		// 在原 Unix V6++ 的逻辑中, 从内存中分配一个 Inode
		// 是用 IGet() 完成的. 在此处, 用 new_inode() 是一个
		// 等价的方法.
		
		// 有一个缺点: 原方法因为向 IGet 传入了 i_number, 
		// 就可以从返回的 Inode 中判断出这样一种异常情况:
		// 明明是从空闲 Inode 栈上拿的 Inode 号, 却发现内存
		// 中已经有一个同 Inode 号的, 已经在工作中的 Inode.

		// new_inode() 检测不到这种情况.

		// new_inode will call secondfs_alloc_inode
		// indirectly, allocating an Inode in memory
		// (instead of filesystem volume! Allocating
		// an Inode in FS needs the logic in the first
		// code block at the beginning of this method).

		// new_inode 会间接调用 secondfs_alloc_inode, 在内
		// 存区域分配一个 Inode. 
		// 注意是内存内而不是文件系统内.
		// 在文件系统中分配 Inode 则需要此函数刚开始时的那段
		// 逻辑.

		pNode = secondfs_c_helper_new_inode(sb);

		/* 未能分配到内存inode */
		if(NULL == pNode)
		{
			secondfs_err("IAlloc %p: failed igetting %d", secsb, ino);
			return NULL;
		}

		pNode->i_number = ino;
		pNode->i_ssb = secsb;

		if( secondfs_c_helper_ilookup_without_iget(secsb->s_vsb, ino) != NULL ) {
			secondfs_err("IAlloc %p: race condition detected!", secsb, ino);
			// TODO: appropriate handling
			return NULL;
		}

		break;

		// 此处删掉了判断 "明明是从空闲 Inode 栈上拿的 Inode 号, 
		// 却发现内存中已经有一个同 Inode 号的, 已经在工作中的 Inode.
		// 这样需要重走 while 循环"的逻辑, 也就不再 Clean() 这个
		// Inode 了.

		// 注意上述逻辑还有一个 BUG: 重走 while 循环如果用完了 Inode
		// 栈, 会访问无效内存!
		
		// 注意同 Unix V6++ 不同, 所有的 Inode 一经创建, i_nlink 都
		// 置为 1.
	}
	return NULL;	/* GCC likes it! */
}

extern "C" void FileSystem_IFree(FileSystem *fs, SuperBlock *secsb, int number) { fs->IFree(secsb, number); }
void FileSystem::IFree(SuperBlock *secsb, int number)
{
	SuperBlock* sb = secsb;
	
	// If the time is not proper to write to fast stack,
	// let the Inode rest in Inode area without record.
	
	/* 
	 * 如果超级块直接管理的空闲Inode表上锁，
	 * 则释放的外存Inode散落在磁盘Inode区中。
	 */
	if (secondfs_c_helper_mutex_is_locked(&sb->s_ilock))
	{
		return;
	}

	/* 
	 * 如果超级块直接管理的空闲外存Inode超过100个，
	 * 同样让释放的外存Inode散落在磁盘Inode区中。
	 */
	if(le32_to_cpu(sb->s_ninode) >= 100)
	{
		return;
	}

	sb->s_inode[le32_to_cpu(sb->s_ninode)] = cpu_to_le32(number);

	/* 设置SuperBlock被修改标志 */
	sb->s_fmod = cpu_to_le32(1);
}

extern "C" void FileSystem_Alloc(FileSystem *fs, SuperBlock *secsb) { fs->Alloc(secsb); }
Buf* FileSystem::Alloc(SuperBlock *secsb)
{
	int blkno;	/* 分配到的空闲磁盘块编号 */
	SuperBlock* sb = secsb;
	Buf* pBuf;

	secondfs_dbg(FILE, "FileSystem::Alloc(%p)...", secsb);

	/* 
	 * 如果空闲磁盘块索引表正在被上锁，表明有其它进程
	 * 正在操作空闲磁盘块索引表，因而对其上锁。这通常
	 * 是由于其余进程调用Free()或Alloc()造成的。
	 */
	secondfs_c_helper_mutex_lock(&sb->s_flock);

	/* Pick a block at the top of fast stack */
	/* 从索引表“栈顶”获取空闲磁盘块编号 */
	sb->s_nfree = cpu_to_le32(le32_to_cpu(sb->s_nfree) - 1);
	blkno = le32_to_cpu(sb->s_free[le32_to_cpu(sb->s_nfree)]);

	secondfs_dbg(FILE, "FileSystem::Alloc(%p): blkno = s_free[%d] == %d", secsb, le32_to_cpu(sb->s_nfree), blkno);

	/* 
	 * 若获取磁盘块编号为零，则表示已分配尽所有的空闲磁盘块。
	 * 或者分配到的空闲磁盘块编号不属于数据盘块区域中(由BadBlock()检查)，
	 * 都意味着分配空闲磁盘块操作失败。
	 */
	if(0 == blkno )
	{
		secondfs_err("FileSystem::Alloc(%p): zero! No space left!", secsb);
		sb->s_nfree = cpu_to_le32(1);
		secondfs_c_helper_mutex_unlock(&sb->s_flock);

		// Diagnose::Write("No Space On %d !\n", dev);
		// u.u_error = User::ENOSPC;
		// secondfs_c_helper_bug();
		return NULL;
	}
	/* if( this->BadBlock(sb, dev, blkno) )
	{
		return NULL;
	} */

	/* 
	 * 栈已空，新分配到空闲磁盘块中记录了下一组空闲磁盘块的编号,
	 * 将下一组空闲磁盘块的编号读入SuperBlock的空闲磁盘块索引表s_free[100]中。
	 */
	if((s32)le32_to_cpu(sb->s_nfree) <= 0)
	{
		/* 
		 * 此处加锁，因为以下要进行读盘操作，有可能发生进程切换，
		 * 新上台的进程可能对SuperBlock的空闲盘块索引表访问，会导致不一致性。
		 */
		// 上面已经加过锁了

		secondfs_dbg(FILE, "FileSystem::Alloc(%p): s_nfree == %d; read next group of free data blocks", secsb, le32_to_cpu(sb->s_nfree));

		/* 读入该空闲磁盘块 */
		pBuf = this->m_BufferManager->Bread(sb->s_dev, blkno);
		// We just hard-code IS_ERR() macro here
		if ((uintptr_t)(pBuf) >= (uintptr_t)-4095) {
			secondfs_err("FileSystem::Alloc(%p): reading %p/%d failed! errno: %d", secsb, sb->s_dev, blkno, (int)(intptr_t)pBuf);

			secondfs_c_helper_mutex_unlock(&sb->s_flock);
			return NULL;
		}

		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		s32* p = (s32 *)pBuf->b_addr;

		/* 首先读出空闲盘块数s_nfree */
		sb->s_nfree = (*p++);

		/* 读取缓存中后续位置的数据，写入到SuperBlock空闲盘块索引表s_free[100]中 */
		memcpy(sb->s_free, p, sizeof(sb->s_free));

		secondfs_dbg(FILE, "FileSystem::Alloc(%p): s_nfree == %d, s_free[top] == %d", secsb, le32_to_cpu(sb->s_nfree), le32_to_cpu(sb->s_free[99]));

		/* 缓存使用完毕，释放以便被其它进程使用 */
		this->m_BufferManager->Brelse(pBuf);

		/* 解除对空闲磁盘块索引表的锁，唤醒因为等待锁而睡眠的进程 */
	}

	secondfs_c_helper_mutex_unlock(&sb->s_flock);

	/* 普通情况下成功分配到一空闲磁盘块 */
	pBuf = this->m_BufferManager->GetBlk(sb->s_dev, blkno);	/* 为该磁盘块申请缓存 */
	this->m_BufferManager->ClrBuf(pBuf);	/* 清空缓存中的数据 */
	sb->s_fmod = cpu_to_le32(1);	/* 设置SuperBlock被修改标志 */

	return pBuf;
}

extern "C" int FileSystem_Free(FileSystem *fs, SuperBlock *secsb, int blkno) { return fs->Free(secsb, blkno); }
int FileSystem::Free(SuperBlock *secsb, int blkno)
{
	// Release data block
	SuperBlock* sb = secsb;
	Buf* pBuf;
	int ret = 0;

	/* 
	 * 尽早设置SuperBlock被修改标志，以防止在释放
	 * 磁盘块Free()执行过程中，对SuperBlock内存副本
	 * 的修改仅进行了一半，就更新到磁盘SuperBlock去
	 */
	sb->s_fmod = cpu_to_le32(1);

	/* 如果空闲磁盘块索引表被上锁，则睡眠等待解锁 */
	secondfs_c_helper_mutex_lock(&sb->s_flock);

	/* 检查释放磁盘块的合法性 */
	/*if(this->BadBlock(sb, dev, blkno))
	{
		return;
	}*/

	/* 
	 * Releasing the first free block into fast stack
	 * Note: this should be an abnormal state. When the
	 * last data block is allocated, s_nfree == 1.
	 */
	/* 
	 * 如果先前系统中已经没有空闲盘块，
	 * 现在释放的是系统中第1块空闲盘块
	 */
	if((s32) le32_to_cpu(sb->s_nfree) <= 0)
	{
		secondfs_err("FileSystem::Free(%p,%d): abnormal situation.", secsb, blkno);
		sb->s_nfree = (s32) cpu_to_le32(1);
		sb->s_free[0] = 0;	/* 使用0标记空闲盘块链结束标志 */
	}

	/* The fast stack in SuperBlock is full */
	/* SuperBlock中直接管理空闲磁盘块号的栈已满 */
	if((s32) le32_to_cpu(sb->s_nfree) >= 100)
	{
		secondfs_dbg(DATABLK, "FileSystem::Free(%p,%d): fast stack full.", secsb, blkno);
		/* 
		 * 使用当前Free()函数正要释放的磁盘块，存放前一组100个空闲
		 * 磁盘块的索引表
		 */
		pBuf = this->m_BufferManager->GetBlk(secsb->s_dev, blkno);	/* 为当前正要释放的磁盘块分配缓存 */

		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		u32* p = (u32 *)pBuf->b_addr;
		
		/* This block is used to store n_free(most of the time 100/99) and 100 free
		 * block numbers stored in fast stack. Then this block is sent to disk,
		 * and fast stack cleared.
		 */
		/* 首先写入空闲盘块数，除了第一组为99块，后续每组都是100块 */
		*p++ = sb->s_nfree;
		/* 将SuperBlock的空闲盘块索引表s_free[100]写入缓存中后续位置 */
		memcpy(p, sb->s_free, sizeof(sb->s_free));

		sb->s_nfree = cpu_to_le32(0);
		/* 将存放空闲盘块索引表的“当前释放盘块”写入磁盘，即实现了空闲盘块记录空闲盘块号的目标 */
		ret = this->m_BufferManager->Bwrite(pBuf);
		
		if (ret < 0) {
			secondfs_err("FileSystem::Free(%p,%d) write failed!", secsb, blkno);
			goto out;
		}
	}
out:
	secondfs_c_helper_mutex_unlock(&sb->s_flock);
	sb->s_free[le32_to_cpu(sb->s_nfree)] = cpu_to_le32(blkno);	/* SuperBlock中记录下当前释放盘块号 */
	sb->s_nfree = cpu_to_le32(le32_to_cpu(sb->s_nfree) + 1);
	sb->s_fmod = cpu_to_le32(1);
	secondfs_dbg(DATABLK, "FileSystem::Free(%p,%d): sb->s_free[%d] = %d", secsb, blkno, le32_to_cpu(sb->s_nfree) - 1, sb->s_free[le32_to_cpu(sb->s_nfree) - 1]);

	return ret;
}

#if false
Mount* FileSystem::GetMount(Inode *pInode)
{
	/* 遍历系统的装配块表 */
	for(int i = 0; i <= FileSystem::NMOUNT; i++)
	{
		Mount* pMount = &(this->m_Mount[i]);

		/* 找到内存Inode对应的Mount装配块 */
		if(pMount->m_inodep == pInode)
		{
			return pMount;
		}
	}
	return NULL;	/* 查找失败 */
}

bool FileSystem::BadBlock(SuperBlock *spb, short dev, int blkno)
{
	return 0;
}
#endif


// @Feng Shun: 以下为 C wrapping 部分
extern "C" {
	SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(SuperBlock);


	const s32
		SECONDFS_NMOUNT = FileSystem::NMOUNT,			/* 系统中用于挂载子文件系统的装配块数量 */
		SECONDFS_SUPER_BLOCK_SECTOR_NUMBER = FileSystem::SUPER_BLOCK_SECTOR_NUMBER,	/* 定义SuperBlock位于磁盘上的扇区号，占据200，201两个扇区。 */
		SECONDFS_SUPER_BLOCK_DISK_SIZE = 1024,
		SECONDFS_ROOTINO = FileSystem::ROOTINO,			/* 文件系统根目录外存Inode编号 */
		SECONDFS_INODE_NUMBER_PER_SECTOR = FileSystem::INODE_NUMBER_PER_SECTOR,	/* 外存INode对象长度为64字节，每个磁盘块可以存放512/64 = 8个外存Inode */
		SECONDFS_INODE_ZONE_START_SECTOR = FileSystem::INODE_ZONE_START_SECTOR,	/* 外存Inode区位于磁盘上的起始扇区号 */
		SECONDFS_INODE_ZONE_SIZE = FileSystem::INODE_ZONE_SIZE,		/* 磁盘上外存Inode区占据的扇区数 */
		SECONDFS_DATA_ZONE_START_SECTOR = FileSystem::DATA_ZONE_START_SECTOR,	/* 数据区的起始扇区号 */
		SECONDFS_DATA_ZONE_END_SECTOR = FileSystem::DATA_ZONE_END_SECTOR,	/* 数据区的结束扇区号 */
		SECONDFS_DATA_ZONE_SIZE = FileSystem::DATA_ZONE_SIZE		/* 数据区占据的扇区数量 */
	;

	SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(FileSystem);
}