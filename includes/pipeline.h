/**
 * @file pipeline.h
 * @brief Pipeline stream header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___PIPELINE_H
#define ___PIPELINE_H

#include <types.h>
#include <memory.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pipeline_t pipeline_t;

pipeline_t* pipeline_create_with_heap(memory_heap_t* heap, uint64_t capacity);
#define pipeline_create(capacity) pipeline_create_with_heap(NULL, capacity)
int8_t pipeline_destroy(pipeline_t* pipeline);

uint64_t pipeline_write(pipeline_t* pipeline, uint64_t size, const uint8_t* data);
uint64_t pipeline_write_blocked(pipeline_t* pipeline, uint64_t size, const uint8_t* data);
uint64_t pipeline_read(pipeline_t* pipeline, uint64_t size, uint8_t* data);
uint64_t pipeline_read_blocked(pipeline_t* pipeline, uint64_t size, uint8_t* data);
uint64_t pipeline_available_data(pipeline_t* pipeline);
uint64_t pipeline_available_space(pipeline_t* pipeline);
uint64_t pipeline_capacity(pipeline_t* pipeline);
int8_t   pipeline_clear(pipeline_t* pipeline);

#ifdef __cplusplus
}
#endif

#endif
