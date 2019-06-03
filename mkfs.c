
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
		"Usage: %s [-rdD] [<long-options>] device [block-count]\n"
		"\t-r, --read-only\tFormat as read-only filesystem (You need to modify the superblock manually to deactivate).\n"
		"\t-d, --dots\tFormat this filesystem as having dots(. & ..) in directory entry. No special effects other than taking more space in directory files.\n"
		"\t-D, --no-dots\tOpposition of -d.\n", argv0
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

static int write_stack_in_block(int fd, int block_no, const fast_stack *buf)
{
	int len = sizeof(fast_stack); 
	return write_block(fd, block_no, buf, len);
}

int main(int argc, char **argv)
{
	int option_index = 0;
	int ret;
	int fd;
	int block_num;
	int arg_block_num;

	struct stat stat_buf;

	while (1) {

		ret = getopt_long(argc, argv, "rdD", long_options, &option_index);

		if (ret == -1)
			break;

		switch (ret) {
		case 0:
			eprintf("Logic error.\n");
			break;

		case 'r':
			sfdbg_pf("Option -%c / --%s enabled.\n", (char)ret, long_options[option_index].name);
			if (read_only_flag != -1) {
				eprintf("Error: -%c / --%s enabled more than once.\n", (char)ret, long_options[option_index].name);
				getopt_err = 1;
				break;
			}
			read_only_flag = 1;
			break;

		case 'd':
			sfdbg_pf("Option -%c / --%s enabled.\n", (char)ret, long_options[option_index].name);
			if (has_dots_flag != -1) {
				eprintf("Error: -%c / --%s enabled/disabled more than once.\n", (char)ret, long_options[option_index].name);
				getopt_err = 1;
				break;
			}
			has_dots_flag = 1;
			break;

		case 'D':
			sfdbg_pf("Option -%c / --%s enabled.\n", (char)ret, long_options[option_index].name);
			if (has_dots_flag != -1) {
				eprintf("Error: -%c / --%s enabled/disabled more than once.\n", (char)ret, long_options[option_index].name);
				getopt_err = 1;
				break;
			}
			has_dots_flag = 0;
			break;

		case '?':
			if (isprint(optopt))
				eprintf("Error: unrecognized option - %c\n", optopt);
			else
				eprintf("Error: unrecognized option - \\x%x\n", optopt);
				
			show_usage(stderr, argv[0]);
			return EXIT_FAILURE;
			break;

		default:
			abort();
		}
	}

	if (read_only_flag == -1) {
		read_only_flag = 0;
	}

	if (has_dots_flag == -1) {
		has_dots_flag = 1;
	}

	sfdbg_pf("Read-only: %d, has-dots: %d\n", read_only_flag, has_dots_flag);

	if (optind != argc - 2 && optind != argc - 1) {
		eprintf("Error: invalid arguments number\n");
		show_usage(stderr, argv[0]);
		return EXIT_FAILURE;
	}

	// Open disk image
	fd = open(argv[optind], O_RDWR);
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
	if (block_num < SECONDFS_BLOCK_MIN_REQUIRED) {
		eprintf("Error: device too small (required = %d blocks, device = %d blocks)\n", SECONDFS_BLOCK_MIN_REQUIRED, block_num);
		ret = ENOSPC;
		goto fclose_err;
	}


	if (optind == argc - 2) {
		arg_block_num = atoi(argv[argc - 1]);
		if (arg_block_num < SECONDFS_BLOCK_MIN_REQUIRED || arg_block_num > block_num) {
			eprintf("Error: argument block-count inproper (block-count = %d blocks, device = %d blocks, minimum = %d blocks)\n", arg_block_num, block_num, SECONDFS_BLOCK_MIN_REQUIRED);
			ret = EINVAL;
			goto fclose_err;
		}
	} else {
		arg_block_num = block_num;
	}

	sfdbg_pf("File: %s, blocks: %d\n", argv[optind], arg_block_num);

	SuperBlock sb_buf;
	bzero(&sb_buf, sizeof(sb_buf));
{
	fast_stack fast_stack_buf;

	int remain_data_block_num;
	int last_index_data_block_no = 0;
	int first_time = 1;
	int curr_group_size;

	remain_data_block_num = arg_block_num - SECONDFS_DATA_FIRST_BLOCK - 1;

	while (1) {
		int host_count;

		fast_stack_buf.count = 0;
		int group_max_size = first_time ? 99 : 100;

		if (first_time)
			fast_stack_buf.stack[LE32_POST_INC(fast_stack_buf.count)] = htole32(0);
		
		curr_group_size = remain_data_block_num < group_max_size ? remain_data_block_num : group_max_size;

		for (int i = 0; i < curr_group_size; i++) {
			fast_stack_buf.stack[LE32_POST_INC(fast_stack_buf.count)] = htole32(remain_data_block_num - i + SECONDFS_DATA_FIRST_BLOCK);
		}

		fast_stack_buf.count = htole32(first_time ? curr_group_size + 1 : curr_group_size);

		if (first_time) {
			first_time = 0;
		}

		remain_data_block_num -= curr_group_size;

		if (remain_data_block_num == 0) {
			sfdbg_pf("Write into SuperBlock: ");
			sb_buf.s_free = fast_stack_buf;
		} else {
			last_index_data_block_no = remain_data_block_num;
			sfdbg_pf("Write into physical block %d (data block %d): ", last_index_data_block_no + SECONDFS_DATA_FIRST_BLOCK, last_index_data_block_no);
			ret = write_stack_in_block(fd, last_index_data_block_no + SECONDFS_DATA_FIRST_BLOCK, &fast_stack_buf);
			if (ret < 0) {
				eprintf("Error writing data block %d: %s\n", last_index_data_block_no + SECONDFS_DATA_FIRST_BLOCK, strerror(errno));
				goto fclose_err;
			}
		}

		host_count = le32toh(fast_stack_buf.count);

		sfdbg_pf("[%d]", host_count);

		for (int i = 0; i < host_count; i++) {
			sfdbg_pf(" %d", le32toh(fast_stack_buf.stack[i]));
		}

		sfdbg_pf("\n");

		if (remain_data_block_num == 0)
			break;
	}
}

	// Write SuperBlock
	
	sb_buf.s_isize = htole32(SECONDFS_DATA_FIRST_BLOCK - SECONDFS_INODE_FIRST_BLOCK);
	sb_buf.s_fsize = htole32(arg_block_num);

	sb_buf.s_inode.count = htole32(100);
	for (int i = 0; i < 100; i++)
		sb_buf.s_inode.stack[i] = htole32(100 - i);

	sb_buf.s_has_dots = htole32(has_dots_flag ? 0xFFFFFFFF : 0);
	
	sb_buf.s_ronly = htole32(read_only_flag ? 1 : 0);
	sb_buf.s_time = htole32((__s32)time(NULL));

	sfdbg_pf("Writing SuperBlock...\n");

	if (write_block(fd, SECONDFS_SB_FIRST_BLOCK, &sb_buf, sizeof(sb_buf)) < 0) {
		eprintf("Error writing SuperBlock: %s\n", strerror(errno));
		goto fclose_err;
	}

	// Reset i_number of all inodes
	char zero[SECONDFS_BLOCK_SIZE] = {0};
	sfdbg_pf("Writing Inode area...\n");
	for (int blkno = SECONDFS_INODE_FIRST_BLOCK; blkno < SECONDFS_DATA_FIRST_BLOCK; blkno++) {
		if (write_block(fd, blkno, zero, sizeof(zero)) < 0) {
			eprintf("Error writing Inode block: %s\n", strerror(errno));
			goto fclose_err;
		}
	}

	// Write root inode and root directory file (Inode 0, Data block 0)
	DiskInode di;
	bzero(&di, sizeof(di));
	di.d_addr[0] = htole32(SECONDFS_DATA_FIRST_BLOCK);
	di.d_atime = htole32((__s32)time(NULL));
	di.d_gid = htole16(0);
	di.d_mode = htole32(0x8000 | 0x4000 | (0x100 | 0x80 | 0x40) | (0x100 | 0x80 | 0x40) >> 3 | (0x100 | 0x80 | 0x40) >> 6);
	di.d_mtime = htole32((__s32)time(NULL));
	di.d_nlink = htole32(has_dots_flag ? 2 : 1);
	di.d_size = htole32(has_dots_flag ? (2 * sizeof(DirectoryEntry)) : 0);
	di.d_uid = htole16(0);

	if (write_block(fd, SECONDFS_INODE_FIRST_BLOCK, &di, sizeof(di)) < 0) {
		eprintf("Error writing root Inode: %s\n", strerror(errno));
		goto fclose_err;
	}

	char block[512] = {0};

	if (has_dots_flag) {
		DirectoryEntry de[2];
		bzero(&de, sizeof(de));

		// Actually "." & ".." should point to the root dir itself
		// (ino == 0), but ino == 0 indicates this DE is not used
		// for now, so we set it to 0xFFFFFFFF. In secondfs, we 
		// will never read this two inos
		de[0].m_ino = htole32(0xFFFFFFFF);
		memcpy(de[0].m_name, ".", 2);
		de[1].m_ino = htole32(0xFFFFFFFF);
		memcpy(de[1].m_name, "..", 3);
		
		memcpy(block, de, sizeof(de));
	}

	if (write_block(fd, SECONDFS_DATA_FIRST_BLOCK, block, sizeof(block)) < 0) {
		eprintf("Error writing root directory file: %s\n", strerror(errno));
		goto fclose_err;
	}

	sfdbg_pf("Done!\n");
	close(fd);
	return ret;

fclose_err:
	close(fd);

	return ret;
}