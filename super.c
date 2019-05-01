#include "secondfs.h"
#include "UNIXV6PP/FileSystem_c_wrapper.h"

#if 1

/* secondfs_iget : 根据 Inode 编号获取 VFS Inode 顺带 UnixV6++ Inode.
 *      sb : 系统传过来的 (VFS) 超块指针
 *      ino : Inode 编号
 * 
 * 先检测系统的 Inode 缓存存不存在这个 Inode, 
 * 若不存在, 则 alloc, 并根据 UnixV6++ 的方式初始化它.
 *
 * 参考 InodeTable::IGet() 改写
 */
struct inode *secondfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	int err;
	Inode *si;
	BufferManager *bm = secondfs_buffermanagerp;

	// 这个 Inode 是否在系统高速缓存? 是, 增加其引用计数,
	// 直接返回;
	// 否, iget_locked 会调用 alloc_inode 分配一个.
	// alloc_inode 又会调用 SecondFS 特定的 
	// secondfs_alloc_inode 函数, 分配一个 Inode 
	// 及其随附的 VFS Inode.
	// 之后增加其引用计数, 返回.
	inode = iget_locked(sb, ino);
	if (!inode) {
		// 把错误号转成错误指针, 返回
		return ERR_PTR(-ENOMEM);
	}

	// 如果这个 Inode 不是新分配的话, 可以直接返回
	if (!(inode->i_state & I_NEW)) {
		return inode;
	}

	si = SECONDFS_INODE(inode);	// 获得包含 VFS Inode 的那个 SecondFS Inode
	
	si->i_ssb = SECONDFS_SB(sb);
	si->i_number = ino;
	// si->i_flag = SECONDFS_ILOCK;
	// si->i_count++;
	si->i_lastr = -1;

	
	/* 将该外存Inode读入缓冲区 */
	Buf* pBuf = BufferManager_Bread(bm, SECONDFS_SB(sb)->s_dev, SECONDFS_INODE_ZONE_START_SECTOR + ino / SECONDFS_INODE_NUMBER_PER_SECTOR );

	/* 如果发生I/O错误 */
	if(pBuf->b_flags & SECONDFS_B_ERROR)
	{
		/* 释放缓存 */
		BufferManager_Brelse(bm, pBuf);
		/* 释放占据的内存Inode */
		iget_failed(inode);
		return ERR_PTR(-EIO);	// TODO: 不严谨的 ERROR
	}

	/* 将缓冲区中的外存Inode信息拷贝到新分配的内存Inode中 */
	Inode_ICopy(si, pBuf, ino);
	// 添加步骤: 将 Inode 的数据拷贝到 VFS Inode 中
	inode->i_mode = si->i_mode;
	i_uid_write(inode, si->i_uid);
	i_gid_write(inode, si->i_gid);
	set_nlink(inode, si->i_nlink);
	inode->i_size = si->i_size;
	inode->i_atime.tv_sec = si->i_atime;
	inode->i_mtime.tv_sec = si->i_mtime;

	// TODO: 根据文件的不同属性(是 Regular File/Directory/Link),
	// 给 inode 赋值不同的 file_operations, inode_operations
	// 以及 address_space_operations

	/* 释放缓存 */
	BufferManager_Brelse(bm, pBuf);
	return inode;
	
}

/* secondfs_alloc_inode : 在系统 alloc_inode 函数中调用.
 *			用以为 SecondFS 分配一个 VFS Inode.
 * 其指针会传给内核供其调用.
 *      sb : 系统传过来的 (VFS) 超块指针
 * 
 * 从 kmem_cache 里分配一个 Inode, 然后
 * 把指向它的成员 vfs_inode 的指针返回.
 * 这样就能保证每一个 SecondFS VFS Inode
 * 都伴随一个 SecondFS Inode.
 *
 */
struct inode *secondfs_alloc_inode(struct super_block *sb)
{
	Inode *si;
	si = kmem_cache_alloc(secondfs_icachep, GFP_KERNEL);
	if (!si) {
		return NULL;
	}

	return &si->vfs_inode;
}


/* secondfs_destroy_inode : 在系统中调用.
 *			用以为 SecondFS 分配一个 VFS Inode.
 * 其指针会传给内核供其调用.
 *      sb : 系统传过来的 (VFS) 超块指针
 * 
 * 从 kmem_cache 里分配一个 Inode, 然后
 * 把指向它的成员 vfs_inode 的指针返回.
 * 这样就能保证每一个 SecondFS VFS Inode
 * 都伴随一个 SecondFS Inode.
 *
 */
void secondfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(secondfs_icachep, SECONDFS_INODE(inode));
}


