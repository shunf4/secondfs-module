#include "secondfs.h"
#include "UNIXV6PP/FileSystem_c_wrapper.h"

#if 1
/* secondfs_iget : 
 * Similar to kernel iget_locked(): *Get* an (vfs) inode
 * by (vfs) super_block and inode number (when cannot find
 * in memory, an inode and a UnixV6++ Inode are allocated, 
 * filled and they bidirectionally linked)
 *      sb : pointer to current VFS super_block
 *      ino : Inode number
 *
 * Written with reference to InodeTable::IGet()
*/

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
	Inode *si;
	BufferManager *bm = secondfs_buffermanagerp;
	Buf* pBuf;

	// Effects of iget_locked:
	// If this inode is in memory, increase the refcount
	// and return it;
	// else, alloc_inode is called, in which secondfs_alloc_inode
	// is called, in which a SecondFS Inode is allocated.
	// an Inode contains an inode struct.
	// Then pointer to the inner inode struct is returned.
	
	// 这个 Inode 是否在系统高速缓存? 是, 增加其引用计数,
	// 直接返回;
	// 否, iget_locked 会调用 alloc_inode 分配一个.
	// alloc_inode 又会调用 SecondFS 特定的 
	// secondfs_alloc_inode 函数, 分配一个 Inode 
	// 及其随附的 VFS Inode.
	// 之后增加其引用计数, 返回.

	secondfs_dbg(INODE, "iget(%p/%lu)", sb, ino);
	inode = iget_locked(sb, ino);
	if (!inode) {
		// 把错误号转成错误指针, 返回
		return ERR_PTR(-ENOMEM);
	}

	// return this inode directly if it is not newly created
	// 如果这个 Inode 不是新分配的话, 可以直接返回
	if (!(inode->i_state & I_NEW)) {
		secondfs_dbg(INODE, "iget(%p/%lu): not new inode; return", sb, ino);
		return inode;
	}

	// Get the container Inode
	// 获得包含 VFS Inode 的那个 SecondFS Inode
	si = SECONDFS_INODE(inode);
	
	si->i_ssb = SECONDFS_SB(sb);
	si->i_number = ino;
	// si->i_flag = SECONDFS_ILOCK;
	// si->i_count++;
	si->i_lastr = -1;

	
	/* 将该外存Inode读入缓冲区 */
	secondfs_dbg(INODE, "iget(%p/%lu): read from disk", sb, ino);
	pBuf = BufferManager_Bread(bm, SECONDFS_SB(sb)->s_dev, SECONDFS_INODE_ZONE_START_SECTOR + ino / SECONDFS_INODE_NUMBER_PER_SECTOR );

	if (IS_ERR(pBuf)) {
		secondfs_dbg(INODE, "iget(%p/%lu): read from disk error! %lu", sb, ino, PTR_ERR(pBuf));
		iget_failed(inode);
		return NULL;
	}

	/* 如果发生I/O错误(never reached) */
	if(pBuf->b_flags & SECONDFS_B_ERROR)
	{
		/* 释放缓存 */
		BufferManager_Brelse(bm, pBuf);
		/* 释放占据的内存Inode */
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}

	/* Transfer DiskInode information to newly allocated Inode */
	/* 将缓冲区中的外存Inode信息拷贝到新分配的内存Inode中 */
	Inode_ICopy(si, pBuf, ino);
	
	// Transfer Inode information to VFS inode
	secondfs_inode_conform_s2v(inode, si);

	// Assign file_operations & inode_operations function
	// table to this inode according to its mode(Regular/Directory)

	// 根据文件的不同属性(是 Regular File/Directory/Link),
	// 给 inode 赋值不同的 file_operations, inode_operations
	// 以及 address_space_operations
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &secondfs_file_inode_operations;
		inode->i_fop = &secondfs_file_operations;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &secondfs_dir_inode_operations;
		inode->i_fop = &secondfs_dir_operations;
	} else {
		secondfs_err("iget(%p/%lu): this inode is neither a regular file nor a directory!", sb, ino);
	}

	/* 释放缓存 */
	BufferManager_Brelse(bm, pBuf);
	unlock_new_inode(inode);
	return inode;	
}

