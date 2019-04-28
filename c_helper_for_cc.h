#ifndef __C_HELPER_FOR_CC_H__
#define __C_HELPER_FOR_CC_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define SECONDFS_TYPE_DISKINODE 1

void *secondfs_c_helper_malloc(size_t size);
void secondfs_c_helper_free(void *pointer);
void secondfs_c_helper_mdebug(void);

unsigned long secondfs_c_helper_get_seconds(void);
void secondfs_c_helper_spin_lock_init(void *lockp);
void secondfs_c_helper_spin_lock(void *lockp);
void secondfs_c_helper_spin_unlock(void *lockp);
void secondfs_c_helper_sema_init(void *semap, int val);
void secondfs_c_helper_up(void *semap);
void secondfs_c_helper_down(void *semap);
int secondfs_c_helper_down_trylock(void *semap);
void secondfs_c_helper_mutex_init(void *mutexp);
void secondfs_c_helper_mutex_lock(void *mutexp);
void secondfs_c_helper_mutex_unlock(void *mutexp);
int secondfs_c_helper_mutex_is_locked(void *mutexp);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __C_HELPER_FOR_CC_H__
