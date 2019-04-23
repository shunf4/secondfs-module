#include "secondfs_kern.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shunf4");
MODULE_DESCRIPTION("A simple Linux Filesystem based on UNIXV6++.");
MODULE_VERSION("0.1");

static char *name = "world";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");

static int __init secondfs_init(void) {
	printk(KERN_INFO "FS: Hello %s from the FS' FS module!\n", name);
	return 0;
}

static void __exit secondfs_exit(void) {
	printk(KERN_INFO "FS: Goodbye %s!\n", name);
}

module_init(secondfs_init);
module_exit(secondfs_exit);