// iget for C++ - returns Inode * instead of inode *
Inode *secondfs_iget_forcc(SuperBlock *secsb, unsigned long ino)
{
	struct inode *inode = secondfs_iget(secsb->s_vsb, ino);
	if (IS_ERR_OR_NULL(inode))
		return NULL;
	return container_of(secondfs_iget(secsb->s_vsb, ino), Inode, vfs_inode);
}

/* secondfs_alloc_inode : Called in alloc_inode
 *			to allocate a Inode&VFS inode.
 * the pointer of this function will be passed to the system.
 *      sb : VFS super_block
 * 
 * alloc an Inode from kmem_cache, then return a pointer to
 * its member vfs_inode.
 * Thus it is ensured that every SecondFS vfs inode is contained
 * inside an Inode.
 *
 */

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
	si = newInode();
	if (!si) {
		secondfs_err("secondfs_alloc_inode: null pointer from newInode()!");
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
 * 	Synchronize the super_block/SuperBlock back to disk.
 * 其指针会传给内核供其调用. Pointer to this function is passed
 * 			to the system.
 *      sb : 系统传过来的 (VFS) 超块指针 VFS super_block
 *      wait : 是否等待. 在本文件系统中只能置为 1 Must be 1(true)
 * 
 * Note: in most filesystem, filesystem specific
 * superblock structure is preserved in memory in
 * the form of buffer(page) cache, thus no need for
 * synchronize back to disk - the buffer cache mechanism
 * does this automatically when needed.
 * 
 * However, Unix V6++ does not use linux buffer cache
 * mechanism, thus the need for write_super and sync_fs.
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

	if (!wait) {
		return 0;
	}

	secondfs_dbg(GENERAL, "SB %p: sync_fs...", secsb);
	FileSystem_Update(secondfs_filesystemp, secsb);
	// Bflush() will be executed at the end of Update()
	return 0;
}


/* secondfs_write_super : 将超块同步回磁盘
	Synchronize the super_block/SuperBlock back to disk.
 *      sb : 系统传过来的 (VFS) 超块指针
 * 		VFS super_block
 *
 * Note: in most filesystem, filesystem specific
 * superblock structure is preserved in memory in
 * the form of buffer(page) cache, thus no need for
 * synchronize back to disk - the buffer cache mechanism
 * does this automatically when needed.
 * 
 * However, Unix V6++ does not use linux buffer cache
 * mechanism, thus the need for write_super and sync_fs.
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
	secondfs_dbg(GENERAL, "SB %p: write_super...", SECONDFS_SB(sb));
#ifdef SECONDFS_KERNEL_BEFORE_4_14
	if (!(sb->s_flags & MS_RDONLY))
		// 即有等待地同步超块
		secondfs_sync_fs(sb, 1);
#else
	if (!sb_rdonly(sb))
		// 即有等待地同步超块
		secondfs_sync_fs(sb, 1);
#endif
}

/* 
 * secondfs_fill_super : 初始化超块. Initialize the vfs super_block.
 * 其指针会传给内核供其初始化超块. Pointer to it is passed to system.
 * 
 *      sb : 系统传过来的 (VFS) 超块指针, 
 *           函数的作用就是合理初始化它.
 * 		VFS super_block from the system.
 * 		We must properly fill/initialize it.
 *      data(自定义数据) 和 silent 我们这里不用
 * 		data/silent is not used here.
 * 
 * Procedure: read SuperBlock blocks(1024 Bytes) and fill the 
 * super_block properly using information from it.
 * 思路就是从设备中取出来 Superblock 块, 这是一个 SecondFS 格式的超块; 
 * 然后用其中的内容初始化 VFS 格式的 sb
 * 
 * 关联的 UNIXV6PP 函数:
 * Associated UnixV6++ function:
 * void FileSystem::LoadSuperBlock()
 */

