#include "secondfs.h"
// Linux requires sizeof(pointer) == sizeof(long). See:
// https://lkml.org/lkml/2009/10/13/122
// https://yarchive.net/comp/linux/int_types.html
typedef long intptr_t;

/* secondfs_inode_conform_v2s : 使 Inode 与 vfs inode 一致 (vfs->secondfs)
 */
void secondfs_inode_conform_v2s(Inode *si, struct inode *inode)
{
	const int buflen = 1000;
	char buf[buflen];

	int length = 0;

	secondfs_dbg(INODE, "Conform vfs->secondfs: %p->%p", inode, si);

	// TODO: 是否能假设 Inode 一定是 IALLOC 的?
	si->i_mode &= ~SECONDFS_IFMT;
	if (S_ISDIR(inode->i_mode)) {
		si->i_mode |= SECONDFS_IFDIR;
		length += sprintf(buf + length, "%s", "DIR|");
	} else if (S_ISCHR(inode->i_mode)) {
		si->i_mode |= SECONDFS_IFCHR;
		length += sprintf(buf + length, "%s", "CHR|");
	} else if (S_ISBLK(inode->i_mode)) {
		si->i_mode |= SECONDFS_IFBLK;
		length += sprintf(buf + length, "%s", "BLK|");
	} else {
		length += sprintf(buf + length, "%s", "REG|");
	}

	

	si->i_mode &= ~SECONDFS_ISUID;
	if (inode->i_mode & S_ISUID) {
		si->i_mode |= SECONDFS_ISUID;
		length += sprintf(buf + length, "%s", "ISUID|");
	}

	si->i_mode &= ~SECONDFS_ISGID;
	if (inode->i_mode & S_ISGID) {
		si->i_mode |= SECONDFS_ISGID;
		length += sprintf(buf + length, "%s", "ISGID|");
	}

	si->i_mode &= ~SECONDFS_ISVTX;
	if (inode->i_mode & S_ISVTX) {
		si->i_mode |= SECONDFS_ISVTX;
		length += sprintf(buf + length, "%s", "ISVTX|");
	}

	si->i_mode &= ~SECONDFS_IRWXU;
	si->i_mode &= ~SECONDFS_IRWXG;
	si->i_mode &= ~SECONDFS_IRWXO;
	if (inode->i_mode & S_IRUSR) 
		si->i_mode |= SECONDFS_IREAD;
	if (inode->i_mode & S_IWUSR) 
		si->i_mode |= SECONDFS_IWRITE;
	if (inode->i_mode & S_IXUSR) 
		si->i_mode |= SECONDFS_IEXEC;
	if (inode->i_mode & S_IRGRP) 
		si->i_mode |= SECONDFS_IREAD >> 3;
	if (inode->i_mode & S_IWGRP) 
		si->i_mode |= SECONDFS_IWRITE >> 3;
	if (inode->i_mode & S_IXGRP) 
		si->i_mode |= SECONDFS_IEXEC >> 3;
	if (inode->i_mode & S_IROTH) 
		si->i_mode |= SECONDFS_IREAD >> 6;
	if (inode->i_mode & S_IWOTH) 
		si->i_mode |= SECONDFS_IWRITE >> 6;
	if (inode->i_mode & S_IXOTH) 
		si->i_mode |= SECONDFS_IEXEC >> 6;

	length += sprintf(buf + length, "%ou", si->i_mode & (SECONDFS_IRWXU | SECONDFS_IRWXG | SECONDFS_IRWXO));

	si->i_uid = i_uid_read(inode);
	length += sprintf(buf + length, ", UID:%d", si->i_uid);
	si->i_gid = i_gid_read(inode);
	length += sprintf(buf + length, ", GID:%d", si->i_gid);
	si->i_nlink = inode->i_nlink;
	length += sprintf(buf + length, ", nlink:%d", si->i_nlink);
	si->i_size = inode->i_size;
	length += sprintf(buf + length, ", size:%d", si->i_size);
	si->i_atime = inode->i_atime.tv_sec;
	length += sprintf(buf + length, ", atime:%d", si->i_atime);
	si->i_mtime = inode->i_mtime.tv_sec;
	length += sprintf(buf + length, ", mtime:%d", si->i_mtime);

	secondfs_dbg(INODE, "%s", buf);

	// 在 dirty_inode 里做过了
	/* if (is_inode_dirty) {
		si->i_flag |= SECONDFS_IUPD;
	} */
}

/* secondfs_inode_conform_s2v : 使 struct inode 与 Inode 一致 (secondfs->vfs)
 */
