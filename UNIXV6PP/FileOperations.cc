/* UNIXV6PP 文件系统(主要是文件操作)代码裁剪. */
#include "../secondfs.h"
#include <errno.h>
#include <memory.h>

#include "../common.h"
#include "Inode.hh"
#include "FileSystem.hh"
#include "FileOperations.hh"

// @Feng Shun: 以下为 C++ 部分

/*======================class FileManager======================*/
FileManager::FileManager()
{
	//nothing to do here
}

FileManager::~FileManager()
{
	//nothing to do here
}

#if false
void FileManager::Initialize()
{
	this->m_FileSystem = &Kernel::Instance().GetFileSystem();

	this->m_InodeTable = &g_InodeTable;
	this->m_OpenFileTable = &g_OpenFileTable;

	this->m_InodeTable->Initialize();
}
#endif

#if false
/*
 * 功能：打开文件
 * 效果：建立打开文件结构，内存i节点开锁 、i_count 为正数（i_count ++）
 * */
void FileManager::Open()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();

	pInode = this->NameI(NextChar, FileManager::OPEN);	/* 0 = Open, not create */
	/* 没有找到相应的Inode */
	if ( NULL == pInode )
	{
		return;
	}
	this->Open1(pInode, u.u_arg[1], 0);
}

/*
 * 功能：创建一个新的文件
 * 效果：建立打开文件结构，内存i节点开锁 、i_count 为正数（应该是 1）
 * */
void FileManager::Creat()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();
	unsigned int newACCMode = u.u_arg[1] & (Inode::IRWXU|Inode::IRWXG|Inode::IRWXO);

	/* 搜索目录的模式为1，表示创建；若父目录不可写，出错返回 */
	pInode = this->NameI(NextChar, FileManager::CREATE);
	/* 没有找到相应的Inode，或NameI出错 */
	if ( NULL == pInode )
	{
		if(u.u_error)
			return;
		/* 创建Inode */
		pInode = this->MakNode( newACCMode & (~Inode::ISVTX) );
		/* 创建失败 */
		if ( NULL == pInode )
		{
			return;
		}

		/* 
		 * 如果所希望的名字不存在，使用参数trf = 2来调用open1()。
		 * 不需要进行权限检查，因为刚刚建立的文件的权限和传入参数mode
		 * 所表示的权限内容是一样的。
		 */
		this->Open1(pInode, File::FWRITE, 2);
	}
	else
	{
		/* 如果NameI()搜索到已经存在要创建的文件，则清空该文件（用算法ITrunc()）。UID没有改变
		 * 原来UNIX的设计是这样：文件看上去就像新建的文件一样。然而，新文件所有者和许可权方式没变。
		 * 也就是说creat指定的RWX比特无效。
		 * 邓蓉认为这是不合理的，应该改变。
		 * 现在的实现：creat指定的RWX比特有效 */
		this->Open1(pInode, File::FWRITE, 1);
		pInode->i_mode |= newACCMode;
	}
}

/* 
* trf == 0由open调用
* trf == 1由creat调用，creat文件的时候搜索到同文件名的文件
* trf == 2由creat调用，creat文件的时候未搜索到同文件名的文件，这是文件创建时更一般的情况
* mode参数：打开文件模式，表示文件操作是 读、写还是读写
*/
void FileManager::Open1(Inode* pInode, int mode, int trf)
{
	User& u = Kernel::Instance().GetUser();

	/* 
	 * 对所希望的文件已存在的情况下，即trf == 0或trf == 1进行权限检查
	 * 如果所希望的名字不存在，即trf == 2，不需要进行权限检查，因为刚建立
	 * 的文件的权限和传入的参数mode的所表示的权限内容是一样的。
	 */
	if (trf != 2)
	{
		if ( mode & File::FREAD )
		{
			/* 检查读权限 */
			this->Access(pInode, Inode::IREAD);
		}
		if ( mode & File::FWRITE )
		{
			/* 检查写权限 */
			this->Access(pInode, Inode::IWRITE);
			/* 系统调用去写目录文件是不允许的 */
			if ( (pInode->i_mode & Inode::IFMT) == Inode::IFDIR )
			{
				u.u_error = User::EISDIR;
			}
		}
	}

	if ( u.u_error )
	{
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* 在creat文件的时候搜索到同文件名的文件，释放该文件所占据的所有盘块 */
	if ( 1 == trf )
	{
		pInode->ITrunc();
	}

	/* 解锁inode! 
	 * 线性目录搜索涉及大量的磁盘读写操作，期间进程会入睡。
	 * 因此，进程必须上锁操作涉及的i节点。这就是NameI中执行的IGet上锁操作。
	 * 行至此，后续不再有可能会引起进程切换的操作，可以解锁i节点。
	 */
	pInode->Prele();

	/* 分配打开文件控制块File结构 */
	File* pFile = this->m_OpenFileTable->FAlloc();
	if ( NULL == pFile )
	{
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 设置打开文件方式，建立File结构和内存Inode的勾连关系 */
	pFile->f_flag = mode & (File::FREAD | File::FWRITE);
	pFile->f_inode = pInode;

	/* 特殊设备打开函数 */
	pInode->OpenI(mode & File::FWRITE);

	/* 为打开或者创建文件的各种资源都已成功分配，函数返回 */
	if ( u.u_error == 0 )
	{
		return;
	}
	else	/* 如果出错则释放资源 */
	{
		/* 释放打开文件描述符 */
		int fd = u.u_ar0[User::EAX];
		if(fd != -1)
		{
			u.u_ofiles.SetF(fd, NULL);
			/* 递减File结构和Inode的引用计数 ,File结构没有锁 f_count为0就是释放File结构了*/
			pFile->f_count--;
		}
		this->m_InodeTable->IPut(pInode);
	}
}

void FileManager::Close()
{
	User& u = Kernel::Instance().GetUser();
	int fd = u.u_arg[0];

	/* 获取打开文件控制块File结构 */
	File* pFile = u.u_ofiles.GetF(fd);
	if ( NULL == pFile )
	{
		return;
	}

	/* 释放打开文件描述符fd，递减File结构引用计数 */
	u.u_ofiles.SetF(fd, NULL);
	this->m_OpenFileTable->CloseF(pFile);
}

void FileManager::Seek()
{
	File* pFile;
	User& u = Kernel::Instance().GetUser();
	int fd = u.u_arg[0];

	pFile = u.u_ofiles.GetF(fd);
	if ( NULL == pFile )
	{
		return;  /* 若FILE不存在，GetF有设出错码 */
	}

	/* 管道文件不允许seek */
	if ( pFile->f_flag & File::FPIPE )
	{
		u.u_error = User::ESPIPE;
		return;
	}

	int offset = u.u_arg[1];

	/* 如果u.u_arg[2]在3 ~ 5之间，那么长度单位由字节变为512字节 */
	if ( u.u_arg[2] > 2 )
	{
		offset = offset << 9;
		u.u_arg[2] -= 3;
	}

	switch ( u.u_arg[2] )
	{
		/* 读写位置设置为offset */
		case 0:
			pFile->f_offset = offset;
			break;
		/* 读写位置加offset(可正可负) */
		case 1:
			pFile->f_offset += offset;
			break;
		/* 读写位置调整为文件长度加offset */
		case 2:
			pFile->f_offset = pFile->f_inode->i_size + offset;
			break;
	}
}

void FileManager::Dup()
{
	File* pFile;
	User& u = Kernel::Instance().GetUser();
	int fd = u.u_arg[0];

	pFile = u.u_ofiles.GetF(fd);
	if ( NULL == pFile )
	{
		return;
	}

	int newFd = u.u_ofiles.AllocFreeSlot();
	if ( newFd < 0 )
	{
		return;
	}
	/* 至此分配新描述符newFd成功 */
	u.u_ofiles.SetF(newFd, pFile);
	pFile->f_count++;
}

void FileManager::FStat()
{
	File* pFile;
	User& u = Kernel::Instance().GetUser();
	int fd = u.u_arg[0];

	pFile = u.u_ofiles.GetF(fd);
	if ( NULL == pFile )
	{
		return;
	}

	/* u.u_arg[1] = pStatBuf */
	this->Stat1(pFile->f_inode, u.u_arg[1]);
}

void FileManager::Stat()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN);
	if ( NULL == pInode )
	{
		return;
	}
	this->Stat1(pInode, u.u_arg[1]);
	this->m_InodeTable->IPut(pInode);
}

