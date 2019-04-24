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

$(obj)/common.o : $(obj)/common.c $(obj)/secondfs_user.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

$(obj)/main.o : $(obj)/main.c $(obj)/secondfs_kern.h $(obj)/secondfs_user.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/super.o : $(obj)/super_c.o $(obj)/UNIXV6PP/FileSystem_cpp.o
	$(LD) $(LDFLAGS) -r -o$@

$(obj)/super_c.o : $(obj)/super.c $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileSystem_c_wrapper.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

$(obj)/FileSystem_cpp.o : $(obj)/UNIXV6PP/FileSystem.cpp $(obj)/UNIXV6PP/FileSystem.hpp $(obj)/UNIXV6PP/SecondFS.hpp $(obj)/UNIXV6PP/FileSystem_c_wrapper.h
	$(CXX) $(CXXFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/inode.o : $(obj)/inode_c.o $(obj)/Inode_cpp.o
	echo "Combining inode";
	$(LD) $(LDFLAGS) -r -o$@

$(obj)/inode_c.o : $(obj)/inode.c $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/Inode_c_wrapper.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

$(obj)/Inode_cpp.o : $(obj)/UNIXV6PP/Inode.cpp $(obj)/UNIXV6PP/Inode.hpp $(obj)/UNIXV6PP/SecondFS.hpp $(obj)/UNIXV6PP/Inode_c_wrapper.h
	$(CXX) $(CXXFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/fileops.o : $(obj)/fileops_c.o $(obj)/FileOperations_cpp.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/fileops_c.o : $(obj)/fileops.c $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileOperations_c_wrapper.h
	$(CC) $(CFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)

$(obj)/FileOperations_cpp.o : $(obj)/UNIXV6PP/FileOperations.cpp $(obj)/UNIXV6PP/FileOperations.hpp $(obj)/UNIXV6PP/SecondFS.hpp $(obj)/UNIXV6PP/FileOperations_c_wrapper.h
	$(CXX) $(CXXFLAGS) -c -DDEBUG -o$@ $(filter-out %.h %.hpp, $^)


