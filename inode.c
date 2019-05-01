#include "secondfs.h"

/* secondfs_conform : 使 Inode 与 struct inode 一致
 */
static void secondfs_conform(Inode *si, struct inode *inode)
{
	si->i_mode = inode->i_mode;
	si->i_uid = i_uid_read(inode);
	si->i_gid = i_gid_read(inode);
	si->i_nlink = inode->i_nlink;
	si->i_size = inode->i_size;
	si->i_atime = inode->i_atime.tv_sec;
	si->i_mtime = inode->i_mtime.tv_sec;
}

/* secondfs_write_inode : 写回 Inode.
 *      inode : 系统传过来的 VFS Inode 指针
 *      wbc : 指示我们应该同步还是异步写, 我们只实现同步写
 *
 * 调用 Inode::IUpdate()
 */
int secondfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	Inode *si = SECONDFS_INODE(inode);
	si->i_flag |= SECONDFS_IUPD;

	// 在此之前: 将 VFS Inode 的状态同步回 Inode
	secondfs_conform(si, inode);

	Inode_IUpdate(si, ktime_get_real_seconds());
}

/* secondfs_evict_inode : 当 Inode 丧失最后一个引用计数时,
                        释放内存 Inode.
 *      inode : 系统传过来的 VFS Inode 指针
 *
 * 参考 InodeTable::IPut() 在 if 分支内的代码编写
 */
void secondfs_evict_inode(struct inode *inode)
{
	Inode *pNode = SECONDFS_INODE(inode);
	int want_delete = 0;

	// 先让 Inode 与 VFS Inode 同步
	secondfs_conform(pNode, inode);
	// 把与这个 VFS Inode 关联的文件页全部释放
	truncate_inode_pages_final(&inode->i_data);

	/* 该文件已经没有目录路径指向它, 删除它 */
	if(inode->i_nlink <= 0)
	{
		want_delete = 1;
		/* 释放该文件占据的数据盘块 */
		Inode_ITrunc(pNode);
		pNode->i_mode = 0;
		/* 稍后会释放对应的外存Inode */
	}

	/* 更新外存Inode信息 */
	Inode_IUpdate(pNode, ktime_get_real_seconds());

	/* 解锁内存Inode，并且唤醒等待进程 */
	// 不解锁了, 暂不使用 Inode 的锁机制
	//pNode->Prele();
	/* 清除内存Inode的所有标志位 */
	pNode->i_flag = 0;
	/* 这是内存inode空闲的标志之一，另一个是i_count == 0 */
	pNode->i_number = -1;

	// 执行 VFS Inode 的清理动作
	invalidate_inode_buffers(inode);
	clear_inode(inode);

	// 若是要删除的, 此时再在磁盘 Inode 表中释放它. 具体原因
	// 见 linux/fs/ext2/inode.c : ext2_evict_inode
	if (want_delete) {
		FileSystem_IFree(secondfs_filesystemp, pNode->i_ssb, pNode->i_number);
	}
}