void FileManager::Stat1(Inode* pInode, unsigned long statBuf)
{
	Buf* pBuf;
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();

	pInode->IUpdate(Time::time);
	pBuf = bufMgr.Bread(pInode->i_dev, FileSystem::INODE_ZONE_START_SECTOR + pInode->i_number / FileSystem::INODE_NUMBER_PER_SECTOR );

	/* 将p指向缓存区中编号为inumber外存Inode的偏移位置 */
	unsigned char* p = pBuf->b_addr + (pInode->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	Utility::DWordCopy( (int *)p, (int *)statBuf, sizeof(DiskInode)/sizeof(int) );

	bufMgr.Brelse(pBuf);
}
#endif

extern "C" long FileManager_Read(FileManager *fm, u8 *buf, size_t len, u32 *ppos, Inode *inode)
{ return fm->Read(buf, len, ppos, inode); }
long FileManager::Read(u8 *buf, size_t len, u32 *ppos, Inode *inode)
{
	IOParameter io_param;
	long size;
	/* 直接调用Rdwr()函数即可 */
	size = this->Rdwr(buf, len, ppos, &io_param, inode, SECONDFS_FREAD);
	if (io_param.err != 0) {
		return io_param.err;
	}
	return size;
}

extern "C" long FileManager_Write(FileManager *fm, const u8 *buf, size_t len, u32 *ppos, Inode *inode)
{ return fm->Write(buf, len, ppos, inode); }
long FileManager::Write(const u8 *buf, size_t len, u32 *ppos, Inode *inode)
{
	IOParameter io_param;
	long size;
	/* 直接调用Rdwr()函数即可 */
	size = this->Rdwr((u8 *)buf, len, ppos, &io_param, inode, SECONDFS_FWRITE);
	if (io_param.err != 0) {
		return io_param.err;
	}
	return size;
}

extern "C" u32 FileManager_Rdwr(FileManager *fm, u8 *buf, size_t len, u32 *ppos, IOParameter *io_paramp, Inode *inode, u32 mode)
{ return fm->Rdwr(buf, len, ppos, io_paramp, inode, mode); }

u32 FileManager::Rdwr(u8 *buf, size_t len, u32 *ppos, IOParameter *io_paramp, Inode *inode, u32 mode)
{
	io_paramp->m_Base = buf;	/* 目标缓冲区首址 */
	io_paramp->m_Count = len;	/* 要求读/写的字节数 */
	io_paramp->isUserP = 1;		// copy to/from user space

	/* 普通文件读写 ，或读写特殊文件。对文件实施互斥访问，互斥的粒度：每次系统调用。
	为此Inode类需要增加两个方法：NFlock()、NFrele()。
	这不是V6的设计。read、write系统调用对内存i节点上锁是为了给实施IO的进程提供一致的文件视图。*/
	{
		inode->NFlock();
		/* 设置文件起始读位置 */
		io_paramp->m_Offset = *ppos;
		if ( SECONDFS_FREAD == mode )
		{
			inode->ReadI(io_paramp);
		}
		else
		{
			inode->WriteI(io_paramp);
		}

		/* 根据读写字数，移动文件读写偏移指针 */
		//*ppos += (len - io_paramp->m_Count);
		*ppos = io_paramp->m_Offset;
		inode->NFrele();
	}

	/* 返回实际读写的字节数 */
	secondfs_dbg(FILE, "FileManager::Rdwr(): return %u", len - io_paramp->m_Count);
	return len - io_paramp->m_Count;
}

#if false
void FileManager::Pipe()
{
	Inode* pInode;
	File* pFileRead;
	File* pFileWrite;
	int fd[2];
	User& u = Kernel::Instance().GetUser();

	/* 分配一个Inode用于创建管道文件 */
	pInode = this->m_FileSystem->IAlloc(DeviceManager::ROOTDEV);
	if ( NULL == pInode )
	{
		return;
	}

	/* 分配读管道的File结构 */
	pFileRead = this->m_OpenFileTable->FAlloc();
	if ( NULL == pFileRead )
	{
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 读管道的打开文件描述符 */
	fd[0] = u.u_ar0[User::EAX];

	/* 分配写管道的File结构 */
	pFileWrite = this->m_OpenFileTable->FAlloc();
	if ( NULL == pFileWrite )    /*若分配失败，擦除管道读端相关的所有打开文件结构*/
	{
		pFileRead->f_count = 0;
		u.u_ofiles.SetF(fd[0], NULL);
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* 写管道的打开文件描述符 */
	fd[1] = u.u_ar0[User::EAX];

	/* Pipe(int* fd)的参数在u.u_arg[0]中，将分配成功的2个fd返回给用户态程序 */
	int* pFdarr = (int *)u.u_arg[0];
	pFdarr[0] = fd[0];
	pFdarr[1] = fd[1];

	/* 设置读、写管道File结构的属性 ，以后read、write系统调用需要这个标识*/
	pFileRead->f_flag = File::FREAD | File::FPIPE;
	pFileRead->f_inode = pInode;
	pFileWrite->f_flag = File::FWRITE | File::FPIPE;
	pFileWrite->f_inode = pInode;

	pInode->i_count = 2;
	pInode->i_flag = Inode::IACC | Inode::IUPD;
	pInode->i_mode = Inode::IALLOC;
}

void FileManager::ReadP(File *pFile)
{
	Inode* pInode = pFile->f_inode;
	User& u = Kernel::Instance().GetUser();

loop:
	/* 对管道文件上锁保证互斥 ，在现在的V6版本普通文件的读写也采取这种非常保守的上锁方式*/
	pInode->Plock();

	/* 管道中没有数据可读取 。管道文件从尾部开始写，故i_size是写指针。*/
	if ( pFile->f_offset == pInode->i_size )
	{
		if ( pFile->f_offset != 0 )
		{
			/* 读管道文件偏移量和管道文件大小重置为0 */
			pFile->f_offset = 0;
			pInode->i_size = 0;

			/* 如果置上IWRITE标志，则表示有进程正在等待写此管道，所以必须唤醒相应的进程。*/
			if ( pInode->i_mode & Inode::IWRITE )
			{
				pInode->i_mode &= (~Inode::IWRITE);
				Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)(pInode + 1));
			}
		}

		pInode->Prele(); /* 不解锁的话，写管道进程无法对管道实施操作。系统死锁 */

		/* 如果管道的读者、写者中已经有一方关闭，则返回 */
		if ( pInode->i_count < 2 )
		{
			return;
		}

		/* IREAD标志表示有进程等待读Pipe */
		pInode->i_mode |= Inode::IREAD;
		u.u_procp->Sleep((unsigned long)(pInode + 2), ProcessManager::PPIPE);
		goto loop;
	}

	/* 管道中有有可读取的数据 */
	u.u_IOParam.m_Offset = pFile->f_offset;
	pInode->ReadI();
	pFile->f_offset = u.u_IOParam.m_Offset;
	pInode->Prele();
}

void FileManager::WriteP(File* pFile)
{
	Inode* pInode = pFile->f_inode;
	User& u = Kernel::Instance().GetUser();

	int count = u.u_IOParam.m_Count;

loop:
	pInode->Plock();

	/* 已完成所有数据写入管道，对管道unlock并返回 */
	if ( 0 == count )
	{
		pInode->Prele();
		u.u_IOParam.m_Count = 0;
		return;
	}

	/* 管道读者进程已关闭读端、用信号SIGPIPE通知应用程序 */
	if ( pInode->i_count < 2 )
	{
		pInode->Prele();
		u.u_error = User::EPIPE;
		u.u_procp->PSignal(User::SIGPIPE);
		return;
	}

	/* 如果已经到达管道的底，则置上同步标志，睡眠等待 */
	if ( Inode::PIPSIZ == pInode->i_size )
	{
		pInode->i_mode |= Inode::IWRITE;
		pInode->Prele();
		u.u_procp->Sleep((unsigned long)(pInode + 1), ProcessManager::PPIPE);
		goto loop;
	}

	/* 将待写入数据尽可能多地写入管道 */
	u.u_IOParam.m_Offset = pInode->i_size;
	u.u_IOParam.m_Count = Utility::Min(count, Inode::PIPSIZ - u.u_IOParam.m_Offset);
	count -= u.u_IOParam.m_Count;
	pInode->WriteI();
	pInode->Prele();

	/* 唤醒读管道进程 */
	if ( pInode->i_mode & Inode::IREAD )
	{
		pInode->i_mode &= (~Inode::IREAD);
		Kernel::Instance().GetProcessManager().WakeUpAll((unsigned long)(pInode + 2));
	}
	goto loop;

}

/* 返回NULL表示目录搜索失败，否则是根指针，指向文件的内存打开i节点 ，上锁的内存i节点  */
Inode* FileManager::NameI( char (*func)(), enum DirectorySearchMode mode )
{
	Inode* pInode;
	Buf* pBuf;
	char curchar;
	char* pChar;
	int freeEntryOffset;	/* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */
	User& u = Kernel::Instance().GetUser();
	BufferManager& bufMgr = Kernel::Instance().GetBufferManager();

	/* 
	 * 如果该路径是'/'开头的，从根目录开始搜索，
	 * 否则从进程当前工作目录开始搜索。
	 */
	pInode = u.u_cdir;
	if ( '/' == (curchar = (*func)()) )
	{
		pInode = this->rootDirInode;
	}

	/* 检查该Inode是否正在被使用，以及保证在整个目录搜索过程中该Inode不被释放 */
	this->m_InodeTable->IGet(pInode->i_dev, pInode->i_number);

	/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
	while ( '/' == curchar )
	{
		curchar = (*func)();
	}
	/* 如果试图更改和删除当前目录文件则出错 */
	if ( '\0' == curchar && mode != FileManager::OPEN )
	{
		u.u_error = User::ENOENT;
		goto out;
	}

	/* 外层循环每次处理pathname中一段路径分量 */
	while (true)
	{
		/* 如果出错则释放当前搜索到的目录文件Inode，并退出 */
		if ( u.u_error != User::NOERROR )
		{
			break;	/* goto out; */
		}

		/* 整个路径搜索完毕，返回相应Inode指针。目录搜索成功返回。 */
		if ( '\0' == curchar )
		{
			return pInode;
		}

		/* 如果要进行搜索的不是目录文件，释放相关Inode资源则退出 */
		if ( (pInode->i_mode & Inode::IFMT) != Inode::IFDIR )
		{
			u.u_error = User::ENOTDIR;
			break;	/* goto out; */
		}

		/* 进行目录搜索权限检查,IEXEC在目录文件中表示搜索权限 */
		if ( this->Access(pInode, Inode::IEXEC) )
		{
			u.u_error = User::EACCES;
			break;	/* 不具备目录搜索权限，goto out; */
		}

		/* 
		 * 将Pathname中当前准备进行匹配的路径分量拷贝到u.u_dbuf[]中，
		 * 便于和目录项进行比较。
		 */
		pChar = &(u.u_dbuf[0]);
		while ( '/' != curchar && '\0' != curchar && u.u_error == User::NOERROR )
		{
			if ( pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]) )
			{
				*pChar = curchar;
				pChar++;
			}
			curchar = (*func)();
		}
		/* 将u_dbuf剩余的部分填充为'\0' */
		while ( pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]) )
		{
			*pChar = '\0';
			pChar++;
		}

		/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
		while ( '/' == curchar )
		{
			curchar = (*func)();
		}

		if ( u.u_error != User::NOERROR )
		{
			break; /* goto out; */
		}

		/* 内层循环部分对于u.u_dbuf[]中的路径名分量，逐个搜寻匹配的目录项 */
		u.u_IOParam.m_Offset = 0;
		/* 设置为目录项个数 ，含空白的目录项*/
		u.u_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0;
		pBuf = NULL;

		while (true)
		{
			/* 对目录项已经搜索完毕 */
			if ( 0 == u.u_IOParam.m_Count )
			{
				if ( NULL != pBuf )
				{
					bufMgr.Brelse(pBuf);
				}
				/* 如果是创建新文件 */
				if ( FileManager::CREATE == mode && curchar == '\0' )
				{
					/* 判断该目录是否可写 */
					if ( this->Access(pInode, Inode::IWRITE) )
					{
						u.u_error = User::EACCES;
						goto out;	/* Failed */
					}

					/* 将父目录Inode指针保存起来，以后写目录项WriteDir()函数会用到 */
					u.u_pdir = pInode;

					if ( freeEntryOffset )	/* 此变量存放了空闲目录项位于目录文件中的偏移量 */
					{
						/* 将空闲目录项偏移量存入u区中，写目录项WriteDir()会用到 */
						u.u_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);
					}
					else  /*问题：为何if分支没有置IUPD标志？  这是因为文件的长度没有变呀*/
					{
						pInode->i_flag |= Inode::IUPD;
					}
					/* 找到可以写入的空闲目录项位置，NameI()函数返回 */
					return NULL;
				}
				
				/* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源，并推出 */
				u.u_error = User::ENOENT;
				goto out;
			}

			/* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
			if ( 0 == u.u_IOParam.m_Offset % Inode::BLOCK_SIZE )
			{
				if ( NULL != pBuf )
				{
					bufMgr.Brelse(pBuf);
				}
				/* 计算要读的物理盘块号 */
				int phyBlkno = pInode->Bmap(u.u_IOParam.m_Offset / Inode::BLOCK_SIZE );
				pBuf = bufMgr.Bread(pInode->i_dev, phyBlkno );
			}

			/* 没有读完当前目录项盘块，则读取下一目录项至u.u_dent */
			int* src = (int *)(pBuf->b_addr + (u.u_IOParam.m_Offset % Inode::BLOCK_SIZE));
			Utility::DWordCopy( src, (int *)&u.u_dent, sizeof(DirectoryEntry)/sizeof(int) );

			u.u_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4);
			u.u_IOParam.m_Count--;

			/* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
			if ( 0 == u.u_dent.m_ino )
			{
				if ( 0 == freeEntryOffset )
				{
					freeEntryOffset = u.u_IOParam.m_Offset;
				}
				/* 跳过空闲目录项，继续比较下一目录项 */
				continue;
			}

			int i;
			for ( i = 0; i < DirectoryEntry::DIRSIZ; i++ )
			{
				if ( u.u_dbuf[i] != u.u_dent.m_name[i] )
				{
					break;	/* 匹配至某一字符不符，跳出for循环 */
				}
			}

			if( i < DirectoryEntry::DIRSIZ )
			{
				/* 不是要搜索的目录项，继续匹配下一目录项 */
				continue;
			}
			else
			{
				/* 目录项匹配成功，回到外层While(true)循环 */
				break;
			}
		}

		/* 
		 * 从内层目录项匹配循环跳至此处，说明pathname中
		 * 当前路径分量匹配成功了，还需匹配pathname中下一路径
		 * 分量，直至遇到'\0'结束。
		 */
		if ( NULL != pBuf )
		{
			bufMgr.Brelse(pBuf);
		}

		/* 如果是删除操作，则返回父目录Inode，而要删除文件的Inode号在u.u_dent.m_ino中 */
		if ( FileManager::DELETE == mode && '\0' == curchar )
		{
			/* 如果对父目录没有写的权限 */
			if ( this->Access(pInode, Inode::IWRITE) )
			{
				u.u_error = User::EACCES;
				break;	/* goto out; */
			}
			return pInode;
		}

		/* 
		 * 匹配目录项成功，则释放当前目录Inode，根据匹配成功的
		 * 目录项m_ino字段获取相应下一级目录或文件的Inode。
		 */
		short dev = pInode->i_dev;
		this->m_InodeTable->IPut(pInode);
		pInode = this->m_InodeTable->IGet(dev, u.u_dent.m_ino);
		/* 回到外层While(true)循环，继续匹配Pathname中下一路径分量 */

		if ( NULL == pInode )	/* 获取失败 */
		{
			return NULL;
		}
	}
