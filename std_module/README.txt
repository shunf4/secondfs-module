This module is here as a standard module,
so that we can analyze the flags needed
in the compilation of the C++ parts of the module.

Also, some kernel data structures' size is stored in hello.ko
when compiling(see main.c). By analyzing constant symbols
of hello.ko, we get the exact sizes of those data structures'
size to use in the C++ linking.

这个模块用于为我们主模块的 C++ 部分编译选项提供参考.
在主 Makefile 中, 会调用本标准模块的 Makefile 进行构建
并记录下 C 的编译选项. 接着将这些选项经过 make-utils/
generate_cpp_compile_options.py 过滤得到 C++ 的编译选项.

此外, main.c 将一些内核结构的 sizeof() 记录为常量. 编译后,
这些常量会在 hello.ko 内的相应段, 可供分析, 提供给 C++ 的链接
使用. (因为担心内核结构的大小在不同 Linux 机器上可能会不一样)
