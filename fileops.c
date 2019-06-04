#include "secondfs.h"
#include <linux/fs.h>

ssize_t secondfs_file_read(struct file *filp, char __user *buf, size_t len,
				loff_t *ppos)
{
	// We use filp->f_path.dentry->d_inode to get the inode
	// rather than the cached filp->f_inode
	// filp->f_inode 是缓存数据, 实时数据在 path 结构里面
	struct inode *inode = filp->f_path.dentry->d_inode;
	Inode *si = SECONDFS_INODE(inode);
	ssize_t ret;
	u32 pos_u32 = *ppos;

	secondfs_dbg(FILE, "file_read(%p,%p,%lu): inode_lock()", filp, buf, len);

	inode_lock(inode);
	
	ret = FileManager_Read(secondfs_filemanagerp, buf, len, &pos_u32, si);
	*ppos = pos_u32;

	file_accessed(filp);

	secondfs_inode_conform_s2v(inode, si);
	mark_inode_dirty_sync(inode);

	inode_unlock(inode);
	return ret;
}

ssize_t secondfs_file_write(struct file *filp, const char __user *buf, size_t len,
				loff_t *ppos)
{
	// We use filp->f_path.dentry->d_inode to get the inode
	// rather than the cached filp->f_inode
	// filp->f_inode 是缓存数据, 实时数据在 path 结构里面
	struct inode *inode = filp->f_path.dentry->d_inode;
	Inode *si = SECONDFS_INODE(inode);
	ssize_t ret;
	u32 pos_u32 = *ppos;

	secondfs_dbg(FILE, "file_write(%.32s,%p,%lu)", filp->f_path.dentry->d_name.name, buf, len);

	secondfs_dbg(FILE, "file_write(%.32s,%p,%lu): inode_lock()", filp->f_path.dentry->d_name.name, buf, len);

	inode_lock(inode);

	secondfs_dbg(FILE, "file_write(%.32s,%p,%lu): update_time()", filp->f_path.dentry->d_name.name, buf, len);
	ret = file_update_time(filp);
	if (ret) {
		secondfs_err("file_write(%.32s,%p,%lu): update_time() failed!", filp->f_path.dentry->d_name.name, buf, len);
		goto out;
	}
	
	ret = FileManager_Write(secondfs_filemanagerp, buf, len, &pos_u32, si);
	*ppos = pos_u32;
	secondfs_inode_conform_s2v(inode, si);
	mark_inode_dirty_sync(inode);

out:
	inode_unlock(inode);
	return ret;
}

int secondfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	// This function is to sync current file to disk
	// However for convenience we sync the whole device(Bflush)
	// 要求将文件同步到磁盘上
	// 我们就简单地整设备同步即可
	secondfs_dbg(FILE, "fsync(%p)", file);

	BufferManager_Bflush(secondfs_buffermanagerp, SECONDFS_INODE(file->f_path.dentry->d_inode)->i_ssb->s_dev);
	return 0;
}

int secondfs_add_link(struct dentry *dentry, struct inode *inode)
{
	// This function is to add dentry to its parent's directory file
	// 要求在 dentry 的父目录项中添加 dentry 这一项, 指向 inode
	
	// Get its parent
	// 父目录
	struct inode *dir = d_inode(dentry->d_parent);

	// Get current dentry's filename
	// 本 dentry 的文件名
	const char *name = dentry->d_name.name;
	Inode *si = SECONDFS_INODE(inode);
	IOParameter io_param = {
		.m_Base = NULL,
		.isUserP = 0
	};
	u32 dummy_ino;
	int ret;
	DirectoryEntry de;
	int i;
	u32 minlen;

	// We locate the "hole"(vacancy) in directory file where we
	// can put the dentry in.
	// DELocate() is called in "CREATE" mode.
	// 首先需要在父目录项中定位可放入目录项的空位
	// 我们调用 DELocate, 它是 NameI 功能的一部分,
	// 可以在父目录项中定位

	secondfs_dbg(FILE, "add_link(): DELocate CREATE %.32s...", name);

	ret = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(dir), name, dentry->d_name.len,
			SECONDFS_CREATE, &io_param, &dummy_ino);
	if (ret != 0) {
		secondfs_dbg(FILE, "add_link(): DELocate failed (%d)", ret);
		return ret;
	}
	
	// With reference to FileManager::MakNode
	// 此处参考 FileManager::MakNode
	si->i_flag |= (SECONDFS_IACC | SECONDFS_IUPD);
	// si->i_nlink = 1;

	// With reference to FileManager::WriteDir
	// 此处参考 FileManager::WriteDir

	// Contruct new DE to fill in the vacancy
	// 在 DirectoryEntry 中填入新项目的信息
	de.m_ino = cpu_to_le32(inode->i_ino);
	memset(de.m_name, 0, sizeof(de.m_name));
	minlen = SECONDFS_DIRSIZ < dentry->d_name.len ? SECONDFS_DIRSIZ : dentry->d_name.len;
	for (i = 0; i < minlen; i++) {
		de.m_name[i] = dentry->d_name.name[i];
	}

	// In DELocate previously called, the offset of the vacancy
	// is set in io_param. We can just set the m_Base(buffer to output)
	// and m_Count(length to write) to write the DE.
	// 刚才的定位信息已经写进 io_param 了, 我们只要设定写数据
	// 地址和写长度就可以把项目写进目录项了
	io_param.m_Base = (u8 *)&de;
	io_param.m_Count = sizeof(de);
	secondfs_dbg(FILE, "add_link(): WriteI() <%d,%.32s>", de.m_ino, de.m_name);
	Inode_WriteI(SECONDFS_INODE(dir), &io_param);

	if (io_param.err != 0) {
		secondfs_err("add_link(): WriteI() failed (%d)", io_param.err);
		return io_param.err;
	}
	//conform parent
	secondfs_inode_conform_s2v(dir, SECONDFS_INODE(dir));

	return 0;
}

