/**
 * @file memory.h
 * @brief memory functions
 */
#ifndef ___MEMORY_H
/*! prevent duplicate header error macro */
#define ___MEMORY_H 0

#include <types.h>

typedef void* lock_t;
/**
 * @struct memory_heap
 * @brief heap interface for all types
 */
typedef struct memory_heap {
	uint32_t header; ///< heap header custom values
	void* metadata; ///< internal heap metadata filled by heap implementation
	void* (* malloc)(struct memory_heap*, size_t, size_t); ///< malloc function of heap implementation
	int8_t (* free)(struct memory_heap*, void*); ///< free function of heap implementation
	lock_t lock;
} memory_heap_t; ///< short hand for struct

/**
 * @brief creates simple heap
 * @param[in]  start start address of heap
 * @param[in]  end   end address of heap
 * @return       heap
 */
memory_heap_t* memory_create_heap_simple(size_t start, size_t end);

/**
 * @brief sets default heap
 * @param[in]  heap the heap will be the default one
 * @return old default heap
 */
memory_heap_t* memory_set_default_heap(memory_heap_t* heap);

/**
 * @brief malloc memory
 * @param[in] heap  the heap used for malloc
 * @param[in] size  size of variable
 * @param[in] align address of variable aligned at.
 * @return address of variable
 *
 * if heap is NULL, variable allocated at default heap.
 */
void* memory_malloc_ext(memory_heap_t* heap, size_t size, size_t align);

/*! malloc with size s at default heap without aligned */
#define memory_malloc(s) memory_malloc_ext(NULL, s, 0x0)
/*! malloc with size s at default heap with aligned a */
#define memory_malloc_aligned(s, a) memory_malloc_ext(NULL, s, a)

/**
 * @brief frees memory
 * @param[in]  heap  the heap where the address is.
 * @param[in]  address address to free
 * @return  0 if successed.
 *
 * if heap is NULL, address will be freed at default heap
 */
int8_t memory_free_ext(memory_heap_t* heap, void* address);
/*! frees memory addr at default heap */
#define memory_free(addr) memory_free_ext(NULL, addr)

/**
 * @brief sets memory with value
 * @param[in]  address the address to be setted.
 * @param[in]  value   the value
 * @param[in] size    repeat count
 * @return 0
 */
int8_t memory_memset(void* address, uint8_t value, size_t size);

/**
 * @brief zeros memory
 * @param[in]  address the address to be zerod.
 * @param[in] size    repeat count
 * @return 0
 */
#define memory_memclean(a, s) memory_memset(a, 0, s)

/**
 * @brief copy source to destination with length bytes from source
 * @param[in]  source      source address
 * @param[in]  destination destination address
 * @param[in]  size      how many bytes will be copied
 * @return 0
 *
 * if destination is smaller then length a memory corruption will be happend
 */
int8_t memory_memcopy(void* source, void* destination, size_t size);

/**
 * @brief compares first length bytes of mem1 with mem2
 * @param  mem1   first memory address
 * @param  mem2   second memory address
 * @param  size count of bytes for compare
 * @return       <0 if mem1>mem2, 0 if mem1=mem2, >0 if mem1>mem2
 */
int8_t memory_memcompare(void* mem1, void* mem2, size_t size);

#endif
