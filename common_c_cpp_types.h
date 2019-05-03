#ifndef __COMMON_C_CPP_TYPES_H__
#define __COMMON_C_CPP_TYPES_H__

#ifndef __cplusplus
#include <linux/types.h>
#endif // __cplusplus

#ifdef __cplusplus
#include <cstdint>
typedef uint32_t u32;
typedef int32_t s32;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;
#endif // __cplusplus

#ifdef __IN_VSCODE__
#define SECONDFS_SEMAPHORE_SIZE 24
#define SECONDFS_SPINLOCK_T_SIZE 4
#define SECONDFS_MUTEX_SIZE 32
#define SECONDFS_INODE_SIZE 600
#endif // __IN_VSCODE__

#endif // __COMMON_C_CPP_TYPES_H__