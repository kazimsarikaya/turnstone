/**
 * @file memory.64.cpp
 * @brief c++ memory management
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <memory.h>
#include <assert.h>
#include <stdbufs.h>

MODULE("turnstone.lib.memory.cpp");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
void * operator new(size_t size) {
    void* res = memory_malloc_ext(NULL, size, 0x0);

    assert(res != NULL && "Failed to allocate memory\n");

    printf("Allocating memory %p\n", res);
    return res;
}

void * operator new(size_t size, size_t align) {
    void* res = memory_malloc_ext(NULL, size, align);

    assert(res != NULL && "Failed to allocate memory\n");

    printf("Allocating memory %p %li\n", res, align);
    return res;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
void * operator new[](size_t size) {
    void* res = memory_malloc_ext(NULL, size, 0x0);

    assert(res != NULL && "Failed to allocate memory\n");

    printf("Allocating array memory %p\n", res);
    return res;
}

void * operator new[](size_t size, size_t align) {
    void* res = memory_malloc_ext(NULL, size, align);

    assert(res != NULL && "Failed to allocate memory\n");

    printf("Allocating array memory %p %li\n", res, align);
    return res;
}
#pragma GCC diagnostic pop

void operator delete(void * p) {
    printf("Deleting memory %p\n", p);
    memory_free_ext(NULL, p);
}

void operator delete[](void * p) {
    printf("Deleting array memory %p\n", p);
    memory_free_ext(NULL, p);
}

void operator delete(void * p, size_t size) {
    printf("Deleting memory with size %p %li\n", p, size);
    memory_free_ext(NULL, p);
}

void operator delete[](void * p, size_t size) {
    printf("Deleting array memory with size %p %li\n", p, size);
    memory_free_ext(NULL, p);
}
