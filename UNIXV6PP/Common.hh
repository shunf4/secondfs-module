#ifndef __COMMON_HH__
#define __COMMON_HH__

/* 一些快捷定义 */
#define SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR(classname) \
const u32 SECONDFS_SIZEOF_##classname = sizeof(classname);\
classname *new##classname() {\
	return new classname();\
}\
void delete##classname(classname *p) {\
	delete p;\
}

#define SECONDFS_QUICK_WRAP_CONSTRUCTOR_DESTRUCTOR_DECLARATION(classname)\
extern const u32 SECONDFS_SIZEOF_##classname;\
classname *new##classname (void);\
void delete##classname (classname *);\

#endif // __COMMON_HH__