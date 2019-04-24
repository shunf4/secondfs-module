obj-m := secondfs.o

# secondfs-objs := main.o super_linked.o inode_linked.o fileops_linked.o
secondfs-objs := main.o super.o inode.o fileops.o UNIXV6PP/FileSystem.o UNIXV6PP/Inode.o UNIXV6PP/FileOperations.o

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
# 修改 : 所有的 _linked.o 暂时废弃
$(obj)/super_linked.o : $(obj)/super.o $(obj)/UNIXV6PP/FileSystem.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/super.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileSystem_c_wrapper.h

$(obj)/UNIXV6PP/FileSystem.o : $(obj)/UNIXV6PP/FileSystem.hh $(obj)/UNIXV6PP/SecondFS.hh $(obj)/UNIXV6PP/FileSystem_c_wrapper.h

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/inode_linked.o : $(obj)/inode.o $(obj)/UNIXV6PP/Inode.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/inode.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/Inode_c_wrapper.h

$(obj)/UNIXV6PP/Inode.o : $(obj)/UNIXV6PP/Inode.hh $(obj)/UNIXV6PP/SecondFS.hh $(obj)/UNIXV6PP/Inode_c_wrapper.h
	$(kecho) "Inode C++"

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/fileops_linked.o : $(obj)/fileops.o $(obj)/UNIXV6PP/FileOperations.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/fileops.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileOperations_c_wrapper.h

$(obj)/UNIXV6PP/FileOperations.o : $(obj)/UNIXV6PP/FileOperations.hh $(obj)/UNIXV6PP/SecondFS.hh $(obj)/UNIXV6PP/FileOperations_c_wrapper.h

