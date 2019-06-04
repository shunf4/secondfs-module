# This module used this framework
# https://github.com/veltzer/kcpp

obj-m := secondfs.o
# https://mongonta.com/howto-build-custom-kernel/
LANG = C

# The project consists of C++ parts (compiled into $(cxxobjs))
# and C parts (compiled into $(objs)).
# They are later linked to form secondfs.ko
# 项目主要分为 C++ 部分 (cxxobjs) 和 C 部分 (objs). 它们都编译为 .o 文件, 最后整体链接在一起
secondfs-cxxobjs := UNIXV6PP/FileSystem.o UNIXV6PP/Inode.o UNIXV6PP/FileOperations.o UNIXV6PP/BufferManager.o UNIXV6PP/CCNewDelete.o
secondfs-objs := main.o super.o inode.o fileops.o c_helpers_for_cc.o bio.o

CFLAGS_DBG = -g -DDEBUG
ccflags-y += $(CFLAGS_DBG)

wrapper-headers = $(obj)/UNIXV6PP/FileOperations_c_wrapper.h $(obj)/UNIXV6PP/FileSystem_c_wrapper.h $(obj)/UNIXV6PP/Inode_c_wrapper.h $(obj)/UNIXV6PP/BufferManager_c_wrapper.h

# Avoid echoing of commands
# 加上 @ 前缀, 执行命令时可以不回显这条命令
Q = @

# Show all warnings & include common includes
# 显示所有警告信息
EXTRA_CFLAGS += -Wall -I/usr/include

# The main target
# all 目标, 主要构建目标
all : set_and_print_cxxflags kernmodule mkfs.secondfs fsck.secondfs

# Analysis the step in which kbuild builds the standard kernel module
# and fetch the compile options & flags into cxxflags.tmp
# hello.ko also contains sizeof() values of several kernel objects
# 模仿内核模块构建器 kbuild 构建 .c 文件的过程, 生成构建 C++ 源文件的参数, 存入 cxxflags.tmp
std_module/hello.ko :
	python2 ./make-utils/generate_cpp_compile_options.py $(shell uname -r) "" cxxflags.tmp

# Generate C++ compiler flags & options from standard kernel module
# 模仿内核模块构建器 kbuild 构建 .c 文件的过程, 生成构建 C++ 源文件的参数, 从 cxxflags.tmp 读取
# 然后设置 CXXMACROS (包含了一些 C 结构的大小) 和 CXXFLAGS 变量
set_and_print_cxxflags : std_module/hello.ko
	$(eval CXXMACROS := $(shell bash make-utils/read_sizes_from_hello_ko.sh))
	$(eval CXXFLAGS := $(shell cat cxxflags.tmp) -I/usr/src/linux-headers-$(shell uname -r)/include $(CXXMACROS))
	$(eval CXXFLAGS := $(CXXFLAGS) -std=c++14 $(CFLAGS_DBG))
	$(Q)echo CXXFLAGS = $(CXXFLAGS)

# Entry for the kernel module
#  Steps:
#  - Compile all C++ objects
#  - Compile the C part of the kernel module
#  - Link the C++ and kernel object
# 内核模块的构建入口
kernmodule : $(secondfs-cxxobjs)
	# 首先链接 C 语言的二进制对象文件: 进入系统内核模块构建目录, 执行 make 进行构建
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	# 接着链接 C++ 语言的二进制对象文件: 丢弃刚生成的 .ko 文件, 而是利用临时生成的 .mod.o (可能包含内核模块特有的符号) 文件, 与所有 C/C++ 的 .o 链接
	$(LD) -r --build-id -osecondfs.ko $(secondfs-objs) secondfs.mod.o $(secondfs-cxxobjs)

mkfs.secondfs : mkfs.c
	$(CC) -g -o$@ $^

fsck.secondfs : fsck.c
	$(CC) -g -o$@ $^

# 清理
clean:
	$(RM) cxxflags.tmp
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	$(RM) mkfs.secondfs

######### 下面是 C 或 C++ 实现的辅助功能/共同函数/入口点相关文件的编译 #########

# When compiling C parts, kbuild is called and current working directory(CWD)
# is shifted. Therefore all related files need prefix "$(obj)/".
# C++ parts are compiled directly, thus no need for "$(obj)/".
# C++ 的 .o 文件不在内核目录下 make, 而是在当前目录下, 所以不能加 $(obj);
# C 的 .o 文件在内核目录下 make, 所以要加 $(obj) 表示文件在源目录
UNIXV6PP/CCNewDelete.o : UNIXV6PP/CCNewDelete.cc common.h secondfs.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

$(obj)/c_helpers_for_cc.o : $(obj)/common.h $(obj)/secondfs.h

$(obj)/bio.o : $(obj)/secondfs.h $(wrapper-headers)

$(obj)/main.o : $(obj)/secondfs.h $(wrapper-headers)

######### 下面是超块相关模块 super.o(C)/FileSystem.o(C++) 的编译 #########

$(obj)/super.o : $(obj)/secondfs.h $(wrapper-headers)

UNIXV6PP/FileSystem.o : UNIXV6PP/FileSystem.cc UNIXV6PP/FileSystem.hh UNIXV6PP/FileSystem_c_wrapper.h common.h secondfs.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

######### 下面是 Inode 相关模块 inode.o(C)/Inode.o(C++) 的编译 #########

$(obj)/inode.o : $(obj)/secondfs.h $(wrapper-headers)

UNIXV6PP/Inode.o : UNIXV6PP/Inode.cc UNIXV6PP/Inode.hh UNIXV6PP/Inode_c_wrapper.h common.h secondfs.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

######### 下面是文件操作相关模块 fileops.o(C)/FileOperations.o(C++) 的编译 #########

$(obj)/fileops.o : $(obj)/secondfs.h $(wrapper-headers)

UNIXV6PP/FileOperations.o : UNIXV6PP/FileOperations.cc UNIXV6PP/FileOperations.hh UNIXV6PP/FileOperations_c_wrapper.h common.h secondfs.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

######### 下面是高速缓存相关模块 fileops.o(C)/FileOperations.o(C++) 的编译 #########

UNIXV6PP/BufferManager.o : UNIXV6PP/BufferManager.cc UNIXV6PP/BufferManager.hh UNIXV6PP/BufferManager_c_wrapper.h common.h secondfs.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)
