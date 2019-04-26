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

#define SECONDFS_GEN_C_HELPER_HEADER_KMEM_CACHE_ALLOC_N_FREE(class_name) \
void *secondfs_c_helper_kmem_cache_alloc_##class_name(size_t size);\
void secondfs_c_helper_kmem_cache_free_##class_name(void *pointer);

SECONDFS_GEN_C_HELPER_HEADER_KMEM_CACHE_ALLOC_N_FREE(DiskInode)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __C_HELPER_FOR_CC_H__
