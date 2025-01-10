/**
 * @file spool.h
 * @brief spool header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___SPOOL_H
#define ___SPOOL_H 0

#include <types.h>
#include <list.h>
#include <buffer.h>

typedef struct spool_item_t spool_item_t;

int8_t         spool_init(size_t spool_size, uint64_t spool_start);
int8_t         spool_add(const char_t* name, size_t buf_cnt, ...);
memory_heap_t* spool_get_heap(void);
list_t*        spool_get_all(void);

const char_t*   spool_get_name(const spool_item_t* item);
size_t          spool_get_buffer_count(const spool_item_t* item);
size_t          spool_get_total_buffer_size(const spool_item_t* item);
const buffer_t* spool_get_buffer(const spool_item_t* item, size_t buf_idx);

#endif
