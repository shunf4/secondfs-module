#include "secondfs_kern.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

/* 本软件撰写方针:
 *   * 贴合内核
 *   * 尽量重用 UnixV6++ 的 C++ 代码
 *   * 其他从简, 包括错误处理
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shunf4");
MODULE_DESCRIPTION("A simple Linux Filesystem based on UNIXV6++.");
MODULE_VERSION("0.1");

static char *username = "user";
module_param(username, charp, S_IRUGO);
MODULE_PARM_DESC(username, "The user's name to display a hello world message in /var/log/kern.log");

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的各数据结构
struct kmem_cache *secondfs_diskinode_cachep;
struct kmem_cache *secondfs_icachep;

// 一次性的对象声明
BufferManager *secondfs_buffermanagerp;
FileSystem *secondfs_filesystemp;
// Inode 表


// 要注册的文件系统结构
/*
struct file_system_type secondfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "secondfs",
	.mount = secondfs_mount,
	.kill_sb = secondfs_kill_superblock,
	.fs_flags = FS_REQUIRES_DEV
};
*/

// 要注册的超块操作函数表
const struct super_operations secondfs_sb_ops = {
	.destroy_inode = NULL,
	.put_super = NULL
};

static int __init secondfs_init(void) {
	int ret = 0;

	// 为所有数据结构 (具有恒定大小的内存空间表示) 创建一套 kmem_cache, 用来快速动态分配
	secondfs_diskinode_cachep = kmem_cache_create("secondfs_diskinode_cache",
		sizeof(DiskInode),
		sizeof(DiskInode), 
		(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),
		NULL);
	
	secondfs_icachep = kmem_cache_create("secondfs_icache",
		sizeof(Inode),
		sizeof(Inode), 
		(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD | SLAB_HWCACHE_ALIGN),
		NULL);

	printk(KERN_INFO "secondfs: Buf size : %u %lu\n", SECONDFS_SIZEOF_Buf, sizeof(Buf));
	printk(KERN_INFO "secondfs: BufferManager size : %u %lu\n", SECONDFS_SIZEOF_BufferManager, sizeof(BufferManager));
	printk(KERN_INFO "secondfs: Devtab size : %u %lu\n", SECONDFS_SIZEOF_Devtab, sizeof(Devtab));
	printk(KERN_INFO "secondfs: DiskInode size : %u %lu\n", SECONDFS_SIZEOF_DiskInode, sizeof(DiskInode));
	printk(KERN_INFO "secondfs: FileSystem size : %u %lu\n", SECONDFS_SIZEOF_FileSystem, sizeof(FileSystem));
	printk(KERN_INFO "secondfs: Inode size : %u %lu\n", SECONDFS_SIZEOF_Inode, sizeof(Inode));
	printk(KERN_INFO "secondfs: SuperBlock size : %u %lu\n", SECONDFS_SIZEOF_SuperBlock, sizeof(SuperBlock));

	// 初始化一次性的对象
	secondfs_buffermanagerp = newBufferManager();
	BufferManager_Initialize(secondfs_buffermanagerp);

	secondfs_filesystemp = newFileSystem();
	FileSystem_Initialize(secondfs_filesystemp);

	if (	0
		|| !secondfs_diskinode_cachep
		|| !secondfs_icachep
	) {
		// IMPROVE: 释放已成功申请的 kmem_cache
		return -ENOMEM;
	}

	// 注册文件系统
	//ret = register_filesystem(&secondfs_fs_type);

	if (likely(ret == 0)) {
		// 打印“Hello world”信息
		printk(KERN_INFO "secondfs: Hello %s from the FS' SecondFS module!\n", username);
	} else {
		printk(KERN_ERR "secondfs: %s, Unfortunately there is something wrong registering secondfs. Error code is %d\n", username, ret);
	}

	return ret;
}

static void __exit secondfs_exit(void) {
	int ret = 0;

	// 反注册文件系统
	//ret = unregister_filesystem(&secondfs_fs_type);

	// 析构一次性的对象
	deleteBufferManager(secondfs_buffermanagerp);
	deleteFileSystem(secondfs_filesystemp);

	// 将所有数据结构的 kmem_cache 析构
	kmem_cache_destroy(secondfs_diskinode_cachep);
	kmem_cache_destroy(secondfs_icachep);

	if (likely(ret == 0)) {
		printk(KERN_INFO "secondfs: Goodbye %s!\n", username);
	} else {
		printk(KERN_ERR "secondfs: %s, Unfortunately there is something wrong unregistering secondfs. Error code is %d\n", username, ret);
	}
}

module_init(secondfs_init);
module_exit(secondfs_exit);