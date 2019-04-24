obj-m := secondfs.o

secondfs-objs := main.o super_linked.o inode_linked.o fileops_linked.o

all : kernmodule mkfs

kernmodule:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

mkfs :
	# TODO:

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	# rm mkfs

$(obj)/common.o : $(obj)/secondfs_user.h

$(obj)/main.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/super_linked.o : $(obj)/super.o $(obj)/UNIXV6PP/FileSystem_cpp.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/super.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileSystem_c_wrapper.h

$(obj)/UNIXV6PP/FileSystem_cpp.o : $(obj)/UNIXV6PP/FileSystem.cpp $(obj)/UNIXV6PP/FileSystem.hpp $(obj)/UNIXV6PP/SecondFS.hpp $(obj)/UNIXV6PP/FileSystem_c_wrapper.h
	g++ -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/inode_linked.o : $(obj)/inode.o $(obj)/Inode_cpp.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/inode.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/Inode_c_wrapper.h

$(obj)/UNIXV6PP/Inode_cpp.o : $(obj)/UNIXV6PP/Inode.cpp $(obj)/UNIXV6PP/Inode.hpp $(obj)/UNIXV6PP/SecondFS.hpp $(obj)/UNIXV6PP/Inode_c_wrapper.h
	g++ -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/fileops_linked.o : $(obj)/fileops.o $(obj)/FileOperations_cpp.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/fileops.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileOperations_c_wrapper.h

$(obj)/UNIXV6PP/FileOperations_cpp.o : $(obj)/UNIXV6PP/FileOperations.cpp $(obj)/UNIXV6PP/FileOperations.hpp $(obj)/UNIXV6PP/SecondFS.hpp $(obj)/UNIXV6PP/FileOperations_c_wrapper.h
	g++ -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)