out:
	this->m_InodeTable->IPut(pInode);
	return NULL;
}
#endif

/* DELocate: Locate DirectoryEntry in dir.
 *    When mode == OPEN,DELETE : (they have no differences)
 * 	search 'name' in Inode *dir; record the DE's offset in out_iop; write its Inode
 * 	number to *inop
 *    When mode == CREATE :
 *	search free(available) DE slot in Inode *dir's DETable; record the DE's offset
 *	in out_iop
 *    When mode == CHECKEMPTY :
 *	tell if dir is an empty directory. If it is, assian a non-zero value to
 *	out_iop->m_Offset; otherwise 0
 *    When mode == LIST :
 * 	read out_iop->m_Offset as offset in DETable, scan the directory.
 * 	(In this case, the 'out' in 'out_iop' is some kind of misname)
 * 
 *	inop = {dir_emit, ctx, type, ppos}
 *	(bool (*)(void *ctx, const char * name, int namelen, u64 ino, unsigned int type))inop[0] -> dir_emit
 *	(void * (Actually struct dir_context *))inop[1] -> ctx
 *	(unsigned int *)inop[2] -> type, typically 0 (DT_UNKNOWN)
 *	(void * (Actually loff_t *))inop[3] -> ppos, ppos is moved everytime out_iop->mOffset moves;
 *		if s_has_dots != 0xffffffff, ppos is 2 DirectoryEntries beyond iop->m_Offset(as if the DETable
 *		has '.' & '..' from the aspect of ppos).
 *	
 *	execute dir_emit for every DE.

 *    When mode == OPEN_NOT_IGNORE_DOTS :
 *	The same as OPEN, DELETE except for inclusion of "." ".." (and most probably name == '.' or  '..')
 */