/* secondfs_sync_fs : 将超块同步回磁盘
 * 其指针会传给内核供其调用.
 *      sb : 系统传过来的 (VFS) 超块指针
 *      wait : 是否等待. 在本文件系统中只能置为 1
 *
 * 注意: 在大多数 Linux 文件系统中, 超块均是以
 * fill_super 中读缓存块并不释放缓存, 将缓存块内容
 * 转换为 struct super_block * 一直使用, 最后在
 * put_super 中释放缓存块的方式管理的. 由于缓存块
 * 会自动与磁盘同步, 因此几乎不需要
 * write_super 或 sync_fs 方法.
 * 
 * Unix V6++ 读超块的逻辑是: 读缓存块 - 从缓存块中拷贝数据
 * - 释放缓存块, 所以需要这几个函数来将超块写回磁盘.
 * 
 */
int secondfs_sync_fs(struct super_block *sb, int wait)
{
	SuperBlock *secsb = SECONDFS_SB(sb);
	int err;

	if (!wait) {
		return 0;
	}

	pr_info("secondfs: syncing fs");
	FileSystem_Update(secondfs_filesystemp, secsb);

	return 0;
}

/* secondfs_write_super : 将超块同步回磁盘
 *      sb : 系统传过来的 (VFS) 超块指针
 *
 * 注意: 在大多数 Linux 文件系统中, 超块均是以
 * fill_super 中读缓存块并不释放缓存, 将缓存块内容
 * 转换为 struct super_block * 一直使用, 最后在
 * put_super 中释放缓存块的方式管理的. 由于缓存块
 * 会自动与磁盘同步, 因此几乎不需要
 * write_super 或 sync_fs 方法.
 * 
 * Unix V6++ 读超块的逻辑是: 读缓存块 - 从缓存块中拷贝数据
 * - 释放缓存块, 所以需要这几个函数来将超块写回磁盘.
 * 
 */
static void secondfs_write_super(struct super_block *sb)
{
	if (!sb_rdonly(sb))
		// 即有等待地同步超块
		secondfs_sync_fs(sb, 1);
}

/* secondfs_fill_super
 * 其指针会传给内核供其初始化超块.
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

int secondfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *i_root;
	SuperBlock *secsb;
	Devtab *devtab;
	Buf *bp;
	struct inode *root_inode;
	int err = 0;

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
	//sb->s_d_op = &dentry; //?

	if (be32_to_cpu(secsb->s_ronly)) {
		pr_warn("this SecondFS volume is read-only.");
		sb->s_flags |= SB_RDONLY;
	}

	// 读入根目录的 Inode
	// 调用 secondfs_iget 获得根目录的 Inode
	// 一般是找不到内存中已载入的 Inode 的, 必须从磁盘上读入
	root_inode = secondfs_iget(sb, SECONDFS_ROOTINO);
	if (IS_ERR(root_inode)) {
		iput(root_inode);
		pr_err("failed loading root inode.");
		// 把错误指针转为错误号
		err = PTR_ERR(root_inode);
		goto out_free;
	}

	// 用根目录的 Inode 建立根目录的 dentry
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		ps_err("failed making root dentry.");
		err = -ENOMEM;
		goto out_free;
	}
	secsb->s_inodep = container_of(root_inode, Inode, vfs_inode);

	inode_init_owner(root_inode, sb->s_root, root_inode->i_mode);
	secondfs_write_super(sb);
	goto out;

out_free:
	deleteSuperBlock(secsb);
	deleteDevtab(devtab);

out:
	return err;
}

/* secondfs_put_super
 * 其指针会传给内核供其释放超块.
 * 
 * secondfs_put_super : 释放超块.
 *      sb : 系统传过来的 (VFS) 超块指针, 
 *           函数的作用就是释放与其相关的结构.
 */
void secondfs_put_super(struct super_block *sb)
{
	SuperBlock *secsb = SECONDFS_SB(sb);

	if (!sb_rdonly(sb)) {
		secondfs_sync_fs(sb, 1);
	}

	deleteDevtab(secsb->s_dev);
	deleteSuperBlock(secsb);
}

/* secondfs_mount
 * 其指针会传给内核供其挂载文件系统.
 * 
 * secondfs_mount : 调用系统默认的 mount_bdev 函数
 *                  来挂载.
 * 
 * 会传递 secondfs_fill_super 函数指针供系统初始化 VFS 超块.
 */
struct dentry *secondfs_mount(struct file_system_type *fs_type,
				int flags, const char *devname,
				void *data) {
	return mount_bdev(fs_type, flags, dev_name, data, secondfs_fill_super);
}

#endif