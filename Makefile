obj-m := secondfs.o

# 项目主要分为 C++ 部分 (cxxobjs) 和 C 部分 (objs). 它们都编译为 .o 文件, 最后整体链接在一起
secondfs-cxxobjs := UNIXV6PP/FileSystem.o UNIXV6PP/Inode.o UNIXV6PP/FileOperations.o UNIXV6PP/BufferManager.o UNIXV6PP/CCNewDelete.o
secondfs-objs := main.o super.o inode.o fileops.o buffer.o c_helper_for_cc.o

wrapper-headers = $(obj)/UNIXV6PP/FileOperations_c_wrapper.h $(obj)/UNIXV6PP/FileSystem_c_wrapper.h $(obj)/UNIXV6PP/Inode_c_wrapper.h $(obj)/UNIXV6PP/BufferManager_c_wrapper.h

# 加上 @ 前缀, 执行命令时可以不回显这条命令
Q = @

# 显示所有警告信息
EXTRA_CFLAGS += -Wall -I/usr/include

# all 目标, 主要构建目标
all : set_and_print_cxxflags kernmodule mkfs

# 模仿内核模块构建器 kbuild 构建 .c 文件的过程, 生成构建 C++ 源文件的参数, 存入 cxxflags.tmp
cxxflags.tmp :
	python ./make-utils/generate_cpp_compile_options.py "" cxxflags.tmp

# 模仿内核模块构建器 kbuild 构建 .c 文件的过程, 生成构建 C++ 源文件的参数, 从 cxxflags.tmp 读取
# 然后设置 CXXFLAGS 变量
set_and_print_cxxflags : cxxflags.tmp
	$(eval CXXFLAGS := $(shell cat cxxflags.tmp) -Wall)
	$(Q)echo CXXFLAGS = $(CXXFLAGS)

# 内核模块的构建入口
kernmodule : $(secondfs-cxxobjs)
	# 首先链接 C 语言的二进制对象文件: 进入系统内核模块构建目录, 执行 make 进行构建
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	# 接着链接 C++ 语言的二进制对象文件: 丢弃刚生成的 .ko 文件, 而是利用临时生成的 .mod.o (可能包含内核模块特有的符号) 文件, 与所有 C/C++ 的 .o 链接
	$(LD) -r --build-id -osecondfs.ko $(secondfs-objs) secondfs.mod.o $(secondfs-cxxobjs)

mkfs :
	# mkfs - TODO

# 清理
clean:
	$(RM) cxxflags.tmp
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	# rm mkfs

######### 下面是 C 或 C++ 实现的辅助功能/共同函数/入口点相关文件的编译 #########

# C++ 的 .o 文件不在内核目录下 make, 所以不能加 $(obj); C 的 .o 文件在内核目录下 make, 所以要加 $(obj) 表示文件在源目录
UNIXV6PP/CCNewDelete.o : UNIXV6PP/CCNewDelete.cc c_helper_for_cc.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

$(obj)/c_helper_for_cc.o : $(obj)/c_helper_for_cc.h

$(obj)/common.o : $(obj)/secondfs_user.h $(wrapper-headers)

$(obj)/main.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(wrapper-headers)

######### 下面是超块相关模块 super.o(C)/FileSystem.o(C) 的编译 #########

$(obj)/super.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(wrapper-headers) common_c_cpp_types.h

UNIXV6PP/FileSystem.o : UNIXV6PP/FileSystem.cc UNIXV6PP/FileSystem.hh UNIXV6PP/FileSystem_c_wrapper.h common_c_cpp_types.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

######### 下面是 Inode 相关模块 inode.o(C)/Inode.o(C) 的编译 #########

$(obj)/inode.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(wrapper-headers) common_c_cpp_types.h

UNIXV6PP/Inode.o : UNIXV6PP/Inode.cc UNIXV6PP/Inode.hh UNIXV6PP/Inode_c_wrapper.h common_c_cpp_types.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

######### 下面是文件操作相关模块 fileops.o(C)/FileOperations.o(C) 的编译 #########

$(obj)/fileops.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(wrapper-headers) common_c_cpp_types.h

UNIXV6PP/FileOperations.o : UNIXV6PP/FileOperations.cc UNIXV6PP/FileOperations.hh UNIXV6PP/FileOperations_c_wrapper.h common_c_cpp_types.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)

######### 下面是高速缓存相关模块 fileops.o(C)/FileOperations.o(C) 的编译 #########

$(obj)/buffer.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(wrapper-headers) common_c_cpp_types.h

UNIXV6PP/BufferManager.o : UNIXV6PP/BufferManager.cc UNIXV6PP/BufferManager.hh UNIXV6PP/BufferManager_c_wrapper.h common_c_cpp_types.h
	$(Q)$(CXX) $(CXXFLAGS) -c -o$@ $(filter-out %.h %.hh, $^)
