#include "secondfs.h"


ssize_t secondfs_file_read(struct file *filp, char __user *buf, size_t len,
				loff_t *ppos)
{
	// filp->f_inode 是缓存数据, 实时数据在 path 结构里面
	struct inode *inode = filp->f_path.dentry->d_inode;
	Inode *si = SECONDFS_INODE(inode);
	ssize_t ret;
	
	ret = FileManager_Read(secondfs_filemanagerp, buf, len, ppos, si);
	return ret;
}

ssize_t secondfs_file_write(struct file *filp, char __user *buf, size_t len,
				loff_t *ppos)
{
	// filp->f_inode 是缓存数据, 实时数据在 path 结构里面
	struct inode *inode = filp->f_path.dentry->d_inode;
	Inode *si = SECONDFS_INODE(inode);
	ssize_t ret;
	
	ret = FileManager_Write(secondfs_filemanagerp, buf, len, ppos, si);
	secondfs_conform_s2v(inode, si);
	mark_inode_dirty_sync(inode);
	return ret;
}

int secondfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	// 要求将文件同步到磁盘上
	// 我们就简单地整设备同步即可
	BufferManager_Bflush(secondfs_buffermanagerp, SECONDFS_INODE(file->f_path.dentry->d_inode)->i_ssb->s_dev);
	return 0;
}

static int secondfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	// 要求在 dir 下为新创建文件
	struct inode *inode;
	int err;

	// 先为文件分配新 inode
	inode = secondfs_new_inode(dir, mode, dentry->d_name);
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
	.write = secondfs_file_write,
	.fsync = secondfs_fsync
};

struct file_operations secondfs_dir_operations = {
	.llseek = generic_file_llseek,

	.read = generic_read_dir,
	.fsync = secondfs_fsync
};

struct inode_operations secondfs_dir_inode_operations = {
	.create = secondfs_create,
	.lookup = secondfs_lookup,
	.mkdir = secondfs_mkdir,
	.rmdir = secondfs_rmdir,
	.rename = secondfs_rename
};