static inline int secondfs_add_nondir(struct dentry *dentry, struct inode *inode)
{
	// The more specific function: add a nondir dentry
	// to its parent's directory file
	// Additionally, the nondir dentry is newly allocated,
	// so we will unlock_new_inode and d_instantiate it.
	// 要求将本文件 dentry 以及 inode 登记到父目录项
	int err;

	secondfs_dbg(FILE, "add_nondir(): before add_link");
	err = secondfs_add_link(dentry, inode);
	secondfs_dbg(FILE, "add_nondir(): after add_link");

	if (err)
		goto err;
	// 创建 Inode 时, 都要用这个函数初始化 dentry
	unlock_new_inode(inode);
	d_instantiate(dentry, inode);
	return 0;

err:
	secondfs_err("add_nondir(): add_link() failed!");
	inode_dec_link_count(inode);
	secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);
	// discard_new_inode(inode);
	unlock_new_inode(inode);
	iput(inode);
	return err;
}

static int secondfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	// The function is to create a new file under dir.
	// the dentry of this new file is created in advance;
	// all we have to do is to allocate a new (VFS & Secondfs)
	// inode for it and add the dentry to its parent.
	// 要求在 dir 下为新创建文件(而不是文件夹)
	struct inode *inode;
	int ret;

	// 先为文件分配新 inode (内存中 & 文件系统中)
	secondfs_dbg(FILE, "create(%.32s): before new_inode", dentry->d_name.name);
	inode = secondfs_new_inode(dir, mode, &dentry->d_name);

	if (IS_ERR(inode)) {
		secondfs_err("create(%.32s): inode is ERR (%ld)", dentry->d_name.name, PTR_ERR(inode));
		return PTR_ERR(inode);
	}

	if (inode == NULL) {
		secondfs_err("create(%.32s): inode is NULL", dentry->d_name.name);
		return -ENOSPC;
	}

	// Add operation function table to it
	inode->i_op = &secondfs_file_inode_operations;
	inode->i_fop = &secondfs_file_operations;
	mark_inode_dirty(inode);

	// 具体在 dentry 里建立与 inode 的链接, 并把
	// 该文件登记到父目录项中
	// d_instantiate_new 在里面调用
	secondfs_dbg(FILE, "create(%.32s): add_nondir", dentry->d_name.name);
	ret = secondfs_add_nondir(dentry, inode);
	secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);
	return ret;
}


