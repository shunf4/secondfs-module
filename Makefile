obj-m := secondfs.o

secondfs-objs := main.o super.o inode.o fileops.o

all : kernmodule mkfs

kernmodule:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

mkfs :
	# TODO:

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	# rm mkfs

common.o : common.c secondfs_user.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

main.o : main.c secondfs_kern.h secondfs_user.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
super.o : super_c.o UNIXV6PP/FileSystem_cpp.o
	$(LD) $(LDFLAGS) -r -o$@

super_c.o : super.c secondfs_kern.h secondfs_user.h UNIXV6PP/FileSystem_c_wrapper.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

FileSystem_cpp.o : UNIXV6PP/FileSystem.cpp UNIXV6PP/FileSystem.hpp UNIXV6PP/SecondFS.hpp UNIXV6PP/FileSystem_c_wrapper.h
	$(CXX) $(CXXFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
inode.o : inode_c.o INode_cpp.o
	$(LD) $(LDFLAGS) -r -o$@

inode_c.o : inode.c secondfs_kern.h secondfs_user.h UNIXV6PP/INode_c_wrapper.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

INode_cpp.o : UNIXV6PP/INode.cpp UNIXV6PP/INode.hpp UNIXV6PP/SecondFS.hpp UNIXV6PP/INode_c_wrapper.h
	$(CXX) $(CXXFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
fileops.o : fileops_c.o FileOperations_cpp.o
	$(LD) $(LDFLAGS) -r -o$@ $^

fileops_c.o : fileops.c secondfs_kern.h secondfs_user.h UNIXV6PP/FileOperations_c_wrapper.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

FileOperations_cpp.o : UNIXV6PP/FileOperations.cpp UNIXV6PP/FileOperations.hpp UNIXV6PP/SecondFS.hpp UNIXV6PP/FileOperations_c_wrapper.h
	$(CXX) $(CXXFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)


