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

// 内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 Inode
static struct kmem_cache *secondfs_inode_cachep;

static int __init secondfs_init(void) {
	// 打印“Hello world”信息
	printk(KERN_INFO "SecondFS: Hello %s from the FS' SecondFS module! The size of Inode is %d\n", username, SECONDFS_DISKINODE_SIZE);

	// 创建一个内核高速缓存 kmem_cache, 用来暂时存放 SecondFS 的 Inode
	// secondfs_inode_cachep = kmem_cache_create("secondfs_icache", )


	return 0;
}

static void __exit secondfs_exit(void) {
	printk(KERN_INFO "SecondFS: Goodbye %s!\n", username);
}

module_init(secondfs_init);
module_exit(secondfs_exit);