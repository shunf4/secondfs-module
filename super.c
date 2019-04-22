#include "secondfs_kern.h"
#include "UNIXV6PP/FileSystem.hpp"
#include "UNIXV6PP/FileSystem_c_wrapper.h"

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
	struct SuperBlock *secsb;

	struct buffer_head *sbbh;
	
	// 读入 Superblock 块的缓存. bread 加上前缀 sb_bread 是 Linux 提供的一个捷径, 可通过 superblock 结构获取设备信息.
	sbbh = sb_bread(sb, secondfs_superblock_blockno);
	BUG_ON(sbbh == NULL);
	
	// sbbh->b_data 就是该块缓存的内容.
	secsb = (struct SuperBlock *)sbbh->b_data;

	


	// 读入
}