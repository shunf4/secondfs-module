#!/bin/bash
# Please call from the top directory
function get_size_from_const () {
	objcopy -j $1 -O binary ./std_module/hello.ko /dev/stdout|od -A n -v -t u4 | awk '{$1=$1;printf $1}'
}

echo -n "-D SECONDFS_SEMAPHORE_SIZE=" ; get_size_from_const semaphore_size
echo -n " -D SECONDFS_SPINLOCK_T_SIZE=" ; get_size_from_const spinlock_t_size
echo -n " -D SECONDFS_MUTEX_SIZE=" ; get_size_from_const mutex_size
echo -n " -D SECONDFS_INODE_SIZE=" ; get_size_from_const inode_size