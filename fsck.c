
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <time.h>

#include <linux/types.h>

#define SECONDFS_BLOCK_SIZE 512
#define SECONDFS_SB_FIRST_BLOCK 200
#define SECONDFS_INODE_FIRST_BLOCK 202
#define SECONDFS_DATA_FIRST_BLOCK 1024
#define SECONDFS_BLOCK_MIN_REQUIRED (SECONDFS_DATA_FIRST_BLOCK + 1)

#define LE32_PRE_INC(x) x = htole32(le32toh(x) + 1), le32toh(x)
#define LE32_POST_INC(x) x = htole32(le32toh(x) + 1), le32toh(x) - 1

#define LE32_PRE_DEC(x) x = htole32(le32toh(x) - 1), le32toh(x)
#define LE32_POST_DEC(x) x = htole32(le32toh(x) - 1), le32toh(x) + 1

typedef struct _fast_stack {
	__s32 count;
	__s32 stack[100];
} fast_stack;

typedef struct _DirectoryEntry{
	__u32 m_ino;		/* 目录项中Inode编号部分 */
	__u8 m_name[28];	/* 目录项中路径名部分 */
} DirectoryEntry;

typedef struct _SuperBlock
{
	__s32	s_isize;		/* 外存Inode区占用的盘块数 */
	__s32	s_fsize;		/* 盘块总数 */
	
	fast_stack s_free;
	
	fast_stack s_inode;
	
	//__s32	s_flock_obsolete;	/* 封锁空闲盘块索引表标志 */
	__s32	s_has_dots;		/* 我们用两个 lock 弃置后的空间来表示 "文件系统是否有 . 和 .. 目录项"吧. */
	__s32	s_ilock_obsolete;	/* 封锁空闲Inode表标志 */
	
	__s32	s_fmod;			/* 内存中super block副本被修改标志，意味着需要更新外存对应的Super Block */
	__s32	s_ronly;		/* 本文件系统只能读出 */
	__s32	s_time;			/* 最近一次更新时间 */
	__s32	padding[47];		/* 填充使SuperBlock块大小等于1024字节，占据2个扇区 */
} SuperBlock;

typedef struct _DiskInode
{
	__u32		d_mode;			/* 状态的标志位，定义见enum INodeFlag */
	__s32		d_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 */
	
	__u16		d_uid;			/* 文件所有者的用户标识数 */
	__u16		d_gid;			/* 文件所有者的组标识数 */
	
	__s32		d_size;			/* 文件大小，字节为单位 */
	__s32		d_addr[10];		/* 用于文件逻辑块号和物理块好转换的基本索引表 */

	__s32		d_atime;		/* 最后访问时间 */
	__s32		d_mtime;		/* 最后修改时间 */
} DiskInode;

static int read_only_flag = -1;
static int has_dots_flag = -1;
static int getopt_err = 0;

static struct option long_options[] = {
	{ "read-only",	no_argument, NULL, 'r' },
	{ "dots",	no_argument, NULL, 'd' },
	{ "no-dots",	no_argument, NULL, 'D' },
};

#define SFDBG_MKFS

#define eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#ifdef SFDBG_MKFS
#define sfdbg_pf(fmt, ...) printf(fmt, ##__VA_ARGS__);
#else // SFDBG_MKFS
#define sfdbg_pf(fmt, ...)
#endif // SFDBG_MKFS

static void show_usage(FILE *f, const char *argv0)
{
	fprintf(f, 
		"Usage: %s device\n"
		, argv0
	);
	
}

static int write_block(int fd, int block_no, const void *buf, int len)
{
	int ret;
	int curr_len = len;
	const char *p = buf;


	ret = lseek(fd, block_no * SECONDFS_BLOCK_SIZE, SEEK_SET);
	if (ret < 0) {
		return ret;
	}

	while (1) {
		ret = write(fd, p, curr_len);
		if (ret < 0) {
			return ret;
		}
		curr_len -= ret;
		p += ret;

		if (curr_len == 0) {
			return 0;
		}
	}
}

static int read_block(int fd, int block_no, void *buf, int len)
{
	int ret;
	int curr_len = len;
	char *p = buf;


	ret = lseek(fd, block_no * SECONDFS_BLOCK_SIZE, SEEK_SET);
	if (ret < 0) {
		return ret;
	}

	while (1) {
		ret = read(fd, p, curr_len);
		if (ret < 0) {
			return ret;
		}
		curr_len -= ret;
		p += ret;

		if (curr_len == 0) {
			return 0;
		}
	}
}

static int write_stack_in_block(int fd, int block_no, const fast_stack *buf)
{
	int len = sizeof(fast_stack); 
	return write_block(fd, block_no, buf, len);
}