/* 在 dir 内定位目录项.
 *    当 mode == OPEN,DELETE : (它们没有区别, 不再检查是否有打开/删除文件的资格, 不再有返回父/子目录的区别)
 *	在 dir 的目录项内查找 name, 并将目录项的偏移量写入 out_iop, 将 Inode 号写入 *inop
 *    当 mode == CREATE :
 *	在 dir 的目录项中查找可以放置目录项的空位, 并将空位的偏移量写入 out_iop
 *    当 mode == CHECKEMPTY :
 *	判断 dir 是否是空目录, 若空, 将 out_iop->m_Offset 置为非 0, 否则 0
 *    当 mode == LIST :
 * 	从 out_iop->m_Offset 设定的偏移值出发, 扫描目录. (从这点来说 iop 也有 in 的成分了, 
 * 	命名不太好)
 * 
 *	将 inop 当作一个 void * 数组的头指针.
 *	(bool (*)(void *ctx, const char * name, int namelen, u64 ino, unsigned int type))inop[0] -> dir_emit 函数指针
 *	(void * (实际上是 struct dir_context *))inop[1] -> ctx, 要传给上述函数的
 *	(unsigned int *)inop[2] -> type, 要传给上述函数的, 一般是 0 (DT_UNKNOWN)
 *	(void * (实际上是 loff_t *))inop[3] -> ppos, 每次 iop 的读头移动后, 这个也要跟着移动;
 *		并且如果 s_has_dots 没有置位, ppos 超前 iop->m_Offset 2 个 DirectoryEntry 的位置.
 *	
 *	对每个目录项执行 dir_emit 函数.
 *    当 mode == OPEN_NOT_IGNORE_DOTS :
 *	同 OPEN, DELETE, 但不要跳过 "." ".." (意即很可能就搜索这两个目录项之一)
 */