static struct dentry *secondfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
	// The function is to find DE in dir, and link the
	// associated inode to the dentry.
	// 要求在 dir 中找到 dentry
	struct inode *inode;
	u32 ino;
	int ret;
	IOParameter iop = {
		.isUserP = 0
	};

	if (dentry->d_name.len > SECONDFS_DIRSIZ) {
		secondfs_err("lookup(): -ENAMETOOLONG");
		return ERR_PTR(-ENAMETOOLONG);
	}

	// Call DELocate() in OPEN mode; the inode number is
	// stored in &ino
	// 定位, 找到这个文件的 inode 存入 ino
	secondfs_dbg(FILE, "lookup(): DELocate OPEN %.32s", dentry->d_name.name);
	ret = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(dir),
				dentry->d_name.name, dentry->d_name.len, 
				SECONDFS_OPEN, &iop, &ino);
	if (ret != 0 && ret != -ENOENT) {
		secondfs_err("lookup(): DELocate failed (%d)", ret);
		return ERR_PTR(ret);
	}

	if (ret != -ENOENT) {
		// Get this inode by inode number
		// 通过 ino 获取这个 inode
		secondfs_dbg(FILE, "lookup(): secondfs_iget(%d)", ino);
		inode = secondfs_iget(dir->i_sb, ino);
		if (IS_ERR_OR_NULL(inode)) {
			secondfs_err("lookup(): secondfs_iget failed (%ld)", PTR_ERR(inode));
			return ERR_PTR(-EIO);
		}
	} else {
		secondfs_dbg(FILE, "lookup(): DELocate did not find inode, link NULL to this entry");
		inode = NULL;
	}

	// Link the inode with the dentry
	// 将 inode 与 dentry 重新链接起来
	return d_splice_alias(inode, dentry);
}

static int secondfs_link(struct dentry *old_dentry, struct inode *dir,
			struct dentry *dentry)
{
	// The function is to add a link from dentry to the inode 
	// associated with old_dentry
	// 要求在 dir 下为 dentry 项目初始化一个硬链接, 指向 old_dentry
	// 的 inode
	struct inode *inode = d_inode(old_dentry);
	int err;

#ifdef SECONDFS_KERNEL_BEFORE_4_9
	inode->i_ctime = CURRENT_TIME_SEC;
#else
	inode->i_ctime = current_time(inode);
#endif

	secondfs_dbg(FILE, "link()...");

	// The inode has one more dentry pointing to it;
	// increase the linkcount and refcount
	// 修改链接数和引用数, 加一
	inode_inc_link_count(inode);
	ihold(inode);

	secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);

	// Write to the dentry to its parent
	// 把这个 inode 和相关 dentry 信息写到父目录去
	err = secondfs_add_link(dentry, inode);
	if (err)
		goto err;

	// fill the inode information in the dentry
	// link 也要 d_instantiate
	d_instantiate(dentry, inode);
	secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);
	return 0;

err:
	secondfs_err("link(): add_link() failed!");
	inode_dec_link_count(inode);
	//discard_new_inode(inode);
	unlock_new_inode(inode);
	secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);
	iput(inode);
	return err;
}

static int secondfs_unlink(struct inode *dir, struct dentry *dentry)
{
	// Unlink dentry with inode.
	// If linkcount of inode falls to zero,
	// this is equivalent to deleting the inode.
	// However, the removing action(free all datablocks,
	// free the inode) is delayed to
	// the executio of secondfs_evict_inode, when
	// the inode's refcount falls to zero.
	// 要求解除 dir 下 dentry 与 inode 的链接.
	// 如果解除后 inode 链接计数为 0, 那么实质上是
	// 删除文件. 这种情况下, inode 失效后, 在
	// secondfs_evict_inode 中会释放 Inode 关联的
	// 数据盘块以及 Inode 自身
	struct inode *inode = d_inode(dentry);
	int err = 0;
	u32 ino;
	IOParameter iop = {
		.isUserP = 0
	};
	DirectoryEntry de;

	secondfs_dbg(FILE, "unlink(%.32s): DELocate() DELETE...", dentry->d_name.name);

	// Locate this file
	// 首先定位到这个文件
	err = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(dir),
			dentry->d_name.name, dentry->d_name.len, SECONDFS_DELETE,
			&iop, &ino);
	if (err) {
		secondfs_err("unlink(%.32s): DELocate() failed", dentry->d_name.name);
		goto out;
	}

	// The file should have the same ino with the one in dentry
	// 可以多加一个检测
	if (ino != d_inode(dentry)->i_ino) {
		secondfs_err("unlink(%.32s): ino != d_inode(dentry)->i_ino", dentry->d_name.name);
		err = -EINVAL;
		goto out;
	}

	// With reference to FileManager::Unlink
	// 参考自 FileManager::Unlink
	de.m_ino = 0;
	strcpy(de.m_name, "(deleted)");
	iop.m_Base = (u8 *)&de;
	iop.m_Count = sizeof(de);
	
	// 原UnixV6++ NameI 在读入一个目录项后 
	// m_Offset 自动加上 DE 大小, 所以返回的 iop 都比
	// 所找目录项超出一个 DE. 原 Unlink 会在这里减回去;

	// 但此处我们改造了 DELocate, 减的操作在 DELocate
	// 完成, 这里不减
	//iop.m_Offset -= sizeof(de);


	secondfs_dbg(FILE, "unlink(%.32s): WriteI() <%d,%.32s>...", dentry->d_name.name , de.m_ino, de.m_name);
	Inode_WriteI(SECONDFS_INODE(dir), &iop);

	if (iop.err != 0) {
		secondfs_err("unlink(%.32s): WriteI() failed (%d)", dentry->d_name.name, iop.err);
		err = iop.err;
		goto out;
	}

	secondfs_inode_conform_s2v(dir, SECONDFS_INODE(dir));
	
	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);
	secondfs_inode_conform_v2s(SECONDFS_INODE(dir), dir);
