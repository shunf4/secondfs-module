#ifndef __FILEOPERATIONS_C_WRAPPER_H__
#define __FILEOPERATIONS_C_WRAPPER_H__

#include "../common.h"

#include "Inode_c_wrapper.h"

#ifndef __cplusplus

#else // __cplusplus
#include <cstddef>
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


// FileManager 类的 C 包装
#ifndef __cplusplus
typedef struct _FileManager
{
	s32 dummy;
} FileManager;
#else // __cplusplus
class FileManager;
#endif // __cplusplus

extern const u32
	SECONDFS_FREAD,
	SECONDFS_FWRITE,
	SECONDFS_FPIPE
;

extern const u32
	SECONDFS_OPEN,		/* 以打开文件方式搜索目录 */
	SECONDFS_CREATE,	/* 以新建文件方式搜索目录 */
	SECONDFS_DELETE,	/* 以删除文件方式搜索目录 */
	SECONDFS_CHECKEMPTY,	/* 检测目录是否非空 */
	SECONDFS_LIST,		/* 遍历目录 */
	SECONDFS_OPEN_NOT_IGNORE_DOTS	/* 以打开文件方式搜索目录, 不要跳过 "." 和 ".." */
;

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR_DECLARATION(FileManager)

long FileManager_Read(FileManager *fm, u8
#ifndef __cplusplus
__user
#endif
 *buf, size_t len, u32 *ppos, Inode *inode);

long FileManager_Write(FileManager *fm, const u8
#ifndef __cplusplus
__user
#endif
*buf, size_t len, u32 *ppos, Inode *inode);

u32 FileManager_Rdwr(FileManager *fm, u8
#ifndef __cplusplus
__user
#endif
*buf, size_t len, u32 *ppos, IOParameter *io_paramp, Inode *inode, u32 mode);

int FileManager_DELocate(FileManager *fm, Inode *dir, const char *name,
		u32 namelen, u32 mode, IOParameter *out_iop, u32 *inop);



#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __FILEOPERATIONS_C_WRAPPER_H__