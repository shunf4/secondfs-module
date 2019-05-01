#include "secondfs.h"

/* secondfs_write_inode : 写回 Inode.
 *      inode : 系统传过来的 VFS Inode 指针
 *      wbc : 指示我们应该同步还是异步写, 我们只实现同步写
 *
 * 调用 Inode::IUpdate()
 */
int secondfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	Inode *si = SECONDFS_INODE(inode);
}