out:
	return err;
}

static int secondfs_add_dots(struct inode *inode, struct inode *parent)
{
	// Add "." & ".." to the new directory
	// 给新目录加上 "." 和 ".." 
	IOParameter iop;
	DirectoryEntry de;

	if (SECONDFS_SB(inode->i_sb)->s_has_dots != 0xffffffff)
		return 0;

	de.m_ino = cpu_to_le32(inode->i_ino);
	if (de.m_ino == 0) {
		de.m_ino = 0xFFFFFFFF;
	}
	memset(de.m_name, 0, sizeof(de.m_name));
	de.m_name[0] = '.';

	iop.isUserP = 0;
	iop.m_Base = (u8 *)&de;
	iop.m_Count = sizeof(de);
	iop.m_Offset = 0;

	secondfs_dbg(FILE, "add_dots(): first WriteI() <%d,%.32s>...", de.m_ino, de.m_name);
	Inode_WriteI(SECONDFS_INODE(inode), &iop);
	if (iop.err != 0) {
		secondfs_err("add_dots(): WriteI() failed (%d)", iop.err);
		return iop.err;
	}

	de.m_ino = cpu_to_le32(parent->i_ino);
	if (de.m_ino == 0) {
		de.m_ino = 0xFFFFFFFF;
	}
	de.m_name[1] = '.';
	iop.m_Base = (u8 *)&de;
	iop.m_Count = sizeof(de);
	iop.m_Offset = sizeof(de);

	secondfs_dbg(FILE, "add_dots(): second WriteI() <%d,%.32s>...", de.m_ino, de.m_name);
	Inode_WriteI(SECONDFS_INODE(inode), &iop);
	if (iop.err != 0) {
		secondfs_err("add_dots(): WriteI() failed (%d)", iop.err);
		return iop.err;
	}
	secondfs_inode_conform_s2v(inode, SECONDFS_INODE(inode));
	return 0;
}

static int secondfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	int err;

	// Unix V6++ is weird without "." & ".." directory entry.
	// To be compatible with original Unix V6++,
	// only when SuperBlock::s_has_dots == 0xffffffff (which
	// is impossible in original UnixV6++),
	// we read & write "." & ".." in directory file.

	// In addition, DE with ino == 0 will be treated as an
	// empty DE. So the DEs which point to the root directory
	// has ino == 0xffffffff
	
	// ext2 的设计: 将 ".", ".." 对于目录的链接也计入 i_nlink.
	// (https://unix.stackexchange.com/questions/101515/why-does-a-new-directory-have-a-hard-link-count-of-2-before-anything-is-added-to)
	// 也就是说, 新建的目录自带 2 个链接计数(父目录里本身一个, "." 一个)
	// 并且每多一个子目录, 链接计数加一(因为被子目录里的 ".." 引用).
	//
	// 对 mkdir 造成的影响: 创建目录, 自带两个链接计数, 同时父目录
	// 计数加一.
	//
	// 老师给的 Unix V6++ 没有 ".", ".." 目录项, 因此没有额外硬链接的目录,
	// 链接计数是 1.
	//
	// 我们通过 SuperBlock::s_has_dots 来指定是否有 "." "..".
	//  - 若有(0xFFFFFFFF) : 按 ext2 来设计链接计数
	//  - 若没有(其他, 一般为 0x00000000) : 按原 Unix V6++ 来设计链接计数

	// 如果 . 或者 .. 的 ino 是根目录项 (ino == 0, 此目录是根目录的直接子目录),
	// 则不能让其 ino == 0, 否则会使得 DELocate 将其当作空目录项.
	// 如果遇到指向根目录项的 DE, 则把 0 置换成 0xFFFFFFFF 

	if (SECONDFS_SB(dir->i_sb)->s_has_dots == 0xFFFFFFFF) {
		inode_inc_link_count(dir);
		secondfs_inode_conform_v2s(SECONDFS_INODE(dir), dir);
	}

	// 创建一个新 Inode, 注意 mode 附上 IFDIR
	secondfs_dbg(FILE, "mkdir(): secondfs_new_inode()...");
	inode = secondfs_new_inode(dir, mode | S_IFDIR, &dentry->d_name);
	err = PTR_ERR(inode);
	if (IS_ERR(inode)) {
		if (SECONDFS_SB(dir->i_sb)->s_has_dots == 0xffffffff)
			inode_dec_link_count(dir);
		secondfs_err("mkdir(): secondfs_new_inode() failed!");
		goto out;
	}

	// 给新的 Inode 附上操作函数集合
	inode->i_op = &secondfs_dir_inode_operations;
	inode->i_fop = &secondfs_dir_operations;

	if (SECONDFS_SB(dir->i_sb)->s_has_dots == 0xffffffff) {
		inode_inc_link_count(inode);
		secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);
	}

	secondfs_dbg(FILE, "mkdir(): secondfs_add_dots()...");
	err = secondfs_add_dots(inode, dir);
	if (err) {
		secondfs_err("mkdir(): secondfs_add_dots() failed!");
		goto out_fail;
	}

	// 在父目录项登记
	secondfs_dbg(FILE, "mkdir(): secondfs_add_link()...");
	err = secondfs_add_link(dentry, inode);
	if (err) {
		secondfs_err("mkdir(): secondfs_add_link() failed!");
		goto out_fail;
	}

	// 初始化 dentry
	unlock_new_inode(inode);
	d_instantiate(dentry, inode);

