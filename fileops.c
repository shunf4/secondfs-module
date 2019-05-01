#include "secondfs.h"

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

	.open = secondfs_file_open,
	.write = secondfs_file_write,
	.release = secondfs_release_file
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