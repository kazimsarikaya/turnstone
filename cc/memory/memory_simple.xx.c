/**
 * @file memory_simple.xx.c
 * @brief a simple heap implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <memory.h>
#include <systeminfo.h>
#include <cpu.h>
#include <video.h>
#include <cpu/sync.h>
#include <linker.h>
#include <utils.h>
#if ___TESTMODE == 1
#include <valgrind.h>
#endif

/*! heap flag for heap start and end hi */
#define HEAP_INFO_FLAG_STARTEND        (1 << 0)
/*! used heap hole flag */
#define HEAP_INFO_FLAG_USED            (1 << 1)
/*! unused heap hole flag */
#define HEAP_INFO_FLAG_NOTUSED         (0 << 1)
/*! heap metadata flag */
#define HEAP_INFO_FLAG_HEAP            (1 << 4)
/*! heap initilized flag */
#define HEAP_INFO_FLAG_HEAP_INITED     (HEAP_INFO_FLAG_HEAP | HEAP_INFO_FLAG_USED)
/*! heap magic for protection*/
#define HEAP_INFO_MAGIC                (0xaa55)
/*! heap padding for protection */
#define HEAP_INFO_PADDING              (0xaa55aa55)
/*! heap header */
#define HEAP_HEADER                    HEAP_INFO_PADDING


/*! heap bottom address comes frome linker script */
extern size_t __kheap_bottom;

/**
 * @struct __heapmetainfo
 * @brief heap info struct
 */
typedef struct __heapmetainfo {
	uint16_t magic; ///< magic value of heap for protection
	uint16_t flags; ///< flags of hi
	struct __heapinfo* first; ///< next hi node
	struct __heapinfo* last; ///< previous hi node
	struct __heapinfo* first_empty; ///< next hi node
	struct __heapinfo* last_full; ///< previous hi node
	struct {
		struct __heapinfo* head;
		struct __heapinfo* tail;
	} __attribute__ ((packed)) fast_classes[128];
	uint64_t malloc_count;
	uint64_t free_count;
	uint64_t total_size;
	uint64_t free_size;
	uint64_t fast_hit;
	uint32_t padding; ///< for 8 byte align for protection
}__attribute__ ((packed)) heapmetainfo_t; ///< short hand for struct

/**
 * @struct __heapinfo
 * @brief heap info struct
 */
typedef struct __heapinfo {
	uint16_t magic; ///< magic value of heap for protection
	uint16_t flags; ///< flags of hi
	uint64_t size; ///< size of slot
	struct __heapinfo* next; ///< next hi node
	struct __heapinfo* previous; ///< previous hi node
	uint32_t padding; ///< for 8 byte align for protection
}__attribute__ ((packed)) heapinfo_t; ///< short hand for struct

/**
 * @brief simple heap malloc implementation
 * @param[in] heap  simple heap (itself)
 * @param[in] size  size of memory
 * @param[in] align align value
 * @return allocated memory start address
 */
void* memory_simple_malloc_ext(memory_heap_t* heap, size_t size, size_t align);

/**
 * @brief simple heap free implementation
 * @param[in]  heap simple heap (itself)
 * @param[in]  address address to free
 * @return         0 if successed
 */
int8_t memory_simple_free(memory_heap_t* heap, void* address);

void memory_simple_stat(memory_heap_t* heap, memory_heap_stat_t* stat);