void secondfs_inode_conform_s2v(struct inode *inode, Inode *si)
{
	const int buflen = 1000;
	char buf[buflen];

	int length = 0;
	
	secondfs_dbg(INODE, "Conform secondfs->vfs: %p->%p", si, inode);

	//inode->i_mode = si->i_mode;
	inode->i_mode &= ~S_IFMT;
	if ((si->i_mode & SECONDFS_IFMT) == SECONDFS_IFDIR) {
		inode->i_mode |= S_IFDIR;
		length += sprintf(buf + length, "%s", "DIR|");
	} else if ((si->i_mode & SECONDFS_IFMT) == SECONDFS_IFCHR) {
		inode->i_mode |= S_IFCHR;
		length += sprintf(buf + length, "%s", "CHR|");
	} else if ((si->i_mode & SECONDFS_IFMT) == SECONDFS_IFBLK) {
		inode->i_mode |= S_IFBLK;
		length += sprintf(buf + length, "%s", "BLK|");
	} else {
		length += sprintf(buf + length, "%s", "REG|");
	}

	inode->i_mode &= ~S_ISUID;
	if (si->i_mode & SECONDFS_ISUID) {
		inode->i_mode |= S_ISUID;
		length += sprintf(buf + length, "%s", "ISUID|");
	}

	inode->i_mode &= ~S_ISGID;
	if (si->i_mode & SECONDFS_ISGID) {
		inode->i_mode |= S_ISGID;
		length += sprintf(buf + length, "%s", "ISGID|");
	}

	inode->i_mode &= ~S_ISVTX;
	if (si->i_mode & SECONDFS_ISVTX) {
		inode->i_mode |= S_ISVTX;
		length += sprintf(buf + length, "%s", "ISVTX|");
	}

	inode->i_mode &= ~S_IRWXU;
	inode->i_mode &= ~S_IRWXG;
	inode->i_mode &= ~S_IRWXO;

	if (si->i_mode & SECONDFS_IREAD) 
		inode->i_mode |= S_IRUSR;
	if (si->i_mode & SECONDFS_IWRITE) 
		inode->i_mode |= S_IWUSR;
	if (si->i_mode & SECONDFS_IEXEC) 
		inode->i_mode |= S_IXUSR;
	if (si->i_mode & SECONDFS_IREAD >> 3) 
		inode->i_mode |= S_IRGRP;
	if (si->i_mode & SECONDFS_IWRITE >> 3) 
		inode->i_mode |= S_IWGRP;
	if (si->i_mode & SECONDFS_IEXEC >> 3) 
		inode->i_mode |= S_IXGRP;
	if (si->i_mode & SECONDFS_IREAD >> 6) 
		inode->i_mode |= S_IROTH;
	if (si->i_mode & SECONDFS_IWRITE >> 6) 
		inode->i_mode |= S_IWOTH;
	if (si->i_mode & SECONDFS_IEXEC >> 6) 
		inode->i_mode |= S_IXOTH;

	length += sprintf(buf + length, "%ou", si->i_mode & (SECONDFS_IRWXU | SECONDFS_IRWXG | SECONDFS_IRWXO));

	i_uid_write(inode, si->i_uid);
	length += sprintf(buf + length, ", UID:%d", si->i_uid);
	i_gid_write(inode, si->i_gid);
	length += sprintf(buf + length, ", GID:%d", si->i_gid);
	set_nlink(inode, si->i_nlink);
	length += sprintf(buf + length, ", nlink:%d", si->i_nlink);
	inode->i_size = si->i_size;
	length += sprintf(buf + length, ", size:%d", si->i_size);
	inode->i_atime.tv_sec = si->i_atime;
	length += sprintf(buf + length, ", atime:%d", si->i_atime);
	inode->i_mtime.tv_sec = si->i_mtime;
	length += sprintf(buf + length, ", mtime:%d", si->i_mtime);

	if (si->i_flag & SECONDFS_IUPD) {
		mark_inode_dirty_sync(inode);
	}

	secondfs_dbg(INODE, "%s", buf);
}

/* secondfs_write_inode : 写回 Inode.
			Write Inode back to disk.
 *      inode : 系统传过来的 VFS Inode 指针
 *      wbc : 指示我们应该同步还是异步写, 我们只实现同步写 Not used - sync write only
 *
 * Calls Inode::IUpdate()
 * 调用 Inode::IUpdate()
 */
int secondfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	Inode *si = SECONDFS_INODE(inode);
	int ret;

	si->i_flag |= SECONDFS_IUPD;

	secondfs_dbg(INODE, "write_inode(%p, %d)...", si->i_ssb, si->i_number);

	// Before that: Sync Inode according to inode
	// 在此之前: 将 VFS Inode 的状态根据 inode 进行同步
	secondfs_inode_conform_v2s(si, inode);

	// IUpdate 已经修改, 不会更新时间
	secondfs_dbg(INODE, "write_inode(%p, %d), IUpdate...", si->i_ssb, si->i_number);
	ret = Inode_IUpdate(si, ktime_get_real_seconds());
	return ret;
}