extern "C" int FileManager_DELocate(FileManager *fm, Inode *dir, const char *name, u32 namelen, u32 mode, IOParameter *out_iop, u32 *inop)
{ return fm->DELocate(dir, name, namelen, mode, out_iop, inop); }
int FileManager::DELocate(Inode *dir, const char *name, u32 namelen, u32 mode, IOParameter *out_iop, u32 *inop)
{
	Inode* pInode;
	Buf* pBuf = NULL;
	int freeEntryOffset;	/* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */
	DirectoryEntry dent;
	int ret = 0;

	bool (*dir_emit)(void *ctx, const char * name, int namelen, u64 ino, unsigned int type);
	void *ctx;
	unsigned int *type;
	void *ppos;

	int firstTimeRead = 1;

	s32 has_dots = dir->i_ssb->s_has_dots;

	dir_emit = (decltype(dir_emit))(((void **)inop)[0]);
	ctx = (decltype(ctx))(((void **)inop)[1]);
	type = (decltype(type))(((void **)inop)[2]);
	ppos = (decltype(ppos))(((void **)inop)[3]);

	BufferManager& bufMgr = *secondfs_buffermanagerp;

	pInode = dir;

	secondfs_dbg(FILE, "FileManager::DELocate()...name=%.32s, type=%u", name, mode);



	/* 检查该Inode是否正在被使用，以及保证在整个目录搜索过程中该Inode不被释放 */
	// 不需要了, 我们不使用 UnixV6++ 对于 Inode 的锁机制
	//this->m_InodeTable->IGet(pInode->i_dev, pInode->i_number);

	{
		/* 循环部分对于 name 中的路径名分量，逐个搜寻匹配的目录项 */

		// 如果是 SECONDFS_LIST, 应该沿着 out_iop->m_Offset 继续搜索
		// 不能置为 0.
		if (SECONDFS_LIST == mode) {
			// 向右对齐
			out_iop->m_Offset = (out_iop->m_Offset + sizeof(DirectoryEntry) - 1) / sizeof(DirectoryEntry) * sizeof(DirectoryEntry);
			out_iop->m_Count = pInode->i_size / sizeof(DirectoryEntry) - out_iop->m_Offset / sizeof(DirectoryEntry);
		} else {
			out_iop->m_Offset = 0;

			/* 设置为目录项个数 ，含空白的目录项*/
			out_iop->m_Count = pInode->i_size / sizeof(DirectoryEntry);
			secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): mode != LIST; m_Offset=%d, m_Count=%d", out_iop->m_Offset, out_iop->m_Count);
		}
		freeEntryOffset = 0;
		pBuf = NULL;

		while (true)
		{
			/* 对目录项已经搜索完毕 */
			if ( 0 == out_iop->m_Count )
			{
				secondfs_dbg(DELOCATE, "FileManager::DELocate(): m_Count == 0, DE search complete");
				if ( NULL != pBuf )
				{
					bufMgr.Brelse(pBuf);
				}
				/* 如果是创建新文件 */
				if ( SECONDFS_CREATE == mode )
				{
					if ( freeEntryOffset )	/* 此变量存放了空闲目录项位于目录文件中的偏移量 */   /*问题：为何if分支没有置IUPD标志？  这是因为文件的长度没有变呀*/
					{
						/* 将空闲目录项偏移量保存，写目录项WriteDir()会用到 */
						out_iop->m_Offset = freeEntryOffset - sizeof(DirectoryEntry);
						secondfs_dbg(DELOCATE, "FileManager::DELocate(): mode == CREATE, found freeEntryOffset=%d", out_iop->m_Offset);
					}
					else /*目录项只能在末尾添加, Inode 长度更新*/
					{
						secondfs_dbg(DELOCATE, "FileManager::DELocate(): mode == CREATE, not found free entry, add new DE at tail");
						pInode->i_flag |= SECONDFS_IUPD;
					}
					/* 找到可以写入的空闲目录项位置，函数返回 */
					*inop = 0;
					return 0;
				}

				/* 如果是判断目录是否非空 */
				if ( SECONDFS_CHECKEMPTY == mode )
				{
					secondfs_dbg(DELOCATE, "FileManager::DELocate(): mode == CHECKEMPTY, m_Offset=1");
					out_iop->m_Offset = 1;
					return 0;
				}
				

				/* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源，并推出 */
				if (SECONDFS_LIST != mode)
				{
					secondfs_dbg(DELOCATE, "FileManager::DELocate(): ENOENT");
					ret = -ENOENT;
					*inop = 0;
				}
				else {
					secondfs_dbg(DELOCATE, "FileManager::DELocate(): finished");
					ret = 0;
				}

				// m_Offset 减去加过头的 sizeof(DirectoryEntry)
				out_iop->m_Offset -= sizeof(DirectoryEntry);
				goto out;
			}

			/* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
			if ( 0 == out_iop->m_Offset % SECONDFS_BLOCK_SIZE || firstTimeRead  )
			{
				firstTimeRead = 0;
				if ( NULL != pBuf )
				{
					bufMgr.Brelse(pBuf);
				}
				/* 计算要读的物理盘块号 */
				secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): finish current block || firstTimeRead; Bmap(%d)", out_iop->m_Offset / SECONDFS_BLOCK_SIZE);
				int phyBlkno = pInode->Bmap(out_iop->m_Offset / SECONDFS_BLOCK_SIZE );
				secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): after Bmap(%d) == %d", out_iop->m_Offset / SECONDFS_BLOCK_SIZE, phyBlkno);
				pBuf = bufMgr.Bread(pInode->i_ssb->s_dev, phyBlkno );
				// We just hard-code IS_ERR() macro here
				if ((uintptr_t)(pBuf) >= (uintptr_t)-4095) {
					secondfs_err("FileManager::DELocate(): Bread() fail! (%d)", (int)(uintptr_t)(pBuf));
					return (int)(uintptr_t)(pBuf);
				}
			}

			/* 没有读完当前目录项盘块，则读取下一目录项至u.u_dent */
			secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): load next DE: m_Offset=%d", out_iop->m_Offset);
			u8* src =(pBuf->b_addr + (out_iop->m_Offset % SECONDFS_BLOCK_SIZE));
			memcpy(&dent, src, sizeof(DirectoryEntry));

			out_iop->m_Offset += (SECONDFS_DIRSIZ + 4);
			secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): after load next DE: m_Offset=%d, ino=%d, currname=%0.32s", out_iop->m_Offset, le32_to_cpu(dent.m_ino), dent.m_name);
			if (SECONDFS_LIST == mode) {
				secondfs_c_helper_set_loff_t(ppos, has_dots == 0xffffffff ? out_iop->m_Offset : (out_iop->m_Offset + sizeof(DirectoryEntry) * 2));
			}
			out_iop->m_Count--;

			/* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
			if ( 0 == le32_to_cpu(dent.m_ino) )
			{
				secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): found empty DE slot");
				if ( 0 == freeEntryOffset )
				{
					// 注意: 加过头了, 之后得减掉一个 DE
					freeEntryOffset = out_iop->m_Offset;
				}
				/* 跳过空闲目录项，继续比较下一目录项 */
				continue;
			}

			// 我们在这里忽略 "." 和 ".."
			// 留给上层去主动回调 "." 和 ".." 目录项
			if (SECONDFS_OPEN_NOT_IGNORE_DOTS != mode)
				if (dent.m_name[0] == '.' && ((dent.m_name[1] == '.' && dent.m_name[2] == '\0')
					|| dent.m_name[1] == '\0')) {
					secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): found dot or dotdot");
					continue;
				}

			/* 走到这里, 说明至少有一个空闲目录项. CHECKEMPTY 可以返回了 */
			/* 如果是判断目录是否非空 */
			if ( SECONDFS_CHECKEMPTY == mode )
			{
				secondfs_dbg(DELOCATE, "FileManager::DELocate(): mode == CHECKEMPTY, return 0");
				out_iop->m_Offset = 0;
				if ( NULL != pBuf )
				{
					bufMgr.Brelse(pBuf);
				}
				return 0;
			}

			// 遍历模式, 执行一次回调函数
			if (SECONDFS_LIST == mode) {
				bool result;

				u8 *p = dent.m_name;
				while(*p && p - dent.m_name < sizeof(dent.m_name) / sizeof(*p)) {
					p++;
				}

				secondfs_dbg(DELOCATE, "FileManager::DELocate(): mode == LIST; currname=%0.32s, ino=%d", dent.m_name, le32_to_cpu(dent.m_ino));

				// 从目录项看不出这个文件属于哪种类别, 所以只能 DT_UNKNOWN
				result = dir_emit(ctx, (const char *)dent.m_name, p - dent.m_name, le32_to_cpu(dent.m_ino), *type);
				if (!result) {
					// 上层发出了停止信号
					secondfs_dbg(DELOCATE, "FileManager::DELocate(): mode == LIST; master interrupt");
					break;
				}
				continue;
			}

			int i;
			bool matchSuc = false;
			for ( i = 0; i < SECONDFS_DIRSIZ; i++ )
			{
				if (i >= namelen)
				{
					break;
				}
				if ( name[i] != dent.m_name[i] )
				{
					break;	/* 匹配至某一字符不符，跳出for循环 */
				}
				if (i == namelen - 1 && (i == SECONDFS_DIRSIZ - 1 || dent.m_name[i + 1] == '\0'))
				{
					matchSuc = true;
					break;
				}
			}

			secondfs_dbg(DELOCATE_V, "FileManager::DELocate(): currname=%0.32s, matchSuc=%d", dent.m_name, (int)matchSuc);

			if( ! matchSuc )
			{
				/* 不是要搜索的目录项，继续匹配下一目录项 */
				continue;
			}
			else
			{
				/* 目录项匹配成功，跳出While(true)循环 */
				secondfs_dbg(DELOCATE, "FileManager::DELocate(): currname=%0.32s, match!", dent.m_name);
				out_iop -= sizeof(DirectoryEntry);
				break;
			}
		}

		/* 
		 * 从内层目录项匹配循环跳至此处，说明pathname中
		 * 当前路径分量匹配成功了
		 */
		if ( NULL != pBuf )
		{
			bufMgr.Brelse(pBuf);
		}

		// 处理 LIST 模式上层给出停止信号的情况
		if (SECONDFS_LIST == mode) {
			return 0;
		} else {
			*inop = le32_to_cpu(dent.m_ino);
		}

		// 如果当前是创建模式的话, 说明文件已经在目录项内存在, 此时是错误的
		if (mode == SECONDFS_CREATE) {
			secondfs_dbg(FILE, "FileManager::DELocate(): EEXIST");
			out_iop -= sizeof(DirectoryEntry);
			return -EEXIST;
		}

		/* 
		 * 匹配目录项成功，则释放当前目录Inode，根据匹配成功的
		 * 目录项m_ino字段获取相应下一级目录或文件的Inode。
		 */
		// 无需释放和获取了
		// this->m_InodeTable->IPut(pInode);
		// pInode = this->m_InodeTable->IGet(dev, u.u_dent.m_ino);
		/* 回到外层While(true)循环，继续匹配Pathname中下一路径分量 */

		//if ( NULL == pInode )	/* 获取失败 */
		//{
		//	return NULL;
		//}
	}
