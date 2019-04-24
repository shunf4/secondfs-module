obj-m := secondfs.o

# secondfs-objs := main.o super_linked.o inode_linked.o fileops_linked.o
secondfs-cxxobjs := UNIXV6PP/FileSystem.o UNIXV6PP/Inode.o UNIXV6PP/FileOperations.o UNIXV6PP/CCUtils.o
secondfs-objs := main.o super.o inode.o fileops.o utils.o $(secondfs-cxxobjs)

CXXFLAGS += -Wall -Wundef -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -Werror-implicit-function-declaration -Wno-format-security -fno-PIE -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m64 -falign-jumps=1 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel=kernel -funit-at-a-time -pipe -Wno-sign-compare -fno-asynchronous-unwind-tables -mindirect-branch=thunk-extern -mindirect-branch-register -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-int-in-bool-context -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -mfentry -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wextra -Wunused -Wno-unused-parameter -Wmissing-declarations -Wmissing-format-attribute -Wmissing-include-dirs -Wunused-but-set-variable -Wunused-const-variable -Wno-missing-field-initializers -Wno-sign-compare

all : kernmodule mkfs

kernmodule : $(secondfs-cxxobjs)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

mkfs :
	# TODO:

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	# rm mkfs

$(obj)/UNIXV6PP/CCUtils.o : $(obj)/UNIXV6PP/CCUtils.cc $(obj)/UNIXV6PP/CCUtils.hh $(obj)/utils.h
	$(CXX) $(CXXFLAGS) -o$@ $(filter-out %.h %.hpp, $^)

$(obj)/utils.o : $(obj)/utils.h

$(obj)/common.o : $(obj)/secondfs_user.h

$(obj)/main.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
# 修改 : 所有的 _linked.o 暂时废弃
$(obj)/super_linked.o : $(obj)/super.o $(obj)/UNIXV6PP/FileSystem.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/super.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileSystem_c_wrapper.h

$(obj)/UNIXV6PP/FileSystem.o : $(obj)/UNIXV6PP/FileSystem.cc $(obj)/UNIXV6PP/FileSystem.hh $(obj)/UNIXV6PP/SecondFS.hh $(obj)/UNIXV6PP/FileSystem_c_wrapper.h
	$(CXX) $(CXXFLAGS) -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/inode_linked.o : $(obj)/inode.o $(obj)/UNIXV6PP/Inode.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/inode.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/Inode_c_wrapper.h

$(obj)/UNIXV6PP/Inode.o : $(obj)/UNIXV6PP/Inode.cc $(obj)/UNIXV6PP/Inode.hh $(obj)/UNIXV6PP/SecondFS.hh $(obj)/UNIXV6PP/Inode_c_wrapper.h
	$(CXX) $(CXXFLAGS) -o$@ $(filter-out %.h %.hpp, $^)

# 此处使用 -r, 将两个 .o 文件合成为一个 .o
$(obj)/fileops_linked.o : $(obj)/fileops.o $(obj)/UNIXV6PP/FileOperations.o
	$(LD) $(LDFLAGS) -r -o$@ $^

$(obj)/fileops.o : $(obj)/secondfs_kern.h $(obj)/secondfs_user.h $(obj)/UNIXV6PP/FileOperations_c_wrapper.h

$(obj)/UNIXV6PP/FileOperations.o : $(obj)/UNIXV6PP/FileOperations.cc $(obj)/UNIXV6PP/FileOperations.hh $(obj)/UNIXV6PP/SecondFS.hh $(obj)/UNIXV6PP/FileOperations_c_wrapper.h
	$(CXX) $(CXXFLAGS) -o$@ $(filter-out %.h %.hpp, $^)

