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

#include "secondfs.h"

#undef __cplusplus
#include "c_helper_for_cc.h"


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
	// 这个是通用的 C++ new helper, 用 kmalloc 实现. 但对于每一个
	// C++ 数据结构我们最好还是用类内重载的 new/delete 用 kmem_cache 实现.
	// 因此这里直接 BUG();
	// Update: 有的时候基于 kmalloc 就够了, 不 BUG() 了
	void *p;

#ifdef SECONDFS_BUG_ON_KMALLOC_BASED_NEW_DELETE
	printk(KERN_ERR "secondfs: general new operator called!!");
	BUG();
#endif // SECONDFS_BUG_ON_KMALLOC_BASED_NEW_DELETE

#ifdef DO_MEMDBG
	malloc_num++;
#endif // DO_MEMDBG

	p = kmalloc(size, GFP_KERNEL);
	printk(KERN_DEBUG "secondfs: Start mallocing, kmalloc size=%lu pointer=%p", size, p);
	if (p == NULL)
		printk(KERN_ERR "secondfs: Unable to allocate memory");
	return p;
}

void secondfs_c_helper_free(void *pointer)
{
	// 这个是通用的 C++ new helper, 用 kmalloc 实现. 但对于每一个
	// C++ 数据结构我们最好还是用类内重载的 new/delete 用 kmem_cache 实现.
	// 因此这里直接 BUG();
	// Update: 有的时候基于 kmalloc 就够了, 不 BUG() 了
#ifdef SECONDFS_BUG_ON_KMALLOC_BASED_NEW_DELETE
	printk(KERN_ERR "secondfs: general delete operator called!!");
	BUG();
#endif // SECONDFS_BUG_ON_KMALLOC_BASED_NEW_DELETE

	printk(KERN_DEBUG "secondfs: Start freeing, kfree pointer=%p", pointer);

#ifdef DO_MEMDBG
	free_num++;
#endif // DO_MEMDBG
	kfree(pointer);
}

void secondfs_c_helper_mdebug(void)
{
#ifdef DO_MEMDBG
	printk(KERN_DEBUG "secondfs: malloc_num is %u", malloc_num);
	printk(KERN_DEBUG "secondfs: free_num is %u", free_num);
#endif // DO_MEMDBG
}

// 以下为 C 对实现 C++ 的 new/delete 操作符提供的助手函数
// 使用 kmem_cache 实现:

#ifdef DO_MEMDBG

#define SECONDFS_GEN_C_HELPER_KMEM_CACHE_ALLOC_N_FREE(class_name, kmem_cachep) \
void *secondfs_c_helper_kmem_cache_alloc_##class_name(size_t size)\
{\
	void *p;\
	BUG_ON(size != kmem_cachep->object_size);\
	p = kmem_cache_alloc(kmem_cachep, GFP_KERNEL);\
	printk(KERN_DEBUG "secondfs: Start kmem_cache_allocing, size=%u pointer=%p", kmem_cachep->object_size, p);\
	if (p == NULL)\
		printk(KERN_ERR "secondfs: Unable to allocate memory");\
	return p;\
}\
\
void secondfs_c_helper_kmem_cache_free_##class_name(void *pointer)\
{\
	printk(KERN_DEBUG "secondfs: Start kmem_cache_freeing, pointer=%p", pointer);\
	kmem_cache_free(kmem_cachep, pointer);\
}

#else // DO_MEMDBG

#define SECONDFS_GEN_C_HELPER_KMEM_CACHE_ALLOC_N_FREE(class_name, kmem_cachep) \
void *secondfs_c_helper_kmem_cache_alloc_##class_name(size_t size)\
{\
	void *p;\
	BUG_ON(size != kmem_cachep->object_size);\
	kmem_cache_malloc_num++;\
	p = kmem_cache_alloc(kmem_cachep, GFP_KERNEL);\
	printk(KERN_DEBUG "secondfs: Start kmem_cache_allocing, size=%u pointer=%p", kmem_cachep->object_size, p);\
	if (p == NULL)\
		printk(KERN_ERR "secondfs: Unable to allocate memory");\
	return p;\
}\
\
void secondfs_c_helper_kmem_cache_free_##class_name(void *pointer)\
{\
	kmem_cache_free_num++;\
	printk(KERN_DEBUG "secondfs: Start kmem_cache_freeing, pointer=%p", pointer);\
	kmem_cache_free(kmem_cachep, pointer);\
}