memory_heap_t* memory_create_heap_simple(size_t start, size_t end){
	size_t heap_start, heap_end;
	if(start == 0 || end == 0) {
		program_header_t* kernel = (program_header_t*)SYSTEM_INFO->kernel_start;

		if(kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_size) {
			heap_start = kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_start;
			heap_end = heap_start + kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_size;
		} else {
			heap_start = (size_t)&__kheap_bottom;
			heap_end = SYSTEM_INFO->kernel_start + SYSTEM_INFO->kernel_4k_frame_count * 0x1000;
		}
	} else {
		heap_start = start;
		heap_end = end;
	}

	PRINTLOG(SIMPLEHEAP, LOG_DEBUG, "heap boundaries 0x%lx 0x%lx", heap_start, heap_end);

	uint8_t* t_start = (uint8_t*)heap_start;
	memory_memclean(t_start, heap_end - heap_start);

	memory_heap_t* heap = (memory_heap_t*)(heap_start);

	size_t lock_start = heap_start + sizeof(memory_heap_t);
	heap->lock = (lock_t)lock_start;
	memory_heap_t** lock_heap = (memory_heap_t**)(lock_start); // black magic first element of lock is heap
	*lock_heap = heap;
	size_t metadata_start = lock_start + SYNC_LOCK_SIZE;

	if(metadata_start % 0x20) { //align 0x20
		metadata_start += 0x20 - (metadata_start % 0x20);
	}

	heapmetainfo_t* metadata = (heapmetainfo_t*)(metadata_start);

	heap->header = HEAP_HEADER;
	heap->metadata = metadata;
	heap->malloc = &memory_simple_malloc_ext;
	heap->free = &memory_simple_free;
	heap->stat = &memory_simple_stat;

	if((metadata->flags & HEAP_INFO_FLAG_HEAP_INITED) == HEAP_INFO_FLAG_HEAP_INITED) {
		PRINTLOG(SIMPLEHEAP, LOG_DEBUG, "heap already initialized between 0x%lx 0x%lx", heap_start, heap_end);

		return heap;
	}

	PRINTLOG(SIMPLEHEAP, LOG_DEBUG, "building internal structures between 0x%lx 0x%lx", heap_start, heap_end);

	size_t hibottom_start = metadata_start + sizeof(heapmetainfo_t);

	if(hibottom_start % 0x20) {
		hibottom_start += 0x20 - (hibottom_start % 0x20);
	}

	heapinfo_t* hibottom = (heapinfo_t*)(hibottom_start);
	heapinfo_t* hitop = (heapinfo_t*)(heap_end - sizeof(heapinfo_t));


	metadata->flags = HEAP_INFO_FLAG_HEAP;
	metadata->magic = HEAP_INFO_MAGIC;
	metadata->padding = HEAP_INFO_PADDING;
	metadata->first = hibottom;
	metadata->last = hitop;
	metadata->first_empty = hibottom;
	metadata->last_full = hitop;
	metadata->total_size = (uint64_t)(heap_end - hibottom_start - sizeof(heapinfo_t) * 2);
	metadata->free_size = metadata->total_size;

	hibottom->magic = HEAP_INFO_MAGIC;
	hibottom->padding = HEAP_INFO_PADDING;
	hibottom->next = NULL;
	hibottom->previous = NULL;
	hibottom->flags = HEAP_INFO_FLAG_STARTEND | HEAP_INFO_FLAG_NOTUSED;   // heapstart, empty;
	hibottom->size = (hitop - hibottom); // inclusive size as heapinfo_t because of alignment

	hitop->magic = HEAP_INFO_MAGIC;
	hitop->padding = HEAP_INFO_PADDING;
	hitop->next = NULL;
	hitop->previous =  NULL;
	hitop->flags = HEAP_INFO_FLAG_STARTEND | HEAP_INFO_FLAG_USED; //heaptop, full;
	hitop->size = 1;


	metadata->flags |= HEAP_INFO_FLAG_USED;

	PRINTLOG(SIMPLEHEAP, LOG_DEBUG, "heap created between 0x%lx 0x%lx", heap_start, heap_end);

	return heap;
}

void memory_simple_insert_sorted(heapmetainfo_t* heap, int8_t tofull, heapinfo_t* item) {
	if (tofull) {
		heapinfo_t* end = heap->last_full;

		while(1) {
			if(end->previous == NULL) {
				end->previous = item;
				item->next = end;
				break;
			}

			if(end->previous > item) {
				end = end->previous;
			} else {
				end->previous->next = item;
				item->previous = end->previous;
				item->next = end;
				end->previous = item;
				break;
			}
		}

	} else {
		heapinfo_t* prev = heap->first_empty;

		if (item < prev) {
			heap->first_empty = item;
			item->next = prev;
			prev->previous = item;

		} else {

			while(prev->next != NULL && prev->next < item) {
				prev = prev->next;
			}

			item->next = prev->next;
			prev->next = item;
			item->previous = prev;

			if(item->next) {
				item->next->previous = item;
			}
		}

	}
}