int secondfs_fill_super(struct super_block *sb, void *data, int silent)
{
	SuperBlock *secsb;
	Devtab *devtab;
	struct inode *root_inode;
	int ret = 0;

	secondfs_dbg(SB_FILL, "SB %p: newing SuperBlock & Devtab...", SECONDFS_SB(sb));
	secsb = newSuperBlock();
	devtab = newDevtab();

	secsb->s_dev = devtab;
	secsb->s_dev->d_bdev = sb->s_bdev;
	secsb->s_vsb = sb;
	
	// Read SuperBlock(little-endian) from the disk.
	// 从硬盘读入 Superblock 块. 注意, 未作任何大小字序转换!
	ret = FileSystem_LoadSuperBlock(secondfs_filesystemp, secsb);
	if (IS_ERR_VALUE(ret)) {
		secondfs_err("fill_super: LoadSuperBlock failed.");
		goto out_free;
	}

	// Fill VFS sb according to SuperBlock
	// 根据读入的超块, 更新 VFS 超块的内容.
	sb->s_fs_info = secsb;
	// Unix V6++ 的最高二级盘块转换支持的最大文件大小
	sb->s_maxbytes = SECONDFS_BLOCK_SIZE * (128 * 128 * 2 + 128 * 2 + 6);
	sb->s_op = &secondfs_sb_ops;

	// 需要文件系统特定的 dentry 操作函数吗?
	//sb->s_d_op = &dentry; //?

	if (le32_to_cpu(secsb->s_ronly)) {
		secondfs_warn("fill_super: this SecondFS volume is read-only.");
#ifdef SECONDFS_KERNEL_BEFORE_4_14
		sb->s_flags |= MS_RDONLY;
#else
		sb->s_flags |= SB_RDONLY;
#endif
		
	}

	// Read Inode of root directory
	// 读入根目录的 Inode
	// 调用 secondfs_iget 获得根目录的 Inode
	// 一般是找不到内存中已载入的 Inode 的, 必须从磁盘上读入

	secondfs_dbg(SB_FILL, "SB %p: getting root inode...", SECONDFS_SB(sb));
	root_inode = secondfs_iget(sb, SECONDFS_ROOTINO);
	if (IS_ERR(root_inode)) {
		iput(root_inode);
		secondfs_err("failed loading root inode.");
		// 把错误指针转为错误号
		ret = PTR_ERR(root_inode);
		goto out_free;
	}

	// Make root dentry
	// 用根目录的 Inode 建立根目录的 dentry
	secondfs_dbg(SB_FILL, "SB %p: making root entry...", SECONDFS_SB(sb));
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		secondfs_err("SB %p: failed making root dentry.", SECONDFS_SB(sb));
		ret = -ENOMEM;
		goto out_free;
	}
	secsb->s_inodep = container_of(root_inode, Inode, vfs_inode);

	FileSystem_PrintSuperBlock(secondfs_filesystemp, secsb);

	inode_init_owner(root_inode, NULL, root_inode->i_mode);
	secondfs_write_super(sb);
	goto out;

out_free:
	deleteSuperBlock(secsb);
	deleteDevtab(devtab);

out:
	return ret;
}

/* secondfs_put_super
 * 其指针会传给内核供其释放超块.
 * Pointer to it is passed to system.
 * 
 * secondfs_put_super : 释放超块.
 * 			Release the super_block
 *      sb : 系统传过来的 (VFS) 超块指针, 
 *           函数的作用就是释放与其相关的结构.
 */
void secondfs_put_super(struct super_block *sb)
{
	SuperBlock *secsb = SECONDFS_SB(sb);

	secondfs_dbg(GENERAL, "put super %p...", SECONDFS_SB(sb));

#ifdef SECONDFS_KERNEL_BEFORE_4_14
	if (!(sb->s_flags & MS_RDONLY))
		// 即有等待地同步超块
		secondfs_sync_fs(sb, 1);
#else
	if (!sb_rdonly(sb))
		// 即有等待地同步超块
		secondfs_sync_fs(sb, 1);
#endif

	deleteDevtab(secsb->s_dev);
	deleteSuperBlock(secsb);
}

/* secondfs_mount
 * 其指针会传给内核供其挂载文件系统.
 * Pointer to it is passed to system.
 * 
 * secondfs_mount : 调用系统默认的 mount_bdev 函数
 *                  来挂载.
 * 		Call default mount_bdev function with 
 * 		parameter "secondfs_fill_super" to mount
 * 		a secondfs volume.
 * 
 * 会传递 secondfs_fill_super 函数指针供系统初始化 VFS 超块.
 */
struct dentry *secondfs_mount(struct file_system_type *fs_type,
				int flags, const char *devname,
				void *data) {
	secondfs_dbg(GENERAL, "mount %s...", devname);
	return mount_bdev(fs_type, flags, devname, data, secondfs_fill_super);
}

#endif