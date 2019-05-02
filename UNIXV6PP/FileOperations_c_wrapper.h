#ifndef __FILEOPERATIONS_C_WRAPPER_H__
#define __FILEOPERATIONS_C_WRAPPER_H__

#include "Common.hh"
#include "../common_c_cpp_types.h"

#ifndef __cplusplus

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// FileManager 类的 C 包装
#ifndef __cplusplus
typedef struct _FileManager
{
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
	SECONDFS_DELETE		/* 以删除文件方式搜索目录 */
;

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR_DECLARATION(FileManager)

u32 FileManager_Read(FileManager *fm, u8
#ifndef __cplusplus
__user
#endif
 *buf, size_t len, u32 *ppos, Inode *inode);
u32 FileManager_Write(FileManager *fm, u8
#ifndef __cplusplus
__user
#endif
*buf, size_t len, u32 *ppos, Inode *inode);
u32 FileManager_Rdwr(FileManager *fm, u8
#ifndef __cplusplus
__user
#endif
*buf, size_t len, u32 *ppos, IOParameter *io_paramp, Inode *inode, u32 mode);



// DirectoryEntry 类的 C 包装

#define SECONDFS_DIRSIZ 28	/* 目录项中路径部分的最大字符串长度 */

#ifndef __cplusplus
typedef struct _DirectoryEntry{
	u32 m_ino;		/* 目录项中Inode编号部分 */
	u8 m_name[SECONDFS_DIRSIZ];	/* 目录项中路径名部分 */
} DirectoryEntry;
#else // __cplusplus
class DirectoryEntry;
#endif // __cplusplus

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR_DECLARATION(DirectoryEntry)


// IOParameter 类的 C 包装
#ifndef __cplusplus
typedef struct _IOParameter{
	u8* m_Base;	/* 当前读、写用户目标区域的首地址 */
	s32 m_Offset;	/* 当前读、写文件的字节偏移量 */
	s32 m_Count;	/* 当前还剩余的读、写字节数量 */
} IOParameter;
#else // __cplusplus
class IOParameter;
#endif // __cplusplus

SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR_DECLARATION(IOParameter)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __FILEOPERATIONS_C_WRAPPER_H__