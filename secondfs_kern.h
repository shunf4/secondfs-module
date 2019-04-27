#ifndef __SECONDFS_KERN_H__
#define __SECONDFS_KERN_H__

/* 引入 Linux 头文件, 声明需要作为内部文件系统操作函数指针的函数原型, 变量 */
/* Includes Linux header files and declare kernel filesystem functions and variables. */

#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/parser.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/version.h>

/* 内核函数也依赖于 SecondFS 自定义的结构 */
#include "secondfs_user.h"

extern const struct super_operations secondfs_sb_ops;

#endif // __SECONDFS_KERN_H__