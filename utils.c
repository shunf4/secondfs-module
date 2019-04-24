#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/timex.h>
#include <linux/cpufreq.h>

#include "utils.h"

#define DO_MEMDBG

#ifdef DO_MEMDBG
static unsigned int malloc_num;
static unsigned int free_num;
#endif

void *secondfs_c_helper_malloc(unsigned int size)
{
	void *p;

#ifdef DO_MEMDBG
	malloc_num++;
#endif // DO_MEMDBG

	p = kmalloc(size, GFP_KERNEL);
	printk(KERN_DEBUG "SecondFS: Start mallocing, kmalloc size=%d pointer=%p", size, p);
	if (p == NULL)
		printk(KERN_ERR "SecondFS: Unable to allocate memory");
	return p;
}

void secondfs_c_helper_free(void *pointer)
{
	printk(KERN_DEBUG "SecondFS: Start freeing, kfree pointer=%p", pointer);

#ifdef DO_MEMDBG
	free_num++;
#endif // DO_MEMDBG
	kfree(pointer);
}

void secondfs_c_helper_mdebug(void)
{
#ifdef DO_MEMDBG
	printk(KERN_DEBUG "SecondFS: malloc_num is %u", malloc_num);
	printk(KERN_DEBUG "SecondFS: free_num is %u", free_num);
#endif // DO_MEMDBG
}