void* memory_simple_malloc_ext(memory_heap_t* heap, size_t size, size_t align){
	PRINTLOG(SIMPLEHEAP, LOG_TRACE, "requesting memory with size 0x%lx and align 0x%lx", size, align);

	heapmetainfo_t* simple_heap = (heapmetainfo_t*)heap->metadata;

	heapinfo_t* empty_hi = simple_heap->first_empty;

	size_t a_size = size;
	size_t t_size =  (a_size + sizeof(heapinfo_t) - 1) / sizeof(heapinfo_t);

	if(align <= sizeof(heapinfo_t) && (t_size - 1) < 128 && simple_heap->fast_classes[t_size - 1].head != NULL) {
		heapinfo_t* res = simple_heap->fast_classes[t_size - 1].head;

		simple_heap->fast_classes[t_size - 1].head = simple_heap->fast_classes[t_size - 1].head->next;

		if(simple_heap->fast_classes[t_size - 1].head) {
			simple_heap->fast_classes[t_size - 1].head->previous = NULL;
		}

		if(res == simple_heap->fast_classes[t_size - 1].tail) {
			simple_heap->fast_classes[t_size - 1].tail = NULL;
		}

		res->previous = NULL;
		res->next = NULL;
		res->flags |= HEAP_INFO_FLAG_USED;

#if ___TESTMODE == 1
		VALGRIND_MALLOCLIKE_BLOCK(res + 1, t_size * sizeof(heapinfo_t), 2, 1);
#endif

		PRINTLOG(SIMPLEHEAP, LOG_TRACE, "memory 0x%lp allocated with size 0x%lx", empty_hi + 1, t_size * sizeof(heapinfo_t));

		simple_heap->free_size -= (res->size - 1) * sizeof(heapinfo_t);
		simple_heap->malloc_count++;
		simple_heap->fast_hit++;

		return res + 1;
	}

	empty_hi = simple_heap->first_empty;

	//find first empty and enough slot
	while(1) { // size enough?
		if(align) {
			if(empty_hi->size == (t_size + 1)) {
				break;
			}
		} else {
			if(empty_hi->size >= (t_size + 1)) {
				break;
			}
		}

		heapinfo_t* empty_hi_t = empty_hi;
		empty_hi = empty_hi->next;

		if(empty_hi == NULL) {
			if(align == 0) {
				memory_heap_stat_t stat;
				memory_get_heap_stat(&stat);

				PRINTLOG(SIMPLEHEAP, LOG_ERROR, "memory stat ts 0x%lx fs 0x%lx mc 0x%lx fc 0x%lx diff 0x%lx", stat.total_size, stat.free_size, stat.malloc_count, stat.free_count, stat.malloc_count - stat.free_count);
				PRINTLOG(SIMPLEHEAP, LOG_ERROR, "no free slot 0x%lx 0x%lx 0x%lx", empty_hi_t, empty_hi_t->size * sizeof(heapinfo_t), empty_hi_t->flags);
				return NULL;
			}
			break;
		}
	}

	if(align) { // if align req, then check it
		int8_t need_check = 0;

		if(empty_hi == NULL) {
			need_check = 1;
		} else {
			size_t start_addr = (size_t)(empty_hi + 1);
			if(start_addr % align) {
				need_check = 1;
			}
		}

		if(need_check) { // not aligned let's search again

			empty_hi = simple_heap->first_empty;

			a_size = size + align;  // this time at align to the dize
			t_size =  (a_size + sizeof(heapinfo_t) - 1) / sizeof(heapinfo_t);

			//find first empty and enough slot
			while(1) { // size enough?
				if(empty_hi->size >= (t_size + 1)) {
					break;
				}

				empty_hi = empty_hi->next;

				if(empty_hi == NULL) {
					PRINTLOG(SIMPLEHEAP, LOG_ERROR, "no free slot for aligned", 0);
					return NULL;
				}
			}

		} else { // we found what we wanted, so send it
			if(empty_hi->previous) { // if we have previous
				empty_hi->previous->next = empty_hi->next; // set that's next to the empty_hi' next
			} else {
				simple_heap->first_empty = empty_hi->next; //update first_empty because we are removing first_empty
			}

			if(empty_hi->next) {
				empty_hi->next->previous = empty_hi->previous;
			}

			// cleanup pointers and set used
			empty_hi->previous = NULL;
			empty_hi->next = NULL;
			empty_hi->flags |= HEAP_INFO_FLAG_USED;

			//memory_simple_insert_sorted(simple_heap, 1, empty_hi); // add to full slot's list
#if ___TESTMODE == 1
			VALGRIND_MALLOCLIKE_BLOCK(empty_hi + 1, t_size * sizeof(heapinfo_t), 2, 1);
#endif

			PRINTLOG(SIMPLEHEAP, LOG_TRACE, "memory 0x%lp allocated with size 0x%lx", empty_hi + 1, t_size * sizeof(heapinfo_t));

			simple_heap->free_size -= (empty_hi->size - 1) * sizeof(heapinfo_t);
			simple_heap->malloc_count++;

			return empty_hi + 1;
		}
	}


	size_t rem = empty_hi->size - (t_size + 1);

	if(rem > 1) { // we slot should store at least one item (inclusive size)
		// we need divide slot
		heapinfo_t* empty_tmp = empty_hi + (1 + t_size); // (1+t_size) is our allocation size 1 for header t_size for request

		empty_tmp->magic =  HEAP_INFO_MAGIC;
		empty_tmp->padding = HEAP_INFO_PADDING;
		empty_tmp->flags = HEAP_INFO_FLAG_NOTUSED;

		empty_tmp->size = rem;

		empty_tmp->previous = empty_hi;
		empty_tmp->next = empty_hi->next;

		if(empty_tmp->next) {
			empty_tmp->next->previous = empty_tmp;
		}

		empty_hi->next = empty_tmp;

		empty_hi->size = 1 + t_size;     // new slot's size 1 for include header, t_size aligned requested size

		simple_heap->free_size -= sizeof(heapinfo_t); // meta occupies free area
	} // if we not we should keep slot's original size


	if (align == 0) { // if we donot need a alignment shortcut malloc remove empty_hi from list and append allocated list

		if(empty_hi->previous) { // if we have previous
			empty_hi->previous->next = empty_hi->next; // set that's next to the empty_hi' next
		} else {
			simple_heap->first_empty = empty_hi->next; //update first_empty because we are removing first_empty
		}

		if(empty_hi->next) {
			empty_hi->next->previous = empty_hi->previous;
		}

		// cleanup pointers and set used
		empty_hi->previous = NULL;
		empty_hi->next = NULL;
		empty_hi->flags |= HEAP_INFO_FLAG_USED;

		//memory_simple_insert_sorted(simple_heap, 1, empty_hi); // add to full slot's list
#if ___TESTMODE == 1
		VALGRIND_MALLOCLIKE_BLOCK(empty_hi + 1, t_size * sizeof(heapinfo_t), 2, 1);
#endif

		PRINTLOG(SIMPLEHEAP, LOG_TRACE, "memory 0x%lp allocated with size 0x%lx", empty_hi + 1, t_size * sizeof(heapinfo_t));

		simple_heap->free_size -= (empty_hi->size - 1) * sizeof(heapinfo_t);
		simple_heap->malloc_count++;

		return empty_hi + 1;
	}

	// now we have alignment problem empty_hi has enough area for alignment
	// we should find where to divide it into left real right
	size_t free_addr = (size_t)(empty_hi + 1);
	size_t aligned_addr = ((free_addr / align) + 1) * align;
	size_t hi_aligned_addr = aligned_addr - sizeof(heapinfo_t);
	int8_t right_exists = 0;
	size_t hi_a_size = size / sizeof(heapinfo_t);
	if(size % sizeof(heapinfo_t)) {
		hi_a_size++;
	}


	heapinfo_t* hi_a = (heapinfo_t*)hi_aligned_addr;                     // our rel addr

	// probable right side: at the end of hi_a
	heapinfo_t* hi_r = hi_a + hi_a_size + 1;


	if(empty_hi + empty_hi->size > hi_r + 1 ) {
		// we have empty space between hi_r and end of empty_hi, let's divide it

		hi_r->magic = HEAP_INFO_MAGIC;
		hi_r->padding = HEAP_INFO_PADDING;
		hi_r->flags = HEAP_INFO_FLAG_USED;
		hi_r->size = empty_hi + empty_hi->size - hi_r;

		hi_r->next = empty_hi->next;
		if(hi_r->next) {
			hi_r->next->previous = hi_r;
		}

		simple_heap->free_size -= sizeof(heapinfo_t);

		right_exists = 1; // set flag for setting previous of hi_r after left
	}


	// now let's look left side of hi_a, it is still empty_hi. if can contain at least one item let it go
	if(hi_a - empty_hi > 1) {
		// yes we have at least one area
		empty_hi->size = hi_a - empty_hi; // set its size

		if(empty_hi->previous == NULL) {
			simple_heap->first_empty = empty_hi;
		}

		if(right_exists) {
			// set empty_hi's right to hi_r
			empty_hi->next = hi_r;
			hi_r->previous = empty_hi;
		}

		simple_heap->free_size -= sizeof(heapinfo_t);
	} else {
		// ok hi_a is a dead area
		if(right_exists) {
			// let's link hi_r to empty_hi' previous
			if(empty_hi->previous) {
				hi_r->previous = empty_hi->previous;
				hi_r->previous->next = hi_r;
			} else { // empty_hi has not any prev so hi_r is first empty!!
				simple_heap->first_empty = hi_r;
			}

		} else {
			// no right not left hence link empty_hi' prev and next
			if(empty_hi->previous) {
				empty_hi->previous->next = empty_hi->next;
				empty_hi->next->previous = empty_hi->previous;
			} else { // empty_hi has not any prev so next is first empty!!
				simple_heap->first_empty = empty_hi->next;
				simple_heap->first_empty->previous = NULL;
			}

			memory_memclean(empty_hi, sizeof(heapinfo_t)); // cleanup data
		}

		simple_heap->free_size -= sizeof(heapinfo_t);
	}

	// memory_simple_insert_sorted(simple_heap, 1, hi_a); // add area used

	// fill ha
	hi_a->magic = HEAP_INFO_MAGIC;
	hi_a->padding = HEAP_INFO_PADDING;
	hi_a->size = hi_a_size + 1;   // inclusive size
	hi_a->flags = HEAP_INFO_FLAG_USED;



	#if ___TESTMODE == 1
	VALGRIND_MALLOCLIKE_BLOCK(aligned_addr, hi_a_size * sizeof(heapinfo_t), 2, 1);
	#endif

	PRINTLOG(SIMPLEHEAP, LOG_TRACE, "memory 0x%lp allocated with size 0x%lx", aligned_addr, hi_a_size * sizeof(heapinfo_t));

	simple_heap->free_size -= ( hi_a->size - 1) * sizeof(heapinfo_t);
	simple_heap->malloc_count++;

	return (uint8_t*)aligned_addr;

}