static int read_stack_from_block(int fd, int block_no, fast_stack *buf)
{
	int len = sizeof(fast_stack); 
	return read_block(fd, block_no, buf, len);
}


int main(int argc, char **argv)
{
	int option_index = 0;
	int ret;
	int fd;
	int block_num;
	int actual_block_num;

	struct stat stat_buf;

	if (argc != 2) {
		eprintf("Error: invalid arguments number\n");
		show_usage(stderr, argv[0]);
		return EXIT_FAILURE;
	}

	// Open disk image
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		eprintf("Error opening file %s: %s\n", argv[optind], strerror(errno));
		return EXIT_FAILURE;
	}

	// Get image size
	ret = fstat(fd, &stat_buf);
	if (ret < 0) {
		eprintf("Error getting file stat: %s\n", strerror(errno));
		ret = errno;
		goto fclose_err;
	}

	block_num = stat_buf.st_size / SECONDFS_BLOCK_SIZE;

	printf("Device size: %d blocks\n", block_num);

	if (block_num < SECONDFS_BLOCK_MIN_REQUIRED) {
		eprintf("Error: device too small (required = %d blocks, device = %d blocks)\n", SECONDFS_BLOCK_MIN_REQUIRED, block_num);
		ret = ENOSPC;
		goto fclose_err;
	}

	SuperBlock sb_buf;
	bzero(&sb_buf, sizeof(sb_buf));

	if ((ret = read_block(fd, SECONDFS_SB_FIRST_BLOCK, &sb_buf, sizeof(sb_buf))) < 0) {
		eprintf("Error reading SuperBlock: %s\n", strerror(errno));
		goto fclose_err;
	}

	printf("s_fsize(Total blocks): %d\n", le32toh(sb_buf.s_fsize));
	printf("s_isize(Inode area blocks): %d\n", le32toh(sb_buf.s_isize));
	printf("s_nfree(Freeblock stack height): %d\n", le32toh(sb_buf.s_free.count));
	printf("s_ninode(Freeinode stack height): %d\n", le32toh(sb_buf.s_inode.count));

	printf("s_has_dots(This fs has . & ..?): 0x%X\n", le32toh(sb_buf.s_has_dots));

	printf("s_fmod(SuperBlock modified): %d\n", le32toh(sb_buf.s_fmod));

	printf("s_ronly(SuperBlock read-only): %d\n", le32toh(sb_buf.s_ronly));
	printf("s_time(Last update): %d\n", le32toh(sb_buf.s_time));
	
{
	fast_stack fast_stack_buf;

	fast_stack_buf = sb_buf.s_free;
	printf("Free fast stack in SuperBlock (top->bottom): ");

	while (1) {
		int nxt_ind_blkno;

		printf("[%d]", le32toh(fast_stack_buf.count));

		if (le32toh(fast_stack_buf.count) > 100) {
			printf("\n");
			eprintf("Error: current fast stack's height exceeds 100. Invalid image.\n");
			ret = EINVAL;
			goto fclose_err;
		}

		while (le32toh(fast_stack_buf.count)) {
			printf(" %d", le32toh(fast_stack_buf.stack[LE32_PRE_DEC(fast_stack_buf.count)]));
		}

		printf("\n");

		if ((nxt_ind_blkno = le32toh(fast_stack_buf.stack[0])) == 0) {
			printf("No free blocks anymore.\n");
			break;
		}

		if (nxt_ind_blkno >= le32toh(sb_buf.s_fsize)) {
			eprintf("Error: next index block exceeds the range of the volume.\n");
			ret = EINVAL;
			goto fclose_err;
		}

		if ((ret = read_stack_from_block(fd, nxt_ind_blkno, &fast_stack_buf)) < 0) {
			eprintf("Error reading from index block %d: %s\n", nxt_ind_blkno, strerror(errno));
			goto fclose_err;
		}

		printf("Free fast stack in data index block %d (top->bottom): ", nxt_ind_blkno);
	}
}

{
	fast_stack fast_stack_buf;
	int nxt_ind_blkno;

	fast_stack_buf = sb_buf.s_inode;
	printf("Free inode fast stack (top->bottom): ");

	printf("[%d]", le32toh(fast_stack_buf.count));

	if (le32toh(fast_stack_buf.count) > 100) {
		printf("\n");
		eprintf("Error: current fast stack's height exceeds 100. Invalid image.\n");
		ret = EINVAL;
		goto fclose_err;
	}

	while (1) {
		printf(" %d", le32toh(fast_stack_buf.stack[LE32_PRE_DEC(fast_stack_buf.count)]));
		if (le32toh(fast_stack_buf.count) == 0) {
			break;
		}
	}

	printf("\n");
}

	sfdbg_pf("Done!\n");
	close(fd);
	return ret;

fclose_err:
	close(fd);

	return ret;
}