out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	//discard_new_inode(inode);
	unlock_new_inode(inode);
	iput(inode);
	secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);

	goto out;
}

static int secondfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	// 要求在 dir 中删除 dentry 关联的目录
	struct inode *inode = d_inode(dentry);
	int err = -ENOTEMPTY;	// 默认错误号置为"目录非空"
	int ret;
	IOParameter iop;
	u32 dummy_ino;

	// We need to tell the directory is not empty
	// The output is put in iop.m_Offset i.e.
	// iop.m_Offset means "target is empty"
	// When DELocate() is called in CHECKEMPTY mode.
	// 现在要判断目录是否为空
	// 函数的输出放在 iop.m_Offset
	secondfs_dbg(FILE, "rmdir(): DELocate() CHECKEMPTY %.32s...", dentry->d_name.name);
	ret = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(inode),
				dentry->d_name.name, dentry->d_name.len, 
				SECONDFS_CHECKEMPTY, &iop, &dummy_ino);

	// 空则 unlink
	secondfs_dbg(FILE, "rmdir(): DELocate() empty? %d", iop.m_Offset);
	if (iop.m_Offset /* Empty */) {
		secondfs_dbg(FILE, "rmdir(): unlink()...");
		err = secondfs_unlink(dir, dentry);
		if (err) {
			secondfs_err("rmdir(): unlink() failed!");
			goto out;
		}

		inode->i_size = 0;
		
		if (SECONDFS_SB(dir->i_sb)->s_has_dots == 0xFFFFFFFF)
			inode_dec_link_count(inode);

		secondfs_inode_conform_v2s(SECONDFS_INODE(inode), inode);

		if (SECONDFS_SB(dir->i_sb)->s_has_dots == 0xffffffff) {
			inode_dec_link_count(dir);
			secondfs_inode_conform_v2s(SECONDFS_INODE(dir), dir);
		}
	}

	// 不空则返回默认错误号
out:
	return err;
}

static void secondfs_set_link(struct inode *dir, IOParameter *iopp,
			struct inode *inode, int update_times)
{
	// The function is to relink the specific DE in dir's
	// directory file to another inode.
	// 将 dir 目录下的特定 DirectoryEntry(由 iop 指向)
	// 改为指向 inode. 如果 update_times 为非 0, 同时更新
	// dir 的修改时间.

	DirectoryEntry de;

	// 我们只需要修改目录项的前四个字节(ino 号)就可以了
	de.m_ino = cpu_to_le32(inode->i_ino);
	iopp->m_Base = (u8 *)&de;
	iopp->m_Count = sizeof(de.m_ino);
	iopp->isUserP = 0;

