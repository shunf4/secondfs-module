#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/mm.h>

#include "secondfs.h"

/*
 * secondfs_submit_bio : 跳过系统为块设备准备的缓存, 直接操作块设备
 * 
 * 	其核心为向通用块层直接提交一个 bio request, 并等待其结束.
 * 
 * 	sb : 当前文件系统的 VFS 超块指针.
 * 	sector : 扇区号. 一个扇区是, 且对齐到 512 字节.
 * 	buf : 结果送出/入的缓冲区.
 * 	op : bio 内要执行的操作.
 * 	op_flags : 操作所需附加的标志位.
 */
static int secondfs_submit_bio(struct block_device *bdev, sector_t sector,
			void *buf, int op, int op_flags)
{
	struct bio *bio;
	u64 io_size;
	int ret;

	bio = bio_alloc(GFP_NOIO, 1);
	bio->bi_iter.bi_sector = sector;
	bio_set_dev(bio, bdev);
	bio->bi_opf = op | op_flags;

	io_size = SECONDFS_BLOCK_SIZE;
	// bio 的目标区域, 必须以页为单位.
	
	// 多次调用 bio_add_page, 分次按页对齐完善 bio 请求
	while (io_size > 0) {
		// 首先计算目标区域 buf 的页框号, 在它所在页框的偏移, 不能超过页尾的本次最大读字节数
		unsigned int page_offset = offset_in_page(buf);
		unsigned int len = min_t(unsigned int, PAGE_SIZE - page_offset, io_size);

		ret = bio_add_page(bio, virt_to_page(buf), len, page_offset);
		if (ret != len) {
			ret = -EIO;
			goto out;
		}
		io_size -= len;
		buf = (u8 *)buf + len;
	}

	// 提交这个同步 bio 请求, 等待执行完毕
	ret = submit_bio_wait(bio);

out:
	bio_put(bio);
	return ret < 0 ? ret : 0;
}

int secondfs_submit_bio_sync_read(void * /* struct block_device * */ bdev, u32 sector,
			void *buf) {
	return secondfs_submit_bio(bdev, sector, buf, REQ_OP_READ, 0);
}

int secondfs_submit_bio_sync_write(void * /* struct block_device * */ bdev, u32 sector,
			void *buf) {
	return secondfs_submit_bio(bdev, sector, buf, REQ_OP_WRITE, REQ_SYNC);
}