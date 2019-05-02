#include "secondfs.h"

/* secondfs_conform_v2s : 使 Inode 与 struct inode 一致
 */
static void secondfs_conform_v2s(Inode *si, struct inode *inode)
{
	// TODO: 是否能假设 Inode 一定是 IALLOC 的?
	si->i_mode &= ~SECONDFS_IFMT;
	if (S_ISDIR(inode->i_mode)) {
		si->i_mode |= SECONDFS_IFDIR;
	} else if (S_ISCHR(inode->i_mode)) {
		si->i_mode |= SECONDFS_IFCHR;
	} else if (S_ISBLK(inode->i_mode)) {
		si->i_mode |= SECONDFS_IFBLK;
	}

	si->i_mode &= ~SECONDFS_ISUID;
	if (inode->i_mode & S_ISUID) {
		si->i_mode |= SECONDFS_ISUID;
	}

	si->i_mode &= ~SECONDFS_ISGID;
	if (inode->i_mode & S_ISGID) {
		si->i_mode |= SECONDFS_ISGID;
	}

	si->i_mode &= ~SECONDFS_ISVTX;
	if (inode->i_mode & S_ISVTX) {
		si->i_mode |= SECONDFS_ISVTX;
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

	si->i_uid = i_uid_read(inode);
	si->i_gid = i_gid_read(inode);
	si->i_nlink = inode->i_nlink;
	si->i_size = inode->i_size;
	si->i_atime = inode->i_atime.tv_sec;
	si->i_mtime = inode->i_mtime.tv_sec;
}

/* secondfs_conform_s2v : 使 struct inode 与 Inode 一致
 */
static void secondfs_conform_s2v(struct inode *inode, Inode *si)
{
	//inode->i_mode = si->i_mode;
	inode->i_mode &= ~S_IFMT;
	if (si->i_mode & SECONDFS_IFMT == SECONDFS_IFDIR) {
		inode->i_mode |= S_IFDIR;
	} else if (si->i_mode & SECONDFS_IFMT == SECONDFS_IFCHR) {
		inode->i_mode |= S_IFCHR;
	} else if (si->i_mode & SECONDFS_IFMT == SECONDFS_IFBLK) {
		inode->i_mode |= S_IFBLK;
	}

	inode->i_mode &= ~S_ISUID;
	if (si->i_mode & SECONDFS_ISUID) {
		inode->i_mode |= S_ISUID;
	}

	inode->i_mode &= ~S_ISGID;
	if (si->i_mode & SECONDFS_ISGID) {
		inode->i_mode |= S_ISGID;
	}

	inode->i_mode &= ~S_ISVTX;
	if (si->i_mode & SECONDFS_ISVTX) {
		inode->i_mode |= S_ISVTX;
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

	i_uid_write(inode, si->i_uid);
	i_gid_write(inode, si->i_gid);
	set_nlink(inode, si->i_nlink);
	inode->i_size = si->i_size;
	inode->i_atime.tv_sec = si->i_atime;
	inode->i_mtime.tv_sec = si->i_mtime;
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
	secondfs_conform_v2s(si, inode);

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
	secondfs_conform_v2s(pNode, inode);
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

struct inode *secondfs_new_inode(struct inode *dir, umode_t mode,
				const struct qstr *str)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	Inode *si;
	SuperBlock *secsb;
	DirectoryEntry de;
	IOParameter io_param;

	secsb = SECONDFS_SB(sb);

	si = FileSystem_IAlloc(secondfs_filesystemp, secsb);
	if (si == NULL)
		return ERR_PTR(-ENOMEM);	// TODO: 更严谨的错误号
	// 此处参考 FileManager::MakNode
	si->i_flag |= (SECONDFS_IACC | SECONDFS_IUPD);
	si->i_nlink = 1;

	// 此处参考 FileManager::WriteDir
	de.m_ino = inode->i_ino;
	memset(de.m_name, sizeof(de.m_name), 0);
	for (int i = 0; i < min(SECONDFS_DIRSIZ, str->len); i++) {
		de.m_name[i] = str->name[i];
	}
	// need namei
	io_param.
	Inode_WriteI(SECONDFS_INODE(dir), )

	inode = &si->vfs_inode;
	secondfs_conform_s2v(inode, si);

	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode_init_owner(inode, dir, mode);
	mark_inode_dirty(inode);
	secondfs_conform_v2s(si, inode);
	
	return inode;
}