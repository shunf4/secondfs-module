#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void *secondfs_c_helper_malloc(unsigned int size);
void secondfs_c_helper_free(void *pointer);
void secondfs_c_helper_mdebug(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __UTILS_H__
