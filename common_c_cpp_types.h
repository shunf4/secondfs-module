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
#endif // __cplusplus

#endif // __COMMON_C_CPP_TYPES_H__