	secondfs_dbg(FILE, "set_link(): WriteI() <%d,%.32s>...", de.m_ino, de.m_name);
	Inode_WriteI(SECONDFS_INODE(dir), iopp);
	if (iopp->err) {
		secondfs_err("set_link(): WriteI() failed! (%d)", iopp->err);
	}

	if (update_times)
		SECONDFS_INODE(dir)->i_mtime = ktime_get_real_seconds();

	secondfs_inode_conform_s2v(dir, SECONDFS_INODE(dir));
}

#ifdef SECONDFS_KERNEL_BEFORE_4_9
static int secondfs_rename(struct inode * old_dir, struct dentry * old_dentry,
			struct inode * new_dir,	struct dentry * new_dentry)
#else
static int secondfs_rename(struct inode * old_dir, struct dentry * old_dentry,
			struct inode * new_dir,	struct dentry * new_dentry,
			unsigned int flags)
#endif
{
	// The essence of rename is to move the file.
	// rename 的实质是搬动文件的位置.

	struct inode *old_inode = d_inode(old_dentry);
	struct inode *new_inode = d_inode(new_dentry);
	int err;
	u32 dummy_ino;
	IOParameter iop = {
		.isUserP = 0
	};
	DirectoryEntry de;
	int source_is_dir = S_ISDIR(old_inode->i_mode);
	IOParameter iop_dot = {
		.isUserP = 0
	};
	int ret_dot;
	
	// 标志位不能有其他标志
#ifndef SECONDFS_KERNEL_BEFORE_4_9
	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;
#endif

	
	// Find the source
	// 先找源
	secondfs_dbg(FILE, "rename(): DELocate() DELETE old_dentry %.32s... in old_dir", old_dentry->d_name.name);
	err = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(old_dir),
			old_dentry->d_name.name, old_dentry->d_name.len, SECONDFS_DELETE,
			&iop, &dummy_ino);
	if (err) {
		secondfs_err("rename(): DELocate() failed (%d)", err);
		err = -ENOENT;
		goto out;
	}

	// If the source is a directory, we need to locate its
	// ".." DE in advance, for it will be relinked to another
	// parent directory.
	// 如果源文件是个目录, 它是否有 "." ".." 目录项?
	// 因为源目录移动, 肯定是要动它的 ".." 文件的链接位置的.
	if (source_is_dir && SECONDFS_SB(old_inode->i_sb)->s_has_dots == 0xffffffff) {
		secondfs_dbg(FILE, "rename(): DELocate() OPEN_NOT_IGNORE_DOTS source's '..' in old_inode...");
		ret_dot = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(old_inode),
							"..", 2, 
							SECONDFS_OPEN_NOT_IGNORE_DOTS, &iop_dot, &dummy_ino);
		if (ret_dot != 0) {
			secondfs_err("rename(): DELocate() failed (%d)", ret_dot);
			goto out_old;
		}
	}

	// mv source_dir existing_empty_dir is allowed.
	// mv source_dir existing_not_empty_dir or
	// mv source_dir existing_file is not allowed.
	// ext2 文件系统允许 mv 目录覆盖目标空目录.
	// 但目标目录非空或目标文件不是目录时, 会失败.
	if (new_inode) {
		IOParameter iop3;

		secondfs_dbg(FILE, "rename(): new_inode exists");

		// 如果目的文件已经存在:
		//	首先看看源文件是否目录
		//	是目录: 则目的文件必须是一空目录
		if (source_is_dir) {
			IOParameter iop2;
			int ret2;

			if (!S_ISDIR(new_inode->i_mode)) {
				secondfs_err("rename(): mv target is not dir");
				err = -EPERM;
				goto out_dir;
			}

			// 现在要判断目录是否为空
			// 函数的输出放在 iop.m_Offset
			// new_dentry->d_name 的那两条没实际作用
			secondfs_dbg(FILE, "rename(): DELocate() to tell if new_inode is empty directory...");
			ret2 = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(new_inode),
						new_dentry->d_name.name, new_dentry->d_name.len, 
						SECONDFS_CHECKEMPTY, &iop2, &dummy_ino);

			if (ret2 != 0) {
				secondfs_err("rename(): DELocate() failed (%d)", ret2);
				goto out_old;
			}

			// 空则对
			if (!iop2.m_Offset /* not Empty */) {
				err = -ENOTEMPTY;
				secondfs_err("rename(): mv target is not empty");
				goto out_dir;
			}
		}

		// 搜一下新 dentry 的父目录
		// 看是否有新 dentry 的目录项, 以便后续
		// 把新文件链接到旧文件
		iop3.isUserP = 0;
		secondfs_dbg(FILE, "rename(): DELocate() OPEN to ensure new_dentry is in new_dir...");
		err = FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(new_dir),
			new_dentry->d_name.name, new_dentry->d_name.len, SECONDFS_OPEN,
			&iop3, &dummy_ino);
		if (err) {
			secondfs_err("rename(): new_dentry is not in new_dir");
			goto out_dir;
		}

		// 现在可以把新文件链接到旧文件了
		secondfs_dbg(FILE, "rename(): secondfs_set_link()...");
		secondfs_set_link(new_dir, &iop3, old_inode, 1);

		// 对于被替换的文件 Inode, 要递减它的链接计数
		if (source_is_dir && SECONDFS_SB(new_inode->i_sb)->s_has_dots == 0xffffffff) {
			secondfs_dbg(FILE, "rename(): drop_nlink(new_inode)...");
			drop_nlink(new_inode);
		}
		secondfs_dbg(FILE, "rename(): inode_dec_link_count(new_inode)...");
		inode_dec_link_count(new_inode);
	} else {
		// 当目标不存在时
		secondfs_dbg(FILE, "rename(): new_inode does not exist");

		secondfs_dbg(FILE, "rename(): secondfs_add_link()...");
		err = secondfs_add_link(new_dentry, old_inode);
		if (err) {
			secondfs_err("rename(): secondfs_add_link() failed");
			goto out_dir;
		}
		
		// 对于目标文件的父 Inode, 由于多了一个子目录, 要递增链接计数.
		if (source_is_dir && SECONDFS_SB(old_inode->i_sb)->s_has_dots == 0xffffffff) {
			inode_inc_link_count(new_dir);
		}
	}
	
