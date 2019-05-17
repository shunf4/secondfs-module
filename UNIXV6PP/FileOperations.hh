#ifndef __FILEOPERATIONS_HH__
#define __FILEOPERATIONS_HH__

#include <cstdint>
#include <cstddef>
#include "../common.h"
#include "Inode.hh"

#include "FileOperations_c_wrapper.h"


/* 
 * 文件管理类(FileManager)
 * 封装了文件系统的各种系统调用在核心态下处理过程，
 * 如对文件的Open()、Close()、Read()、Write()等等
 * 封装了对文件系统访问的具体细节。
 */
class FileManager
{
public:
	/* 目录搜索模式，用于NameI()函数 */
	enum DirectorySearchMode
	{
		OPEN = 0,		/* 以打开文件方式搜索目录 */
		CREATE = 1,		/* 以新建文件方式搜索目录 */
		DELETE = 2,		/* 以删除(=打开)文件方式搜索目录 */
		CHECKEMPTY = 3,
		LIST = 4,
		OPEN_NOT_IGNORE_DOTS = 5	/* 以打开文件方式搜索目录, 不要跳过 "." 和 ".." */
	};

	/* Functions */
public:
	/* Constructors */
	FileManager();
	/* Destructors */
	~FileManager();


	/* 
	 * @comment 初始化对全局对象的引用
	 */
	void Initialize();

	/* 
	 * @comment Open()系统调用处理过程
	 */
	void Open();

	/* 
	 * @comment Creat()系统调用处理过程
	 */
	void Creat();

	/* 
	 * @comment Open()、Creat()系统调用的公共部分
	 */
	void Open1(Inode* pInode, int mode, int trf);

	/* 
	 * @comment Close()系统调用处理过程
	 */
	void Close();

	/* 
	 * @comment Seek()系统调用处理过程
	 */
	void Seek();

	/* 
	 * @comment Dup()复制进程打开文件描述符
	 */
	void Dup();

	/* 
	 * @comment FStat()获取文件信息
	 */
	void FStat();

	/* 
	 * @comment FStat()获取文件信息
	 */
	void Stat();

	/* FStat()和Stat()系统调用的共享例程 */
	void Stat1(Inode* pInode, unsigned long statBuf);

	/* 
	 * @comment Read()系统调用处理过程
	 */
	u32 Read(u8 *buf, size_t len, u32 *ppos, Inode *inode);

	/* 
	 * @comment Write()系统调用处理过程
	 */
	u32 Write(const u8 *buf, size_t len, u32 *ppos, Inode *inode);

	/* 
	 * @comment 读写系统调用公共部分代码
	 */
	u32 Rdwr(u8 *buf, size_t len, u32 *ppos, IOParameter *io_paramp, Inode *inode, u32 /* enum File::FileFlags */ mode);

	/* 
	 * @comment Pipe()管道建立系统调用处理过程
	 */
	void Pipe();

	/* 
	 * @comment 管道读操作
	 */
	void Read(IOParameter *io_param, Inode *inode);

	/* 
	 * @comment 管道写操作
	 */
	void WriteP(IOParameter *io_param, Inode *inode);
	
	/* 
	 * @comment 目录搜索，将路径转化为相应的Inode，
	 * 返回上锁后的Inode
	 */
	Inode* NameI(char (*func)(), enum DirectorySearchMode mode);

	/* 
	 * @comment 仅在某一目录下搜索, 节选了 NameI 函数
	 */
	int DELocate(Inode *dir, const char *name, u32 namelen, u32 mode, IOParameter *out_iop, u32 *inop);

	/* 
	 * @comment 获取路径中的下一个字符
	 */
	static char NextChar();

	/* 
	 * @comment 被Creat()系统调用使用，用于为创建新文件分配内核资源
	 */
	Inode* MakNode(unsigned int mode);

	/* 
	 * @comment 向父目录的目录文件写入一个目录项
	 */
	void WriteDir(Inode* pInode);

	/*
	 * @comment 设置当前工作路径
	 */
	void SetCurDir(char* pathname);

	/* 
	 * @comment 检查对文件或目录的搜索、访问权限，作为系统调用的辅助函数
	 */
	int Access(Inode* pInode, unsigned int mode);

	/* 
	 * @comment 检查文件是否属于当前用户
	 */
	Inode* Owner();

	/* 改变文件访问模式 */
	void ChMod();

	/* 改变文件文件所有者user ID及其group ID */
	void ChOwn();

	/* 改变当前工作目录 */
	void ChDir();

	/* 创建文件的异名引用 */
	void Link();

	/* 取消文件 */
	void UnLink();

	/* 用于建立特殊设备文件的系统调用 */
	void MkNod();
	
public:
};

#endif // __FILEOPERATIONS_HH__