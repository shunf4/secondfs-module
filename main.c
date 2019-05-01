#include "secondfs.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

/* 本软件撰写方针:
 *   * 贴合内核
 *   * 尽量重用 UnixV6++ 的 C++ 代码
 *   * 其他从简, 包括错误处理
 *   * Unix V6++ 的默认端序(小端序)有可能与本机不一样, 所以
 *     处理方法为: SuperBlock 和 DiskInode 结构不转换, 
 *     为小端序; 在向它读取和写入数据时, 做转换. 
 *     其他为本机序.
 *     这也就导致一个不优雅的地方:
 *       SuperBlock 写回时可以直接写回;
 *       内存中 Inode 结构则需转换为小端序的 DiskInode 再写回
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
struct file_system_type secondfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "secondfs",
	.mount = secondfs_mount,
	.kill_sb = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV
};


// 要注册的超块操作函数表
const struct super_operations secondfs_sb_ops = {
	.alloc_inode	= secondfs_alloc_inode,
	.destroy_inode	= secondfs_destroy_inode,
	.write_inode	= secondfs_write_inode,
	.evict_inode	= ext2_evict_inode,
	.put_super	= ext2_put_super,
	.sync_fs	= ext2_sync_fs,
	.freeze_fs	= ext2_freeze,
	.unfreeze_fs	= ext2_unfreeze,
	.statfs		= ext2_statfs,
	.remount_fs	= ext2_remount,
	.show_options	= ext2_show_options,
#ifdef CONFIG_QUOTA
	.quota_read	= ext2_quota_read,
	.quota_write	= ext2_quota_write,
	.get_dquots	= ext2_get_dquots,
#endif
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