#ifdef SECONDFS_KERNEL_BEFORE_4_9
	old_inode->i_ctime = CURRENT_TIME_SEC;
#else
	old_inode->i_ctime = current_time(old_inode);
#endif

	
	mark_inode_dirty(old_inode);

	secondfs_dbg(FILE, "rename(): conforming new_dir...");
	secondfs_inode_conform_v2s(SECONDFS_INODE(new_dir), new_dir);
	if (new_inode) {
		secondfs_dbg(FILE, "rename(): conforming new_inode...");
		secondfs_inode_conform_v2s(SECONDFS_INODE(new_inode), new_inode);
	}
	secondfs_dbg(FILE, "rename(): conforming old_dir...");
	secondfs_inode_conform_v2s(SECONDFS_INODE(old_dir), old_dir);
	secondfs_dbg(FILE, "rename(): conforming old_inode...");
	secondfs_inode_conform_v2s(SECONDFS_INODE(old_inode), old_inode);

	// 现在可以移除源文件了. 源文件在父目录项中的位置还记在 iop 里面.
	// 参考自 FileManager::Unlink
	de.m_ino = 0;
	iop.m_Base = (u8 *)&de;
	iop.m_Count = sizeof(de);
	secondfs_dbg(FILE, "rename(): WriteI() (remove source) <%d,%.32s>...", de.m_ino, de.m_name);
	Inode_WriteI(SECONDFS_INODE(old_dir), &iop);
	if (iop.err) {
		secondfs_err("rename(): WriteI() (remove source) failed!");
		err = iop.err;
		goto out_eio;
	}

	secondfs_inode_conform_s2v(old_dir, SECONDFS_INODE(old_dir));
	
	// 如果我们是搬动目录的话, 最后还要重新链接源目录的 ".."
	if (source_is_dir && SECONDFS_SB(old_inode->i_sb)->s_has_dots == 0xffffffff) {
		if (old_dir != new_dir)
		{
			secondfs_dbg(FILE, "rename(): secondfs_set_link() (.. -> new_dir)...");
			secondfs_set_link(old_inode, &iop_dot, new_dir, 0);
		}
		inode_dec_link_count(old_dir);
		secondfs_dbg(FILE, "rename(): conforming old_dir...");
		secondfs_inode_conform_v2s(SECONDFS_INODE(old_dir), old_dir);
	}
	return 0;
out_dir:
out_eio:
out_old:
out:
	return err;
}

struct file_operations secondfs_file_operations = {
	.llseek = generic_file_llseek,

	// 我们跳过 Linux 读写基于页缓存-块设备的文件系统常用的
	// kiocb,iovec - address_map - page cache - readpage()/direct_IO()
	// 调用路径. 所以, 我们不指定
	// read_iter 和 write_iter 方法.
	//.read_iter = secondfs_file_read_iter,
	//.write_iter = secondfs_file_write_iter,

