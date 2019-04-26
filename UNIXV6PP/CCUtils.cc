#include "../utils.h"

int __gxx_personality_v0;
int _Unwind_Resume;


void *operator new(unsigned long x) {
	return(secondfs_c_helper_malloc(x));
}

void operator delete(void *pointer) {
	secondfs_c_helper_free(pointer);
}

#if false
void operator delete(void *pointer, const long unsigned int type) {
	secondfs_c_helper_free(pointer);
}
#endif