out:
	return ret;
}

#if false
char FileManager::NextChar()
{
	User& u = Kernel::Instance().GetUser();
	
	/* u.u_dirp指向pathname中的字符 */
	return *u.u_dirp++;
}

/* 由creat调用。
 * 为新创建的文件写新的i节点和新的目录项
 * 返回的pInode是上了锁的内存i节点，其中的i_count是 1。
 *
 * 在程序的最后会调用 WriteDir，在这里把属于自己的目录项写进父目录，修改父目录文件的i节点 、将其写回磁盘。
 *
 */
Inode* FileManager::MakNode( unsigned int mode )
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();

	/* 分配一个空闲DiskInode，里面内容已全部清空 */
	pInode = this->m_FileSystem->IAlloc(u.u_pdir->i_dev);
	if( NULL ==	pInode )
	{
		return NULL;
	}

	pInode->i_flag |= (Inode::IACC | Inode::IUPD);
	pInode->i_mode = mode | Inode::IALLOC;
	pInode->i_nlink = 1;
	pInode->i_uid = u.u_uid;
	pInode->i_gid = u.u_gid;
	/* 将目录项写入u.u_dent，随后写入目录文件 */
	this->WriteDir(pInode);
	return pInode;
}

void FileManager::WriteDir( Inode* pInode )
{
	User& u = Kernel::Instance().GetUser();

	/* 设置目录项中Inode编号部分 */
	u.u_dent.m_ino = pInode->i_number;

	/* 设置目录项中pathname分量部分 */
	for ( int i = 0; i < DirectoryEntry::DIRSIZ; i++ )
	{
		u.u_dent.m_name[i] = u.u_dbuf[i];
	}

	u.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;
	u.u_segflg = 1;

	/* 将目录项写入父目录文件 */
	u.u_pdir->WriteI();
	this->m_InodeTable->IPut(u.u_pdir);
}