	// 相反, 我们指定 read() 方法和 write() 方法, 以
	// Unix V6++ 的方式读写文件.
	.read = secondfs_file_read,
	.write = secondfs_file_write,

	.open = generic_file_open,
	.fsync = secondfs_fsync
};

static int secondfs_readdir(struct file *file, struct dir_context *ctx)
{
	// 要求对于 file 目录的各项, 逐项调用 dir_emit.
	// 这样系统就能遍历这个目录了.

	// 这个 pos 我不知道是什么, 姑且当作在目录文件里的偏移好了
	struct inode *inode = file_inode(file);
	IOParameter iop;
	unsigned int type = DT_UNKNOWN;
	void *params[4] = {
		&dir_emit,
		ctx,
		&type,
		&ctx->pos
	};
	
	secondfs_dbg(FILE, "readdir()...");

	if (SECONDFS_SB(inode->i_sb)->s_has_dots != 0xffffffff) {
		if (ctx->pos > inode->i_size + 2 * sizeof(DirectoryEntry) - sizeof(DirectoryEntry)) {
			secondfs_err("readdir(): ctx->pos > inode->i_size + sizeof(DirectoryEntry)");
			return 0;
		}
	} else {
		if (ctx->pos > inode->i_size - sizeof(DirectoryEntry)) {
			secondfs_err("readdir(): ctx->pos > inode->i_size - sizeof(DirectoryEntry)");
			return 0;
		}
	}

	iop.isUserP = 0;
	iop.m_Base = NULL;

	// Regardless of whether secsb->s_has_dots == 0xffffffff,
	// to upper VFS layer, our directory file always looks
	// as if it has "." and ".." directory entry.
	// So when secsb->s_has_dots != 0xffffffff,
	// ctx->pos (VFS offset on "imaginary directory file with
	// '.' & '..'") == iop->m_Offset + 2 * sizeof(DirectoryEntry).
	// When secsb->s_has_dots == 0xffffffff,
	// ctx->pos == iop->m_Offset.

	// 为统一, 我们不管 secsb->s_has_dots 是否置位,
	// 我们让 VFS 上层以为, 目录项的第 0 个 DirectoryEntry
	// 总是放 ".", 第 1 个 DirectoryEntry 放 "..".
	// 那么, secsb->s_has_dots 未置位的目录项中, 
	// 系统传过来的 pos 总是要比我们实际在文件中的 iop->m_Offset
	// 超前两个 DirectoryEntry;
	// 如果 secsb->s_has_dots 置位,
	// pos 和 iop->m_Offset 是一致的.

	// 是否 emit 点目录项
	// Whether should we emit "." & ".."
	if (ctx->pos < sizeof(DirectoryEntry)) {
		if (!dir_emit_dot(file, ctx))
			return false;
		ctx->pos = sizeof(DirectoryEntry);
		secondfs_dbg(FILE, "readdir(): emit_dot");
	}
	if (ctx->pos < 2 * sizeof(DirectoryEntry)) {
		if (!dir_emit_dotdot(file, ctx))
			return false;
		ctx->pos = 2 * sizeof(DirectoryEntry);
		secondfs_dbg(FILE, "readdir(): emit_dotdot");
	}

	iop.m_Offset = ctx->pos;

	// 然后若 s_has_dots 置位, 不管; 否则, m_Offset 要减掉 2 位.
	if (SECONDFS_SB(inode->i_sb)->s_has_dots != 0xffffffff) {
		iop.m_Offset -= 2 * sizeof(DirectoryEntry);
	}

	secondfs_dbg(FILE, "readdir(): DELocate() LIST...");
	FileManager_DELocate(secondfs_filemanagerp, SECONDFS_INODE(inode), "", 0,
			SECONDFS_LIST, &iop, (u32 *)params);
	return 0;
	
}

struct inode_operations secondfs_file_inode_operations = {
};

struct file_operations secondfs_dir_operations = {
	.llseek = generic_file_llseek,

	.read = generic_read_dir,	// 这个函数会直接返回错误, 因为不能直接读取目录
	.iterate = secondfs_readdir,	// 遍历目录
	.fsync = secondfs_fsync
};

struct inode_operations secondfs_dir_inode_operations = {
	.create = secondfs_create,
	.lookup = secondfs_lookup,
	.link = secondfs_link,
	.unlink = secondfs_unlink,
	.mkdir = secondfs_mkdir,
	.rmdir = secondfs_rmdir,
	//.mknod = secondfs_mknod,
	.rename = secondfs_rename
};