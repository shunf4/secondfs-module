#include "secondfs_kern.h"
#include "UNIXV6PP/FileSystem_c_wrapper.h"

#if 1

/* secondfs_iget : 根据 Inode 编号获取 VFS Inode 顺带 UnixV6++ Inode.
 *      sb : 系统传过来的 (VFS) 超块指针
 *      ino : Inode 编号
 * 
 * 先检测系统的 Inode 缓存存不存在这个 Inode, 
 * 若不存在, 则 alloc, 并根据 UnixV6++ 的方式初始化它.
 *
 */
struct inode *secondfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	int err;

	// 这个 Inode 是否在系统高速缓存? 是, 直接返回;
	// 否, iget_locked 会调用 alloc_inode 分配一个.
	// alloc_inode 又会调用文件系统特定的 alloc_inode 函数
	inode = iget_locked(sb, ino);
	if (!inode) {
		// 把错误号转成错误指针, 返回
		return ERR_PTR(-NOMEM);
	}



	
}


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

static int secondfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *i_root;
	SuperBlock *secsb;
	Devtab *devtab;
	Buf *bp;
	struct inode *root_inode;
	int err;

	secsb = newSuperBlock();
	devtab = newDevtab();

	secsb->s_dev = devtab;
	secsb->s_dev->d_bdev = sb->s_bdev;
	
	// 从硬盘读入 Superblock 块. 注意, 未作任何大小字序转换!
	FileSystem_LoadSuperBlock(secondfs_filesystemp, secsb);

	// 根据读入的超块, 更新 VFS 超块的内容.
	sb->s_fs_info = secsb;
	// Unix V6++ 的最高二级盘块转换支持的最大文件大小
	sb->s_maxbytes = SECONDFS_BLOCK_SIZE * (128 * 128 * 2 + 128 * 2 + 6);
	sb->s_op = &secondfs_sb_ops;

	// 需要文件系统特定的 dentry 操作函数吗?
	sb->s_d_op = &dentry?

	if (be32_to_cpu(secsb->s_ronly)) {
		pr_warn("this SecondFS volume is read-only.");
		sb->s_flags |= SB_RDONLY;
	}

	// 读入根目录的 Inode
	// 调用 secondfs_iget 获得根目录的 Inode
	// 一般是找不到内存中已载入的 Inode 的, 必须从磁盘上读入
	root_inode = secondfs_iget(sb, SECONDFS_ROOTINO);
	if (IS_ERR(root_inode)) {
		pr_err("failed loading root inode.");
		// 把错误指针转为错误号
		err = PTR_ERR(rootinoe);
		goto out_free;
	}

	// 用根目录的 Inode 建立根目录的 dentry
	sb->s_root = d_make_root(root);
	if (!sb->s_root) {
		ps_err("failed making root dentry.");
		err = -ENOMEM;
		goto out_free;
	}

	inode_init_owner(root_inode, sb->s_root, root_inode->i_mode);

out_free:
	deleteSuperBlock(secsb);
	deleteDevtab(devtab);

out:
	return err;
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

#endif;