void FileManager::SetCurDir(char* pathname)
{
	User& u = Kernel::Instance().GetUser();
	
	/* 路径不是从根目录'/'开始，则在现有u.u_curdir后面加上当前路径分量 */
	if ( pathname[0] != '/' )
	{
		int length = Utility::StringLength(u.u_curdir);
		if ( u.u_curdir[length - 1] != '/' )
		{
			u.u_curdir[length] = '/';
			length++;
		}
		Utility::StringCopy(pathname, u.u_curdir + length);
	}
	else	/* 如果是从根目录'/'开始，则取代原有工作目录 */
	{
		Utility::StringCopy(pathname, u.u_curdir);
	}
}

/*
 * 返回值是0，表示拥有打开文件的权限；1表示没有所需的访问权限。文件未能打开的原因记录在u.u_error变量中。
 */
int FileManager::Access( Inode* pInode, unsigned int mode )
{
	User& u = Kernel::Instance().GetUser();

	/* 对于写的权限，必须检查该文件系统是否是只读的 */
	if ( Inode::IWRITE == mode )
	{
		if( this->m_FileSystem->GetFS(pInode->i_dev)->s_ronly != 0 )
		{
			u.u_error = User::EROFS;
			return 1;
		}
	}
	/* 
	 * 对于超级用户，读写任何文件都是允许的
	 * 而要执行某文件时，必须在i_mode有可执行标志
	 */
	if ( u.u_uid == 0 )
	{
		if ( Inode::IEXEC == mode && ( pInode->i_mode & (Inode::IEXEC | (Inode::IEXEC >> 3) | (Inode::IEXEC >> 6)) ) == 0 )
		{
			u.u_error = User::EACCES;
			return 1;
		}
		return 0;	/* Permission Check Succeed! */
	}
	if ( u.u_uid != pInode->i_uid )
	{
		mode = mode >> 3;
		if ( u.u_gid != pInode->i_gid )
		{
			mode = mode >> 3;
		}
	}
	if ( (pInode->i_mode & mode) != 0 )
	{
		return 0;
	}

	u.u_error = User::EACCES;
	return 1;
}

Inode* FileManager::Owner()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();

	if ( (pInode = this->NameI(NextChar, FileManager::OPEN)) == NULL )
	{
		return NULL;
	}

	if ( u.u_uid == pInode->i_uid || u.SUser() )
	{
		return pInode;
	}

	this->m_InodeTable->IPut(pInode);
	return NULL;
}

void FileManager::ChMod()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();
	unsigned int mode = u.u_arg[1];

	if ( (pInode = this->Owner()) == NULL )
	{
		return;
	}
	/* clear i_mode字段中的ISGID, ISUID, ISTVX以及rwxrwxrwx这12比特 */
	pInode->i_mode &= (~0xFFF);
	/* 根据系统调用的参数重新设置i_mode字段 */
	pInode->i_mode |= (mode & 0xFFF);
	pInode->i_flag |= Inode::IUPD;

	this->m_InodeTable->IPut(pInode);
	return;
}

