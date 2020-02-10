#ifndef ___MEMORY_H
#define ___MEMORY_H 0

#ifndef NULL
#define NULL 0
#endif

int init_simple_memory();
void *simple_kmalloc(unsigned int);
int simple_kfree(void*);
void simple_memset(void*,unsigned char,unsigned int);
#define simple_memclean(addr,size) simple_memset(addr,NULL,size)

#endif
