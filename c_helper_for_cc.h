#ifndef __C_HELPER_FOR_CC_H__
#define __C_HELPER_FOR_CC_H__

#include "common_c_cpp_types.h"
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

int secondfs_c_helper_printk(const char *s);
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

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __C_HELPER_FOR_CC_H__
