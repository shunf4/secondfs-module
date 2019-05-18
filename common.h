#ifndef __COMMON_H__
#define __COMMON_H__

/* This file is a common header for C & C++ sources. */
/* 此文件是 C 和 C++ 源公用的头文件 */

// Define kernel typedefs(u32 etc.) for C++ sources
// 将 u32 等内核代码中对基础数据类型的简写引入 C++

#ifndef __cplusplus
#include <linux/types.h>
#endif // __cplusplus

#ifdef __cplusplus
#include <cstdint>
#include <asm-generic/errno-base.h>
#include <new>
typedef uint32_t u32;
typedef int32_t s32;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;
#endif // __cplusplus

// For intellisense in Visual Studio Code.
// These macros are added by -D options (please refer to Makefile) in fact.
// 这几个宏是 Makefile 加进去的, 放在这里只是为了让 VSCode
// 不划线线

#ifdef __IN_VSCODE__
#define SECONDFS_SEMAPHORE_SIZE 24
#define SECONDFS_SPINLOCK_T_SIZE 4
#define SECONDFS_MUTEX_SIZE 32
#define SECONDFS_INODE_SIZE 600
#endif // __IN_VSCODE__

// Some shorthand macros
// 一些快捷定义
#define SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(classname) \
const u32 SECONDFS_SIZEOF_##classname = sizeof(classname);\
classname *new##classname() {\
	return new(std::nothrow) classname();\
}\
void delete##classname(classname *p) {\
	try {\
		delete p;\
	} catch (...) {\
		secondfs_warning("deleteing a " #classname " %p: error!", p);\
	}\
}

#define SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR_DECLARATION(classname)\
extern const u32 SECONDFS_SIZEOF_##classname;\
classname *new##classname (void);\
void delete##classname (classname *);\

// C helper functions for C++
// In C sources files, this header is used for declaration.
// Then the functions are implemented.
// In C++ sources files, the functions are only declared as
// external symbols.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define SECONDFS_TYPE_DISKINODE 1

#ifdef __cplusplus
#define cpu_to_le32 secondfs_c_helper_cpu_to_le32
#define cpu_to_le16 secondfs_c_helper_cpu_to_le16
#define le32_to_cpu secondfs_c_helper_le32_to_cpu
#define le16_to_cpu secondfs_c_helper_le16_to_cpu
#endif // __cplusplus

void *secondfs_c_helper_malloc(size_t size);
void secondfs_c_helper_free(void *pointer);
void secondfs_c_helper_mdebug(void);

unsigned long secondfs_c_helper_ktime_get_real_seconds(void);
void secondfs_c_helper_spin_lock_init(void *lockp);
void secondfs_c_helper_spin_lock(void *lockp);
void secondfs_c_helper_spin_unlock(void *lockp);
int secondfs_c_helper_spin_is_locked(void *lockp);
void secondfs_c_helper_sema_init(void *semap, int val);
void secondfs_c_helper_up(void *semap);
void secondfs_c_helper_down(void *semap);
int secondfs_c_helper_down_trylock(void *semap);
void secondfs_c_helper_mutex_init(void *mutexp);
void secondfs_c_helper_mutex_lock(void *mutexp);
void secondfs_c_helper_mutex_unlock(void *mutexp);
int secondfs_c_helper_mutex_is_locked(void *mutexp);
int secondfs_c_helper_mutex_trylock(void *mutexp);
unsigned long secondfs_c_helper_copy_to_user(void 
#ifndef __cplusplus
__user
#endif
*to, const void *from, unsigned long n);
unsigned long secondfs_c_helper_copy_from_user(void *to, const void
#ifndef __cplusplus
__user
#endif
*from, unsigned long n);
void secondfs_c_helper_bug(void);
struct inode *secondfs_c_helper_ilookup_without_iget(void *sb, unsigned long ino);
void secondfs_c_helper_iput(void *inode);

u32 secondfs_c_helper_cpu_to_le32(u32 x);
u16 secondfs_c_helper_cpu_to_le16(u16 x);
u32 secondfs_c_helper_le32_to_cpu(u32 x);
u16 secondfs_c_helper_le16_to_cpu(u16 x);

void secondfs_c_helper_set_loff_t(void *x, u32 val);

int secondfs_c_helper_printk(const char *s, ...);

#ifdef __cplusplus
}
#endif // __cplusplus


// Debug flags and macros

#define SFDBG_SB_SIZECONSISTENCY 0x00000001
#define SFDBG_SB_FILL 0x00000002
#define SFDBG_BUFFER 0x00000004
#define SFDBG_BUFFERQ 0x00000008

#define SFDBG_MASK 0xFFFFFFFF
// #define SECONDFS_DEBUG (0)

#define SFDBG_ENA(flg) (SFDBG_##flg & SFDBG_MASK)

#ifdef __cplusplus
#include <linux/kern_levels.h>
#define secondfs_dbg(flg, fmt, ...)\
do {\
	if (SFDBG_##flg & SFDBG_MASK)\
		secondfs_c_helper_printk(KERN_DEBUG "secondfs: " fmt, ##__VA_ARGS__);\
} while (0)
#define secondfs_info(fmt, ...) secondfs_c_helper_printk(KERN_INFO "secondfs: " fmt, ##__VA_ARGS__);
#define secondfs_notice(fmt, ...) secondfs_c_helper_printk(KERN_NOTICE "secondfs: " fmt, ##__VA_ARGS__);
#define secondfs_warning(fmt, ...) secondfs_c_helper_printk(KERN_WARNING "secondfs: " fmt, ##__VA_ARGS__);
#define secondfs_err(fmt, ...) secondfs_c_helper_printk(KERN_ERR "secondfs: " fmt, ##__VA_ARGS__);
#define secondfs_crit(fmt, ...) secondfs_c_helper_printk(KERN_CRIT "secondfs: " fmt, ##__VA_ARGS__);
#define secondfs_alert(fmt, ...) secondfs_c_helper_printk(KERN_ALERT "secondfs: " fmt, ##__VA_ARGS__);
#define secondfs_emerg(fmt, ...) secondfs_c_helper_printk(KERN_EMERG "secondfs: " fmt, ##__VA_ARGS__);
#else // __cplusplus
#define secondfs_dbg(flg, fmt, ...)\
do {\
	if (SFDBG_##flg & SFDBG_MASK)\
		printk(KERN_DEBUG "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);\
} while (0)
#define secondfs_info(fmt, ...) printk(KERN_INFO "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#define secondfs_notice(fmt, ...) printk(KERN_NOTICE "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#define secondfs_warning(fmt, ...) printk(KERN_WARNING "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#define secondfs_err(fmt, ...) printk(KERN_ERR "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#define secondfs_crit(fmt, ...) printk(KERN_CRIT "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#define secondfs_alert(fmt, ...) printk(KERN_ALERT "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#define secondfs_emerg(fmt, ...) printk(KERN_EMERG "secondfs: " pr_fmt(fmt), ##__VA_ARGS__);
#endif // __cplusplus


#endif // __COMMON_H__