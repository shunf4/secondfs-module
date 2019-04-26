#include "../utils.h"
#include <asm-generic/bug.h>
int __gxx_personality_v0;
int _Unwind_Resume;


void *operator new(unsigned long x) {
	// 这个是通用的 C++ new, 用 kmalloc 实现. 但对于每一个
	// C++ 数据结构我们最好还是用类内重载的 new/delete 用 kmem_cache实现.
	// 因此这里直接 BUG();
	BUG();
	return(secondfs_c_helper_malloc(x));
}

#if false
void operator delete(void *pointer) {
	secondfs_c_helper_free(pointer);
}
#endif

void operator delete(void *pointer, const long unsigned int type) {
	// 这个是通用的 C++ delete, 用 kmalloc 实现. 但对于每一个
	// C++ 数据结构我们最好还是用类内重载的 new/delete 用 kmem_cache实现.
	// 因此这里直接 BUG();
	BUG();
	secondfs_c_helper_free(pointer);
}
