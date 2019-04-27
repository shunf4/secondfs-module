#include "secondfs_kern.h"

/* 无需外部声明的函数: secondfs_fill_super
 * 但其指针会传给内核供其初始化超块.
 * 
 * secondfs_fill_super : 初始化超块.
 *      sb : 系统传过来的 (VFS) 超块指针, 
 *           函数的作用就是合理初始化它.
 *      data(自定义数据) 和 silent 我们这里不用
 * 
 * 思路就是从设备中取出来 Superblock 块, 这是一个 SecondFS 格式的超块; 
 * 然后用其中的内容初始化 VFS 格式的 sb
 * 
 * 关联的 UNIXV6PP 函数:
 * void FileSystem::LoadSuperBlock()
 */
static int secondfs_fill_super(struct super_block *sb, void *data, int silent) {
	struct inode *i_root;
	SuperBlock *secsb;
	Devtab *devtab;
	Buf *bp;

	secsb = newSuperBlock();
	devtab = newDevtab();

	secsb->s_dev = devtab;
	
	// 读入 Superblock 块的缓存.
	FileSystem_LoadSuperBlock(secondfs_filesystemp, secsb);

	sb->s_fs_info = secsb;
	sb->s_maxbytes = SECONDFS_BLOCK_SIZE;
	sb->s_op = &secondfs_sb_ops;


	// 读入
}

/* secondfs_mount
 * 其指针会传给内核供其挂载文件系统.
 * 
 * secondfs_mount : 调用系统默认的 mount_bdev 函数
 *                  来挂载.
 * 
 * 会传递 fill_super 函数指针供系统初始化 VFS 超块.
 */
struct dentry *secondfs_mount(struct file_system_type *fs_type,
				int flags, const char *devname,
				void *data) {
	return mount_bdev(fs_type, flags, dev_name, data, secondfs_fill_super);
}