#endif // DO_MEMDBG

void secondfs_c_helper_kmem_cache_mdebug(void)
{
#ifdef DO_MEMDBG
	printk(KERN_DEBUG "secondfs: kmem_cache_malloc_num is %u", kmem_cache_malloc_num);
	printk(KERN_DEBUG "secondfs: kmem_cache_free_num is %u", kmem_cache_free_num);
#endif // DO_MEMDBG
}

SECONDFS_GEN_C_HELPER_KMEM_CACHE_ALLOC_N_FREE(DiskInode, secondfs_diskinode_cachep)

// 以下为 C 为 C++ 提供的 Linux 内核服务

int secondfs_c_helper_printk(const char *s)
{
	return printk(s);
}

unsigned long secondfs_c_helper_ktime_get_real_seconds()
{
	return ktime_get_real_seconds();
}

void secondfs_c_helper_spin_lock_init(void *lockp)
{
	spin_lock_init((spinlock_t *)lockp);
}

void secondfs_c_helper_spin_lock(void *lockp)
{
	spin_lock((spinlock_t *)lockp);
}

void secondfs_c_helper_spin_unlock(void *lockp)
{
	spin_unlock((spinlock_t *)lockp);
}

int secondfs_c_helper_spin_is_locked(void *lockp)
{
	return spin_is_locked((spinlock_t *)lockp);
}

void secondfs_c_helper_sema_init(void *semap, int val)
{
	sema_init((struct semaphore *)semap, val);
}

void secondfs_c_helper_up(void *semap)
{
	up((struct semaphore *)semap);
}

void secondfs_c_helper_down(void *semap)
{
	down((struct semaphore *)semap);
}

int secondfs_c_helper_down_trylock(void *semap)
{
	return down_trylock((struct semaphore *)semap);
}

void secondfs_c_helper_mutex_init(void *mutexp)
{
	mutex_init((struct mutex *)mutexp);
}

void secondfs_c_helper_mutex_lock(void *mutexp)
{
	mutex_lock((struct mutex *)mutexp);
}

void secondfs_c_helper_mutex_unlock(void *mutexp)
{
	mutex_unlock((struct mutex *)mutexp);
}

int secondfs_c_helper_mutex_is_locked(void *mutexp)
{
	return mutex_is_locked((struct mutex *)mutexp);
}

int secondfs_c_helper_mutex_trylock(void *mutexp)
{
	return mutex_trylock((struct mutex *)mutexp);
}

unsigned long secondfs_c_helper_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return copy_to_user(to, from, n);
}

unsigned long secondfs_c_helper_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return copy_from_user(to, from, n);
}

void secondfs_c_helper_bug()
{
	pr_err("BUG() called in C Helper");
	BUG();
}

struct inode *secondfs_c_helper_ilookup_without_iget(void *sb, unsigned long ino)
{
	struct inode *ret = ilookup((struct super_block *)sb, ino);
	if (ret)
		iput(ret);
	return ret;
}

void secondfs_c_helper_iput(void *inode)
{
	iput((struct inode *)inode);
}

u32 secondfs_c_helper_cpu_to_le32(u32 x)
{
	return cpu_to_le32(x);
}
u16 secondfs_c_helper_cpu_to_le16(u16 x)
{
	return cpu_to_le16(x);
}
u32 secondfs_c_helper_le32_to_cpu(u32 x)
{
	return le32_to_cpu(x);
}
u16 secondfs_c_helper_le16_to_cpu(u16 x)
{
	return le16_to_cpu(x);
}

void secondfs_c_helper_set_loff_t(void *x, u32 val)
{
	*(loff_t *)x = val;
}