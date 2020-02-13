asm (".code16gcc");
#include <memory.h>

#define HEAP_INFO_FLAG_STARTEND        (1<<0)
#define HEAP_INFO_FLAG_USED            (1<<1)
#define HEAP_INFO_FLAG_NOTUSED         (0<<1)
#define HEAP_INFO_MAGIC                (0xaa55)


extern unsigned char *__kheap_bottom;
extern unsigned char *__kheap_top;

typedef struct __heapinfo {
	int magic;
	char flags;
	struct __heapinfo *next;
	struct __heapinfo *previous;
}__attribute__ ((packed)) heapinfo;

heapinfo *hibottom = NULL;
heapinfo *hitop = NULL;

int init_simple_memory (){

	hibottom = (heapinfo*)&__kheap_bottom;
	hitop = (heapinfo*)(&__kheap_top-sizeof(heapinfo));

	hibottom->magic = HEAP_INFO_MAGIC;
	hitop->magic = HEAP_INFO_MAGIC;

	hibottom->flags = HEAP_INFO_FLAG_STARTEND | HEAP_INFO_FLAG_NOTUSED; // heapstart, empty;
	hitop->flags = HEAP_INFO_FLAG_STARTEND | HEAP_INFO_FLAG_USED; //heaptop, full;

	hibottom->next = hitop;
	hibottom->previous = NULL;

	hitop->next = NULL;
	hitop->previous = hibottom;

	return 0;
}

void *simple_kmalloc(unsigned int size){
	heapinfo *hi = hibottom;
	unsigned int t_size = size / sizeof(heapinfo);
	if ((size % sizeof(heapinfo)) != 0) {
		t_size++;
	}

	//find first empty and enough slot
	while((hi->flags&HEAP_INFO_FLAG_USED) ==  HEAP_INFO_FLAG_USED // empty?
	      || hi+1+t_size>hi->next) { // size enough?
		if( hi == hitop) {
			return NULL;
		}
		hi = hi->next;
	}

	heapinfo *tmp = hi+1+t_size;
	tmp->magic =  HEAP_INFO_MAGIC;
	tmp->flags = HEAP_INFO_FLAG_NOTUSED;
	tmp->previous = hi;
	tmp->next = hi->next;
	hi->next = tmp;
	hi->flags |= HEAP_INFO_FLAG_USED;

	return hi+1;
}


int simple_kfree(void *address){
	if(address==NULL) {
		return -1;
	}
	heapinfo *hi = ((heapinfo*)address)-1;
	if(hi->magic != HEAP_INFO_MAGIC) {
		//incorrect address to clean
		return -1;
	}

	if(hi->next == NULL) {
		// heap end, can not clean
		return -1;
	}
	hi->flags ^= HEAP_INFO_FLAG_USED;


	//clean data
	unsigned int size = (void*)hi->next-address;
	simple_memclean(address,size);

	//merge next empty slot if any
	if(!(hi->next->flags & HEAP_INFO_FLAG_USED)) {
		void *caddr = hi->next;
		hi->next = hi->next->next; //next cannot be NULL do not afraid of it
		simple_memclean(caddr,sizeof(heapinfo));
	}

	// merge previous empty slot if any
	if(hi->previous!=NULL) {
		if(!(hi->previous->flags & HEAP_INFO_FLAG_USED)) { // if prev is full then stop
			void *caddr = hi;
			hi->previous->next = hi->next;
			simple_memclean(caddr,sizeof(heapinfo));
		}
	}
	return 0;
}

void simple_memset(void *address,unsigned char value,unsigned int size) {
	for(unsigned int i=0; i<size; i++) {
		((unsigned char*)address)[i] = value;
	}
}
