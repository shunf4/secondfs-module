/* 一些快捷定义 */
#define SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR(classname) \
const size_t SECONDFS_SIZEOF_##classname = sizeof(classname);\
classname *new##classname() {\
	return new classname();\
}\
void delete##classname(classname *p) {\
	delete p;\
}

#define SECONDFS_QUICK_WRAP_CONSTRUCTOR_DECONSTRUCTOR_DECLARATION(classname) \
extern const size_t SECONDFS_SIZEOF_##classname;\
classname *new##classname(void);\
void delete##classname(classname *);
