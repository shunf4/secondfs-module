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
#include <linux/slub_def.h>

#include "secondfs_user.h"
#include "utils.h"

#define DO_MEMDBG

#ifdef DO_MEMDBG
static unsigned int malloc_num;
static unsigned int free_num;
static unsigned int kmem_cache_malloc_num;
static unsigned int kmem_cache_free_num;
#endif

// 以下为 C 对实现 C++ 的 new/delete 操作符提供的助手函数
// 使用 kmalloc 实现:

void *secondfs_c_helper_malloc(size_t size)
{
	void *p;

#ifdef DO_MEMDBG
	malloc_num++;
#endif // DO_MEMDBG

	p = kmalloc(size, GFP_KERNEL);
	printk(KERN_DEBUG "SecondFS: Start mallocing, kmalloc size=%lu pointer=%p", size, p);
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

// 以下为 C 对实现 C++ 的 new/delete 操作符提供的助手函数
// 使用 kmem_cache 实现:

#ifdef DO_MEMDBG

#define SECONDFS_GEN_C_HELPER_KMEM_CACHE_ALLOC_N_FREE(class_name, kmem_cachep) \
void *secondfs_c_helper_kmem_cache_alloc_##class_name(size_t size) {\
	void *p;\
	BUG_ON(size != kmem_cachep->object_size);\
	p = kmem_cache_alloc(kmem_cachep, GFP_KERNEL);\
	printk(KERN_DEBUG "SecondFS: Start kmem_cache_allocing, size=%lu pointer=%p", kmem_cachep->object_size, p);\
	if (p == NULL)\
		printk(KERN_ERR "SecondFS: Unable to allocate memory");\
	return p;\
}\
\
void secondfs_c_helper_kmem_cache_free_##class_name(void *pointer) {\
	printk(KERN_DEBUG "SecondFS: Start freeing, kfree pointer=%p", pointer);\
	kmem_cache_free(kmem_cachep, pointer);\
}

#else // DO_MEMDBG

#define SECONDFS_GEN_C_HELPER_KMEM_CACHE_ALLOC_N_FREE(class_name, kmem_cachep) \
void *secondfs_c_helper_kmem_cache_alloc_##class_name(size_t size) {\
	void *p;\
	BUG_ON(size != kmem_cachep->object_size);\
	kmem_cache_malloc_num++;\
	p = kmem_cache_alloc(kmem_cachep, GFP_KERNEL);\
	printk(KERN_DEBUG "SecondFS: Start kmem_cache_allocing, size=%lu pointer=%p", kmem_cachep->object_size, p);\
	if (p == NULL)\
		printk(KERN_ERR "SecondFS: Unable to allocate memory");\
	return p;\
}\
\
void secondfs_c_helper_kmem_cache_free_##class_name(void *pointer) {\
	kmem_cache_free_num++;\
	printk(KERN_DEBUG "SecondFS: Start freeing, kfree pointer=%p", pointer);\
	kmem_cache_free(kmem_cachep, pointer);\
}

#endif // DO_MEMDBG

void secondfs_c_helper_kmem_cache_mdebug(void)
{
#ifdef DO_MEMDBG
	printk(KERN_DEBUG "SecondFS: kmem_cache_malloc_num is %u", kmem_cache_malloc_num);
	printk(KERN_DEBUG "SecondFS: kmem_cache_free_num is %u", kmem_cache_free_num);
#endif // DO_MEMDBG
}

SECONDFS_GEN_C_HELPER_KMEM_CACHE_ALLOC_N_FREE(DiskInode, secondfs_diskinode_cachep)