int8_t memory_simple_free(memory_heap_t* heap, void* address){
	if(address == NULL) {
		return -1;
	}

	heapmetainfo_t* simple_heap = (heapmetainfo_t*)heap->metadata;

	void* hibottom = simple_heap->first; // need as void*
	void* hitop = simple_heap->last; // need as void*

	if(address < hibottom || address >= hitop) {
		PRINTLOG(SIMPLEHEAP, LOG_FATAL, "memory 0x%lp not inside heap", address);

		return -1;
	}

	heapinfo_t* hi = ((heapinfo_t*)address) - 1;

	if(hi->magic != HEAP_INFO_MAGIC && hi->padding != HEAP_INFO_PADDING) {
		PRINTLOG(SIMPLEHEAP, LOG_FATAL, "memory 0x%lp broken", address);

		return -1;
	}

	if((hi->flags & HEAP_INFO_FLAG_USED) != HEAP_INFO_FLAG_USED) {
		return 0;
	}

	//clean data
	size_t size = (hi->size - 1) * sizeof(heapinfo_t); // get real exclusive size

	if((((uint8_t*)(address)) + size) >= (uint8_t*)hitop) {
		PRINTLOG(SIMPLEHEAP, LOG_FATAL, "memory 0x%lp with size 0x%lx not inside heap", address, size);

		return -1;
	}

	memory_memclean(address, size);

	hi->flags ^= HEAP_INFO_FLAG_USED;
	hi->previous = NULL;
	hi->next = NULL;

	simple_heap->free_count++;
	simple_heap->free_size += size;

	if((hi->size - 2) < 128) {
		if(simple_heap->fast_classes[hi->size - 2].tail == NULL) {
			simple_heap->fast_classes[hi->size - 2].tail = hi;
			simple_heap->fast_classes[hi->size - 2].head = hi;
		} else {
			hi->previous = simple_heap->fast_classes[hi->size - 2].tail;
			simple_heap->fast_classes[hi->size - 2].tail->next = hi;
			simple_heap->fast_classes[hi->size - 2].tail = hi;
		}
	} else {
		memory_simple_insert_sorted(simple_heap, 0, hi);
	}

	PRINTLOG(SIMPLEHEAP, LOG_TRACE, "memory 0x%lp freed with size 0x%lx", address, size);


#if ___TESTMODE == 1
	VALGRIND_FREELIKE_BLOCK(address, 0);
#endif

	return 0;
}

void memory_simple_stat(memory_heap_t* heap, memory_heap_stat_t* stat) {
	if(stat) {
		heapmetainfo_t* simple_heap = (heapmetainfo_t*)heap->metadata;

		stat->malloc_count = simple_heap->malloc_count;
		stat->free_count = simple_heap->free_count;
		stat->total_size = simple_heap->total_size;
		stat->free_size = simple_heap->free_size;
		stat->fast_hit = simple_heap->fast_hit;

	}
}
