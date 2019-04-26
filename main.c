#include "secondfs_kern.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shunf4");
MODULE_DESCRIPTION("A simple Linux Filesystem based on UNIXV6++.");
MODULE_VERSION("0.1");

static char *username = "user";
module_param(username, charp, S_IRUGO);
MODULE_PARM_DESC(username, "The user's name to display a hello world message in /var/log/kern.log");

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 DiskInode
struct kmem_cache *secondfs_diskinode_cachep;

static DiskInode *secondfs_test_diskinode;
static Inode *secondfs_test_inode;

static int __init secondfs_init(void) {
	// 为所有数据结构 (具有恒定大小的内存空间表示) 创建一套 kmem_cache, 用来快速动态分配
	
	secondfs_diskinode_cachep = kmem_cache_create("secondfs_diskinode_cache",
		sizeof(DiskInode),
		sizeof(DiskInode), 
		(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),
		NULL);

	// 打印“Hello world”信息
	secondfs_test_diskinode = newDiskInode();
	secondfs_test_inode = newInode();

	if (SECONDFS_DISKINODE_SIZE != sizeof(DiskInode)) {
		BUG();
	}

	printk(KERN_INFO "SecondFS: Hello %s from the FS' SecondFS module! The size of Inode is %d(in const variable) == %lu(in sizeof())\n", username, SECONDFS_DISKINODE_SIZE, sizeof(*secondfs_test_diskinode));

	return 0;
}

static void __exit secondfs_exit(void) {
	deleteDiskInode(secondfs_test_diskinode);
	deleteInode(secondfs_test_inode);
	kmem_cache_destroy(secondfs_diskinode_cachep);
	printk(KERN_INFO "SecondFS: Goodbye %s!\n", username);
}

module_init(secondfs_init);
module_exit(secondfs_exit);