/* secondfs_evict_inode : 当 Inode 丧失最后一个引用计数时,
                        释放内存 Inode.
			Release Inode when it lose the last
			refcount.
 *      inode : 系统传过来的 VFS Inode 指针
 *
 * Written with reference to InodeTable::IPut()
 * 参考 InodeTable::IPut() 在 if 分支内的代码编写
 */
void secondfs_evict_inode(struct inode *inode)
{
	Inode *pNode = SECONDFS_INODE(inode);
	int want_delete = 0;
	int ret;

	// inode->pNode synchronization
	// 先让 Inode 与 VFS Inode 同步
	secondfs_inode_conform_v2s(pNode, inode);

	secondfs_dbg(INODE, "evict_inode(%p, %d)...", pNode->i_ssb, pNode->i_number);

	// We do not use linux page cache, so 
	// truncate_inode_pages_final will do nothing
	// good here.
	// 把与这个 VFS Inode 关联的文件页全部释放 ?
	// 由于我们不使用 address_space 和页缓存机制,
	// 这里没有必要
	// truncate_inode_pages_final(&inode->i_data);

	/* When refcount falls to 0, delete(unlink) it */
	/* 该文件已经没有目录路径指向它, 删除它 */
	if(inode->i_nlink <= 0)
	{
		secondfs_dbg(INODE, "evict_inode(%p, %d): want_delete", pNode->i_ssb, pNode->i_number);
		want_delete = 1;
		/* Truncate all the data blocks */
		/* 释放该文件占据的数据盘块 */
		secondfs_dbg(INODE, "evict_inode(%p, %d): Inode::ITrunc()...", pNode->i_ssb, pNode->i_number);
		ret = Inode_ITrunc(pNode);
		if (IS_ERR_VALUE((intptr_t)ret)) {
			secondfs_err("evict_inode(%p,%d) Inode::ITrunc() error %d!", pNode->i_ssb, pNode->i_number, ret);
			return;
		}
		pNode->i_mode = 0;
		/* The Inode will be released later */
		/* 稍后会释放对应的外存Inode */
	}

	/* 更新外存Inode信息 */
	// IUpdate 已经修改, 不会更新时间
	ret = Inode_IUpdate(pNode, ktime_get_real_seconds());
	if (IS_ERR_VALUE((intptr_t)ret)) {
		secondfs_err("evict_inode(%p,%d) Inode::IUpdate() error %d!", pNode->i_ssb, pNode->i_number, ret);
		return;
	}

	/* 解锁内存Inode，并且唤醒等待进程 */
	// 不解锁了, 暂不使用 Inode 的锁机制
	//pNode->Prele();
	/* 清除内存Inode的所有标志位 */
	pNode->i_flag = 0;

	/* 这是内存inode空闲的标志之一，另一个是i_count == 0 */
	// i_count is not used in this module
	pNode->i_number = -1;

	// System clean actions to VFS inode
	// 执行 VFS Inode 的清理动作
	invalidate_inode_buffers(inode);
	clear_inode(inode);

	// if want_delete, we should free it to inode table here
	// but not earlier.
	// See: linux/fs/ext2/inode.c : ext2_evict_inode
	// & linux/fs/ext2/ialloc.c : ext2_free_inode 
	// 若是要删除的, 此时才能在磁盘 Inode 表中释放它. 具体原因
	// 见 linux/fs/ext2/inode.c : ext2_evict_inode
	if (want_delete) {
		secondfs_dbg(INODE, "evict_inode(%p, %d): Inode::IFree()...", pNode->i_ssb, pNode->i_number);
		FileSystem_IFree(secondfs_filesystemp, pNode->i_ssb, pNode->i_number);
	}
	// TODO: 在目录项中删除(真的需要吗?)
}

struct inode *secondfs_new_inode(struct inode *dir, umode_t mode,
				const struct qstr *str)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	Inode *si;
	SuperBlock *secsb;

	secsb = SECONDFS_SB(sb);

	secondfs_dbg(INODE, "new_inode(%p,%u,%s)...", dir, mode, str->name);
	
	// Allocate Inode in memory/fs
	// 先在内存 & 文件系统分配 Inode
	si = FileSystem_IAlloc(secondfs_filesystemp, secsb);
	if (si == NULL) {
		secondfs_dbg(INODE, "new_inode(%p,%u,%s): IAlloc fail!", dir, mode, str->name);
		return ERR_PTR(-ENOMEM);
	}

	inode = &si->vfs_inode;
	secondfs_inode_conform_s2v(inode, si);

#ifdef SECONDFS_KERNEL_BEFORE_4_9
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
#else
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
#endif
	
	inode_init_owner(inode, dir, mode);
	mark_inode_dirty(inode);
	secondfs_inode_conform_v2s(si, inode);
	
	return inode;
}

void secondfs_dirty_inode(struct inode *inode, int flags)
{
	SECONDFS_INODE(inode)->i_flag |= SECONDFS_IUPD;
}