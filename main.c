#include "secondfs.h"

/* Guidelines:
 *   * Close to kernel coding style
 *   * Make best use of UnixV6++ codes
 *   * Simplify everything
 *   * Unix V6++ Filesystem(Second Filesystem) uses *Little Endian*.
 *     Thus some structures related to the filesystem might have
 *     different endian to the system. Here's the rules of endian:
 * 
 *     SuperBlock, DirectoryEntry & DiskInode struct remains little
 *     endian, while other struct is converted to current CPU endian.
 * 
 *   * C-style cast is used in casting unsigned int to signed int.
 *     (Though it is implementation-defined) Please do not bother,
 *     language lawyers!
 */

/* 本软件撰写方针:
 *   * 贴合内核
 *   * 尽量重用 UnixV6++ 的 C++ 代码
 *   * 其他从简, 包括错误处理
 *   * Unix V6++ 的默认端序(小端序)有可能与本机不一样, 所以
 *     处理方法为: SuperBlock, DirectoryEntry 和 DiskInode 结构不转换, 
 *     为小端序; 在向它读取和写入数据时, 做转换. 
 *     其他为本机序.
 *     这也就导致一个不优雅的地方:
 *       SuperBlock 写回时可以直接写回;
 *       内存中 Inode 结构则需转换为小端序的 DiskInode 再写回
 *   * 将 unsigned int 转换为 signed int 的方法
 *     一律是 C 风格 cast. 我知道这在 C++ 里是实现依赖的行为,
 *     语言律师退散!
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shunf4");
MODULE_DESCRIPTION("UNIXV6++ filesystem. (https://gitee.com/solym/UNIX_V6PP)");
MODULE_VERSION("0.1");

static char *username = "user";
module_param(username, charp, S_IRUGO);
MODULE_PARM_DESC(username, "The user's name to display a hello world message in /var/log/kern.log");

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的各数据结构
// Kernel cache (slab) descriptor for frequently allocated
// & disposed structs for secondfs
struct kmem_cache *secondfs_diskinode_cachep;
struct kmem_cache *secondfs_icachep;

// 一次性的对象定义
// Some one-time objects
BufferManager *secondfs_buffermanagerp;
FileSystem *secondfs_filesystemp;
FileManager *secondfs_filemanagerp;


// 要注册的文件系统结构
// File system structure to register to system
struct file_system_type secondfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "secondfs",
	.mount = secondfs_mount,
	.kill_sb = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV
};

// 要注册的超块操作函数表
// Superblock manipulation functions to register to system
struct super_operations secondfs_sb_ops = {
	.alloc_inode	= secondfs_alloc_inode,
	.destroy_inode	= secondfs_destroy_inode,
	.write_inode	= secondfs_write_inode,
	.evict_inode	= secondfs_evict_inode,
	.put_super	= secondfs_put_super,
	.sync_fs	= secondfs_sync_fs,
	.dirty_inode	= secondfs_dirty_inode,
	//.statfs		= secondfs_statfs,
	//.remount_fs	= secondfs_remount,
	//.show_options	= secondfs_show_options,
};

static int __init init_secondfs(void) {
	int ret = 0;

	// Create kernel cache descriptor for frequently allocated datastruct.
	// 为频繁创建的数据结构 (具有恒定大小的内存空间表示) 创建一套 kmem_cache, 用来快速动态分配
	secondfs_diskinode_cachep = kmem_cache_create("secondfs_diskinode_cache",
		sizeof(DiskInode),
		sizeof(DiskInode), 
		(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),
		NULL);

	if (!secondfs_diskinode_cachep) {
		return -ENOMEM;
	}
	
	secondfs_icachep = kmem_cache_create("secondfs_icache",
		sizeof(Inode),
		sizeof(Inode), 
		(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD | SLAB_HWCACHE_ALIGN),
		NULL);

	if (!secondfs_icachep) {
		kmem_cache_destroy(secondfs_diskinode_cachep);
		return -ENOMEM;
	}

	// Check consistency of sizeof() various datastructs from C part and C++ part.
	secondfs_dbg(SIZECONSISTENCY, "Buf size : %u %lu\n", SECONDFS_SIZEOF_Buf, sizeof(Buf));
	secondfs_dbg(SIZECONSISTENCY, "BufferManager size : %u %lu\n", SECONDFS_SIZEOF_BufferManager, sizeof(BufferManager));
	secondfs_dbg(SIZECONSISTENCY, "Devtab size : %u %lu\n", SECONDFS_SIZEOF_Devtab, sizeof(Devtab));
	secondfs_dbg(SIZECONSISTENCY, "DiskInode size : %u %lu\n", SECONDFS_SIZEOF_DiskInode, sizeof(DiskInode));
	secondfs_dbg(SIZECONSISTENCY, "FileSystem size : %u %lu\n", SECONDFS_SIZEOF_FileSystem, sizeof(FileSystem));
	secondfs_dbg(SIZECONSISTENCY, "Inode size : %u %lu\n", SECONDFS_SIZEOF_Inode, sizeof(Inode));
	secondfs_dbg(SIZECONSISTENCY, "SuperBlock size : %u %lu\n", SECONDFS_SIZEOF_SuperBlock, sizeof(SuperBlock));
	secondfs_dbg(SIZECONSISTENCY, "FileManager size : %u %lu\n", SECONDFS_SIZEOF_FileManager, sizeof(FileManager));
	secondfs_dbg(SIZECONSISTENCY, "DirectoryEntry size : %u %lu\n", SECONDFS_SIZEOF_DirectoryEntry, sizeof(DirectoryEntry));

	if (		SECONDFS_SIZEOF_Buf		!= sizeof(Buf)
		||	SECONDFS_SIZEOF_BufferManager	!= sizeof(BufferManager)
		||	SECONDFS_SIZEOF_Devtab		!= sizeof(Devtab)
		||	SECONDFS_SIZEOF_DiskInode	!= sizeof(DiskInode)
		||	SECONDFS_SIZEOF_FileSystem	!= sizeof(FileSystem)
		||	SECONDFS_SIZEOF_Inode		!= sizeof(Inode)
		||	SECONDFS_SIZEOF_SuperBlock	!= sizeof(SuperBlock)
		||	SECONDFS_SIZEOF_FileManager	!= sizeof(FileManager)
		||	SECONDFS_SIZEOF_DirectoryEntry	!= sizeof(DirectoryEntry)
	) {
		// Sizes match error
		secondfs_err("sizeof() does not match! secondfs refuses to load.");
		return -EPERM;
	}

	// Initialize one-time objects
	// 初始化一次性的对象
	secondfs_buffermanagerp = newBufferManager();
	secondfs_filesystemp = newFileSystem();
	secondfs_filemanagerp = newFileManager();

	if (!secondfs_buffermanagerp || !secondfs_filesystemp || !secondfs_filemanagerp) {
		if (secondfs_buffermanagerp)
			deleteBufferManager(secondfs_buffermanagerp);
		if (secondfs_filesystemp)
			deleteFileSystem(secondfs_filesystemp);
		if (secondfs_filemanagerp)
			deleteFileManager(secondfs_filemanagerp);
		return -ENOMEM;
	}

	BufferManager_Initialize(secondfs_buffermanagerp);
	FileSystem_Initialize(secondfs_filesystemp);
	secondfs_filesystemp->m_BufferManager = secondfs_buffermanagerp;

	// 注册文件系统
	ret = register_filesystem(&secondfs_fs_type);

	if (likely(ret == 0)) {
		// 打印“Hello world”信息
		secondfs_info("Hello %s!", username);
	} else {
		secondfs_warn("%s, Unfortunately there is something wrong registering secondfs. Error code is %d\n", username, ret);
	}

	return ret;
}

static void __exit exit_secondfs(void) {
	int ret = 0;

	// 反注册文件系统
	ret = unregister_filesystem(&secondfs_fs_type);

	// 析构一次性的对象
	deleteBufferManager(secondfs_buffermanagerp);
	deleteFileSystem(secondfs_filesystemp);
	deleteFileManager(secondfs_filemanagerp);

	// 将所有数据结构的 kmem_cache 析构
	kmem_cache_destroy(secondfs_diskinode_cachep);
	kmem_cache_destroy(secondfs_icachep);

	if (likely(ret == 0)) {
		secondfs_info("Goodbye %s!", username);
	} else {
		printk(KERN_ERR "%s, Unfortunately there is something wrong unregistering secondfs. Error code is %d\n", username, ret);
	}
}

module_init(init_secondfs);
module_exit(exit_secondfs);