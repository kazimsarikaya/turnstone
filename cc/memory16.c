#include <memory.h>

#define HEAP_INFO_FLAG_STARTEND        (1<<0)
#define HEAP_INFO_FLAG_USED            (1<<1)
#define HEAP_INFO_FLAG_NOTUSED         (0<<1)
#define HEAP_INFO_MAGIC                (0xaa55)


extern uint8_t *__kheap_bottom;
extern uint8_t *__kheap_top;

typedef struct __heapinfo {
	uint16_t magic;
	uint8_t flags;
	struct __heapinfo *next;
	struct __heapinfo *previous;
}__attribute__ ((packed)) heapinfo;

heapinfo *hibottom = NULL;
heapinfo *hitop = NULL;

uint8_t init_simple_memory (){

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

void *simple_kmalloc(size_t size){
	heapinfo *hi = hibottom;
	size_t t_size = size / sizeof(heapinfo);
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
	simple_memclean(hi+1, size);
	return hi+1;
}


uint8_t simple_kfree(void *address){
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
	size_t size = (void*)hi->next-address;
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

void simple_memset(void *address,uint8_t value,size_t size) {
	for(size_t i=0; i<size; i++) {
		((uint8_t*)address)[i] = value;
	}
}

size_t detect_memory(memory_map_t** mmap) {
	*mmap = simple_kmalloc(sizeof(memory_map_t*)*MMAP_MAX_ENTRY_COUNT);
	memory_map_t * mmap_a = *mmap;
	regext_t contID = {0,0},signature, bytes;
	size_t entries = 0;
	do {
		__asm__ __volatile__ ("int $0x15"
		                      : "=a" (signature), "=c" (bytes), "=b" (contID)
		                      : "a" (0xE820), "b" (contID), "c" (24), "d" (0x534D4150), "D" (mmap_a));
		if(signature.part_high != 0x534D && signature.part_low != 0x4150) {
			return -1;
		}
		if(bytes.part_high == 0 && bytes.part_low > 20 && mmap_a->acpi.part_low & 0x0001) {

		}
		else{
			mmap_a++;
			entries++;
		}
	} while(contID.part_low !=0 && entries<MMAP_MAX_ENTRY_COUNT);

	return entries;
}
