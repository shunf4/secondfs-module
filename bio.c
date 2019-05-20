#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/mm.h>
#include <linux/types.h>

#include "secondfs.h"

/*
 * secondfs_submit_bio : 跳过系统为块设备准备的缓存, 直接操作块设备
 * 	Directly access block device (read/write) for 1 block(512 bytes)(Do bio).
 * 
 * 	The main procedure is to submit a bio request to generic
 * 	block layer and wait it to complete.
 * 	其核心为向通用块层直接提交一个 bio request, 并等待其结束.
 * 
 * 	sb : 当前文件系统的 VFS 超块指针. VFS super_block
 * 	sector : 扇区号. 一个扇区是, 且对齐到 512 字节. sector number
 * 	buf : 结果送出/入的缓冲区. The buffer to read from/write to. <= 512 bytes.
 * 	op : bio 内要执行的操作. the operation(read/write)
 * 	op_flags : 操作所需附加的标志位. the flags(0/sync)
 * 	rw : substitution for op/op_flags before kernel 4.8.
 */
#ifdef SECONDFS_KERNEL_BEFORE_4_8
static int secondfs_submit_bio(struct block_device *bdev, sector_t sector,
			void *buf, int rw)

#else
static int secondfs_submit_bio(struct block_device *bdev, sector_t sector,
			void *buf, int op, int op_flags)
#endif
{
	struct bio *bio;
	u64 io_size;
	int ret;

	bio = bio_alloc(GFP_NOIO, 1);
	bio->bi_iter.bi_sector = sector;
#ifdef SECONDFS_KERNEL_BEFORE_4_14
	bio->bi_bdev = bdev;
#else
	bio_set_dev(bio, bdev);
#endif

#ifdef SECONDFS_KERNEL_BEFORE_4_8
	bio->bi_rw = rw;
#else
	bio->bi_opf = op | op_flags;
#endif

	// Bytes to read/write.
	// bio 的目标区域, 必须以块大小为单位.
	io_size = SECONDFS_BLOCK_SIZE;
	
	// buf may cross 2 pages; in one loop, one page is processed
	// 多次调用 bio_add_page, 分次按页对齐完善 bio 请求
	while (io_size > 0) {
		// Get the page frame (physical) of buf and its offset in the frame
		// 首先计算目标区域 buf 的页框号, 在它所在页框的偏移, 不能超过页尾的本次最大读字节数
		unsigned int page_offset = offset_in_page(buf);
		unsigned int len = min_t(unsigned int, PAGE_SIZE - page_offset, io_size);

		secondfs_dbg(BUFFER, "submit_bio(): add <sector=%lu,page=%lu,len=%u,pageoffset=%u> to bio_add_page", sector, virt_to_page(buf), len, page_offset);

		ret = bio_add_page(bio, virt_to_page(buf), len, page_offset);

		secondfs_dbg(BUFFER, "submit_bio(): bio_add_page: ret=%d", ret);

		if (ret != len) {
			secondfs_err("submit_bio(): ret != len; error! return -EIO");
			ret = -EIO;
			goto out_eio;
		}
		io_size -= len;
		buf = (u8 *)buf + len;
	}

	// Wait it to complete
	// 提交这个同步 bio 请求, 等待执行完毕
#ifdef SECONDFS_KERNEL_BEFORE_4_8
	ret = submit_bio_wait(rw, bio);
#else
	ret = submit_bio_wait(bio);
#endif

out_eio:
	bio_put(bio);
	return ret < 0 ? ret : 0;
}

int secondfs_submit_bio_sync_read(void * /* struct block_device * */ bdev, u32 sector,
			void *buf) {
#ifdef SECONDFS_KERNEL_BEFORE_4_8
	return secondfs_submit_bio(bdev, sector, buf, READ);
#else
	return secondfs_submit_bio(bdev, sector, buf, REQ_OP_READ, 0);
#endif
}

int secondfs_submit_bio_sync_write(void * /* struct block_device * */ bdev, u32 sector,
			void *buf) {
#ifdef SECONDFS_KERNEL_BEFORE_4_8
	return secondfs_submit_bio(bdev, sector, buf, WRITE_SYNC);
#else
	return secondfs_submit_bio(bdev, sector, buf, REQ_OP_WRITE, REQ_SYNC);
#endif
}