void FileManager::ChOwn()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();
	short uid = u.u_arg[1];
	short gid = u.u_arg[2];

	/* 不是超级用户或者不是文件主则返回 */
	if ( !u.SUser() || (pInode = this->Owner()) == NULL )
	{
		return;
	}
	pInode->i_uid = uid;
	pInode->i_gid = gid;
	pInode->i_flag |= Inode::IUPD;

	this->m_InodeTable->IPut(pInode);
}

void FileManager::ChDir()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN);
	if ( NULL == pInode )
	{
		return;
	}
	/* 搜索到的文件不是目录文件 */
	if ( (pInode->i_mode & Inode::IFMT) != Inode::IFDIR )
	{
		u.u_error = User::ENOTDIR;
		this->m_InodeTable->IPut(pInode);
		return;
	}
	if ( this->Access(pInode, Inode::IEXEC) )
	{
		this->m_InodeTable->IPut(pInode);
		return;
	}
	this->m_InodeTable->IPut(u.u_cdir);
	u.u_cdir = pInode;
	pInode->Prele();

	this->SetCurDir((char *)u.u_arg[0] /* pathname */);
}

void FileManager::Link()
{
	Inode* pInode;
	Inode* pNewInode;
	User& u = Kernel::Instance().GetUser();

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN);
	/* 打卡文件失败 */
	if ( NULL == pInode )
	{
		return;
	}
	/* 链接的数量已经最大 */
	if ( pInode->i_nlink >= 255 )
	{
		u.u_error = User::EMLINK;
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 对目录文件的链接只能由超级用户进行 */
	if ( (pInode->i_mode & Inode::IFMT) == Inode::IFDIR && !u.SUser() )
	{
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* 解锁现存文件Inode,以避免在以下搜索新文件时产生死锁 */
	pInode->i_flag &= (~Inode::ILOCK);
	/* 指向要创建的新路径newPathname */
	u.u_dirp = (char *)u.u_arg[1];
	pNewInode = this->NameI(FileManager::NextChar, FileManager::CREATE);
	/* 如果文件已存在 */
	if ( NULL != pNewInode )
	{
		u.u_error = User::EEXIST;
		this->m_InodeTable->IPut(pNewInode);
	}
	if ( User::NOERROR != u.u_error )
	{
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 检查目录与该文件是否在同一个设备上 */
	if ( u.u_pdir->i_dev != pInode->i_dev )
	{
		this->m_InodeTable->IPut(u.u_pdir);
		u.u_error = User::EXDEV;
		/* 出错，释放资源并退出 */
		this->m_InodeTable->IPut(pInode);
		return;
	}

	this->WriteDir(pInode);
	pInode->i_nlink++;
	pInode->i_flag |= Inode::IUPD;
	this->m_InodeTable->IPut(pInode);
}

void FileManager::UnLink()
{
	Inode* pInode;
	Inode* pDeleteInode;
	User& u = Kernel::Instance().GetUser();

	pDeleteInode = this->NameI(FileManager::NextChar, FileManager::DELETE);
	if ( NULL == pDeleteInode )
	{
		return;
	}
	pDeleteInode->Prele();

	pInode = this->m_InodeTable->IGet(pDeleteInode->i_dev, u.u_dent.m_ino);
	if ( NULL == pInode )
	{
		Utility::Panic("unlink -- iget");
	}
	/* 只有root可以unlink目录文件 */
	if ( (pInode->i_mode & Inode::IFMT) == Inode::IFDIR && !u.SUser() )
	{
		this->m_InodeTable->IPut(pDeleteInode);
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* 写入清零后的目录项 */
	u.u_IOParam.m_Offset -= (DirectoryEntry::DIRSIZ + 4);
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;
	u.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	
	u.u_dent.m_ino = 0;
	pDeleteInode->WriteI();

	/* 修改inode项 */
	pInode->i_nlink--;
	pInode->i_flag |= Inode::IUPD;

	this->m_InodeTable->IPut(pDeleteInode);
	this->m_InodeTable->IPut(pInode);
}

void FileManager::MkNod()
{
	Inode* pInode;
	User& u = Kernel::Instance().GetUser();

	/* 检查uid是否是root，该系统调用只有uid==root时才可被调用 */
	if ( u.SUser() )
	{
		pInode = this->NameI(FileManager::NextChar, FileManager::CREATE);
		/* 要创建的文件已经存在,这里并不能去覆盖此文件 */
		if ( pInode != NULL )
		{
			u.u_error = User::EEXIST;
			this->m_InodeTable->IPut(pInode);
			return;
		}
	}
	else
	{
		/* 非root用户执行mknod()系统调用返回User::EPERM */
		u.u_error = User::EPERM;
		return;
	}
	/* 没有通过SUser()的检查 */
	if ( User::NOERROR != u.u_error )
	{
		return;	/* 没有需要释放的资源，直接退出 */
	}
	pInode = this->MakNode(u.u_arg[1]);
	if ( NULL == pInode )
	{
		return;
	}
	/* 所建立是设备文件 */
	if ( (pInode->i_mode & (Inode::IFBLK | Inode::IFCHR)) != 0 )
	{
		pInode->i_addr[0] = u.u_arg[2];
	}
	this->m_InodeTable->IPut(pInode);
}
#endif

// @Feng Shun: 以下为 C wrapping 部分
extern "C" {
	const u32
		SECONDFS_FREAD = 0x1,
		SECONDFS_FWRITE = 0x2,
		SECONDFS_FPIPE = 0x4
	;

	const u32
		SECONDFS_OPEN = 0,		/* 以打开文件方式搜索目录 */
		SECONDFS_CREATE = 1,		/* 以新建文件方式搜索目录 */
		SECONDFS_DELETE = 2,		/* 以删除文件方式搜索目录 */
		SECONDFS_CHECKEMPTY = 3,	/* 检测目录是否非空 */
		SECONDFS_LIST = 4,		/* 遍历目录 */
		SECONDFS_OPEN_NOT_IGNORE_DOTS = 5	/* 以打开文件方式搜索目录, 不要跳过 "." 和 ".." */
	;

	SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(FileManager);

}
