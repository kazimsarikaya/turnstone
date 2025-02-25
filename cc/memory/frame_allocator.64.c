/**
 * @file frame_allocator.64.c
 * @brief Physical Frame allocator implementation for 64-bit systems.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <memory/frame.h>
#include <list.h>
#include <bplustree.h>
#include <systeminfo.h>
#include <efi.h>
#include <cpu.h>
#include <cpu/sync.h>
#include <cpu/descriptor.h>
#include <memory/paging.h>
#include <stdbufs.h>
#include <logging.h>

MODULE("turnstone.kernel.memory.frame");

typedef struct frame_allocator_context_t {
    memory_heap_t* heap;
    list_t*        free_frames_sorted_by_size;
    list_t*        acpi_frames;
    index_t*       free_frames_by_address;
    index_t*       allocated_frames_by_address;
    index_t*       reserved_frames_by_address;
    lock_t*        lock;
    uint64_t       total_frame_count;
    uint64_t       free_frame_count;
    uint64_t       allocated_frame_count;
} frame_allocator_context_t;


int8_t       frame_allocator_cmp_by_size(const void* data1, const void* data2);
int8_t       frame_allocator_cmp_by_address(const void* data1, const void* data2);
int8_t       fa_reserve_system_frames(frame_allocator_t* self, frame_t* f);
int8_t       fa_allocate_frame_by_count(frame_allocator_t* self, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size);
int8_t       fa_allocate_frame(frame_allocator_t* self, frame_t* f);
int8_t       fa_release_frame(frame_allocator_t* self, frame_t* f);
int8_t       fa_release_acpi_reclaim_memory(frame_allocator_t* self);
int8_t       fa_cleanup(frame_allocator_t* self);
frame_t*     fa_get_reserved_frames_of_address(frame_allocator_t* self, void* address);
frame_type_t fa_get_fa_type(efi_memory_type_t efi_m_type);
uint64_t     fa_get_total_frame_count(frame_allocator_t* self);
uint64_t     fa_get_free_frame_count(frame_allocator_t* self);
uint64_t     fa_get_allocated_frame_count(frame_allocator_t* self);

int8_t frame_allocator_cmp_by_size(const void* data1, const void* data2){
    frame_t* f1 = (frame_t*)data1;
    frame_t* f2 = (frame_t*)data2;

    if(f1->frame_count > f2->frame_count) {
        return 1;
    } else if(f1->frame_count < f2->frame_count) {
        return -1;
    }

    return 0;
}

int8_t frame_allocator_cmp_by_address(const void* data1, const void* data2){
    frame_t* f1 = (frame_t*)data1;
    frame_t* f2 = (frame_t*)data2;

    uint64_t f1_start = f1->frame_address;
    uint64_t f1_end = f1->frame_address + f1->frame_count * FRAME_SIZE - 1;

    uint64_t f2_start = f2->frame_address;
    uint64_t f2_end = f2->frame_address + f2->frame_count * FRAME_SIZE - 1;

    if(f1_end < f2_start) {
        return -1;
    }

    if(f1_start > f2_end) {
        return 1;
    }

    return 0;
}

uint64_t fa_get_total_frame_count(frame_allocator_t* self) {
    if(self == NULL) {
        return 0;
    }

    frame_allocator_context_t* ctx = (frame_allocator_context_t*)self->context;

    return ctx->total_frame_count;
}

uint64_t fa_get_free_frame_count(frame_allocator_t* self) {
    if(self == NULL) {
        return 0;
    }

    frame_allocator_context_t* ctx = (frame_allocator_context_t*)self->context;

    return ctx->free_frame_count;
}

uint64_t fa_get_allocated_frame_count(frame_allocator_t* self) {
    if(self == NULL) {
        return 0;
    }

    frame_allocator_context_t* ctx = (frame_allocator_context_t*)self->context;

    return ctx->allocated_frame_count;
}

int8_t fa_reserve_system_frames(frame_allocator_t* self, frame_t* f){
    frame_allocator_context_t* ctx = (frame_allocator_context_t*)self->context;

    lock_acquire(ctx->lock);

    uint64_t rem_frm_cnt = f->frame_count;
    uint64_t rem_frm_start = f->frame_address;



    while(rem_frm_cnt) {

        frame_t search_frm = {rem_frm_start, 1, 0, 0};

        frame_t* frm = (frame_t*)ctx->reserved_frames_by_address->find(ctx->reserved_frames_by_address, &search_frm);

        if(frm == NULL) {
            break;
        }

        if(frm->frame_address <= rem_frm_start && rem_frm_cnt <= frm->frame_count) {
            lock_release(ctx->lock);
            PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "frame inside reserved area");

            return 0;
        }

        uint64_t frm_alloc_cnt = (rem_frm_start - frm->frame_address) / FRAME_SIZE;
        frm_alloc_cnt = frm->frame_count - frm_alloc_cnt;

        rem_frm_start += frm_alloc_cnt * FRAME_SIZE;
        rem_frm_cnt -= frm_alloc_cnt;
    }


    while(rem_frm_cnt) {
        PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "remaining frame start 0x%llx count 0x%llx", rem_frm_start, rem_frm_cnt);

        frame_t search_frm = {rem_frm_start, 1, 0, 0};

        frame_t* frm = (frame_t*)ctx->free_frames_by_address->find(ctx->free_frames_by_address, &search_frm);

        if(frm == NULL) {
            frame_t* new_r_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

            if(new_r_frm == NULL) {
                PRINTLOG(FRAMEALLOCATOR, LOG_FATAL, "no free memory. Halting...");
                cpu_hlt();
            }

            new_r_frm->frame_address = rem_frm_start;
            new_r_frm->frame_count = rem_frm_cnt;
            new_r_frm->type = FRAME_TYPE_RESERVED;
            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_r_frm, new_r_frm, NULL);

            PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "no used frame found, inserted into reserveds, frame start 0x%llx count 0x%llx", rem_frm_start, rem_frm_cnt);

            break;
        }

        PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "area inside free frames, frame start 0x%llx count 0x%llx", rem_frm_start, rem_frm_cnt);

        uint64_t frm_alloc_cnt = (rem_frm_start - frm->frame_address) / FRAME_SIZE;
        frm_alloc_cnt = frm->frame_count - frm_alloc_cnt;

        frame_t new_frm = {rem_frm_start, 0, FRAME_TYPE_RESERVED, 0};

        if(frm_alloc_cnt < rem_frm_cnt) {
            new_frm.frame_count = frm_alloc_cnt;
            rem_frm_cnt -= frm_alloc_cnt;
            rem_frm_start += frm_alloc_cnt * FRAME_SIZE;
        } else {
            new_frm.frame_count = rem_frm_cnt;
            rem_frm_cnt = 0;
        }

        self->allocate_frame(self, &new_frm);
    }

    lock_release(ctx->lock);


    return 0;
}


int8_t fa_allocate_frame_by_count(frame_allocator_t* self, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size) {
    frame_allocator_context_t* ctx = (frame_allocator_context_t*)self->context;


    lock_acquire(ctx->lock);

    if(fa_type & FRAME_ALLOCATION_TYPE_RELAX) {

    } else if(fa_type & FRAME_ALLOCATION_TYPE_BLOCK) {
        iterator_t* iter = list_iterator_create(ctx->free_frames_sorted_by_size);

        if(iter == NULL) {
            lock_release(ctx->lock);
            PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "cannot list free frames");

            return -1;
        }

        frame_t* tmp_frm = NULL;

        while(iter->end_of_iterator(iter) != 0) {
            frame_t* item = (frame_t*)iter->get_item(iter);

            if(item->frame_count >= count) {
                if(fa_type & FRAME_ALLOCATION_TYPE_UNDER_4G) {
                    if(item->frame_address + count * FRAME_SIZE >= 0x100000000) {
                        iter->next(iter);

                        continue;
                    }
                }

                if(count % 0x200 == 0) {
                    if(item->frame_address % MEMORY_PAGING_PAGE_LENGTH_2M == 0) { // fast hit
                        iter->delete_item(iter);
                        ctx->free_frames_by_address->delete(ctx->free_frames_by_address, item, NULL);
                        tmp_frm = item;

                        break;
                    } else {
                        // we need check fit with alignment
                        uint64_t begin_rem = item->frame_address % MEMORY_PAGING_PAGE_LENGTH_2M;
                        uint64_t begin_rem_f_cnt = begin_rem / FRAME_SIZE;

                        if(item->frame_count >= count + begin_rem_f_cnt) {
                            iter->delete_item(iter);
                            ctx->free_frames_by_address->delete(ctx->free_frames_by_address, item, NULL);

                            // we found, but we need to re-insert begining to free frames
                            frame_t* new_begin_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

                            if(new_begin_frm == NULL) {
                                PRINTLOG(FRAMEALLOCATOR, LOG_FATAL, "no free memory. Halting...");
                                cpu_hlt();
                            }

                            new_begin_frm->frame_address = item->frame_address;
                            new_begin_frm->frame_count = begin_rem_f_cnt;

                            list_sortedlist_insert(ctx->free_frames_sorted_by_size, new_begin_frm);
                            ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_begin_frm, new_begin_frm, NULL);

                            item->frame_address += begin_rem_f_cnt * FRAME_SIZE;
                            item->frame_count -= begin_rem_f_cnt;

                            tmp_frm = item;

                            break;
                        }


                    }

                } else {
                    iter->delete_item(iter);
                    ctx->free_frames_by_address->delete(ctx->free_frames_by_address, item, NULL);
                    tmp_frm = item;

                    break;
                }
            }

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        if(tmp_frm == NULL) {
            lock_release(ctx->lock);
            PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "cannot find free frames with count 0x%llx", count);

            return -1;
        }

        if(alloc_list_size) {
            *alloc_list_size = 1;
        }

        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        if(new_frm == NULL) {
            PRINTLOG(FRAMEALLOCATOR, LOG_FATAL, "no free memory. Halting...");
            cpu_hlt();
        }

        uint64_t rem_frms = tmp_frm->frame_count - count;

        new_frm->frame_address = tmp_frm->frame_address;
        new_frm->frame_count = count;
        new_frm->frame_attributes = tmp_frm->frame_attributes;

        ctx->allocated_frame_count += count;
        ctx->free_frame_count -= count;

        if(fa_type & FRAME_ALLOCATION_TYPE_OLD_RESERVED) {
            new_frm->frame_attributes |= FRAME_ATTRIBUTE_OLD_RESERVED;
        }

        *fs = new_frm;

        if(fa_type & FRAME_ALLOCATION_TYPE_RESERVED) {
            new_frm->type = FRAME_TYPE_RESERVED;
            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_frm, new_frm, NULL);
        } else {
            new_frm->type = FRAME_TYPE_USED;
            ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, new_frm, new_frm, NULL);
        }


        if(rem_frms) {
            frame_t* free_rem_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

            if(free_rem_frm == NULL) {
                PRINTLOG(FRAMEALLOCATOR, LOG_FATAL, "no free memory. Halting...");
                cpu_hlt();
            }

            free_rem_frm->frame_address = tmp_frm->frame_address + count * FRAME_SIZE;
            free_rem_frm->frame_count = rem_frms;
            free_rem_frm->type = FRAME_TYPE_FREE;
            free_rem_frm->frame_attributes = tmp_frm->frame_attributes;

            ctx->free_frames_by_address->insert(ctx->free_frames_by_address, free_rem_frm, free_rem_frm, NULL);
            list_sortedlist_insert(ctx->free_frames_sorted_by_size, free_rem_frm);
        }

        memory_free_ext(ctx->heap, tmp_frm);

        lock_release(ctx->lock);

        return 0;
    } else {
        lock_release(ctx->lock);
        PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "unknown alloctation type for frames 0x%x", fa_type);

        return -1;
    }

    lock_release(ctx->lock);

    PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "Should never hit");

    return -1;
}

int8_t fa_allocate_frame(frame_allocator_t* self, frame_t* f) {
    if(self == NULL) {
        return -1;
    }

    frame_allocator_context_t* ctx = self->context;

    lock_acquire(ctx->lock);

    frame_t* frm = (frame_t*)ctx->free_frames_by_address->find(ctx->free_frames_by_address, f);

    if(frm == NULL) {
        lock_release(ctx->lock);
        PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "frame not found 0x%llx 0x%llx", f->frame_address, f->frame_count);

        return -1;
    }

    uint64_t rem_frms = frm->frame_count;

    list_sortedlist_delete(ctx->free_frames_sorted_by_size, frm);
    ctx->free_frames_by_address->delete(ctx->free_frames_by_address, frm, NULL);

    if(frm->frame_address < f->frame_address) {
        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        if(new_frm == NULL) {
            lock_release(ctx->lock);
            return -1;
        }

        new_frm->frame_address = frm->frame_address;
        new_frm->frame_count = (f->frame_address - frm->frame_address) / FRAME_SIZE;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = frm->frame_attributes;

        list_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm, NULL);

        rem_frms -= new_frm->frame_count;
    }

    rem_frms -= f->frame_count;

    if(rem_frms) {
        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        if(new_frm == NULL) {
            lock_release(ctx->lock);
            return -1;
        }

        new_frm->frame_address = f->frame_address + f->frame_count * FRAME_SIZE;
        new_frm->frame_count = rem_frms;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = frm->frame_attributes;

        list_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm, NULL);
    }


    frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

    if(new_frm == NULL) {
        lock_release(ctx->lock);
        return -1;
    }

    new_frm->frame_address = f->frame_address;
    new_frm->frame_count = f->frame_count;
    new_frm->type = f->type != FRAME_TYPE_FREE?f->type:FRAME_TYPE_USED;
    new_frm->frame_attributes = f->frame_attributes?f->frame_attributes:frm->frame_attributes;

    ctx->allocated_frame_count += new_frm->frame_count;
    ctx->free_frame_count -= new_frm->frame_count;

    if(new_frm->type == FRAME_TYPE_USED) {
        ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, new_frm, new_frm, NULL);
    } else {
        ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_frm, new_frm, NULL);
    }

    memory_free_ext(ctx->heap, frm);

    lock_release(ctx->lock);

    return 0;
}

int8_t fa_release_frame(frame_allocator_t* self, frame_t* f) {
    if(self == NULL) {
        return -1;
    }

    frame_allocator_context_t* ctx = self->context;

    lock_acquire(ctx->lock);

    const frame_t* tmp_frame = ctx->allocated_frames_by_address->find(ctx->allocated_frames_by_address, f);

    if(tmp_frame) {

        ctx->allocated_frames_by_address->delete(ctx->allocated_frames_by_address, tmp_frame, NULL);

        uint64_t rem_frms = tmp_frame->frame_count - f->frame_count;

        if(tmp_frame->frame_address < f->frame_address) {
            uint64_t prev_frm_count = (f->frame_address - tmp_frame->frame_address) / FRAME_SIZE;

            frame_t* prev_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

            if(prev_frm == NULL) {
                lock_release(ctx->lock);
                return -1;
            }
            prev_frm->frame_address = tmp_frame->frame_address;
            prev_frm->frame_count = prev_frm_count;
            prev_frm->type = FRAME_TYPE_USED;
            prev_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, prev_frm, prev_frm, NULL);

            rem_frms -= prev_frm_count;
        }

        if(rem_frms) {
            frame_t* next_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

            if(next_frm == NULL) {
                lock_release(ctx->lock);
                return -1;
            }
            next_frm->frame_address = f->frame_address + f->frame_count * FRAME_SIZE;
            next_frm->frame_count = rem_frms;
            next_frm->type = FRAME_TYPE_USED;
            next_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, next_frm, next_frm, NULL);
        }

        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        if(new_frm == NULL) {
            lock_release(ctx->lock);
            return -1;
        }

        new_frm->frame_address = f->frame_address;
        new_frm->frame_count = f->frame_count;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = tmp_frame->frame_attributes;

        ctx->allocated_frame_count -= new_frm->frame_count;
        ctx->free_frame_count += new_frm->frame_count;

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm, NULL);
        list_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);

        memory_free_ext(ctx->heap, (void*)tmp_frame);

        lock_release(ctx->lock);

        return 0;
    }

    tmp_frame = ctx->reserved_frames_by_address->find(ctx->reserved_frames_by_address, f);

    if(tmp_frame) {
        ctx->reserved_frames_by_address->delete(ctx->reserved_frames_by_address, tmp_frame, NULL);

        uint64_t rem_frms = tmp_frame->frame_count - f->frame_count;

        if(tmp_frame->frame_address < f->frame_address) {
            uint64_t prev_frm_count = (f->frame_address - tmp_frame->frame_address) / FRAME_SIZE;

            frame_t* prev_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

            if(prev_frm == NULL) {
                lock_release(ctx->lock);
                return -1;
            }

            prev_frm->frame_address = tmp_frame->frame_address;
            prev_frm->frame_count = prev_frm_count;
            prev_frm->type = FRAME_TYPE_USED;
            prev_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, prev_frm, prev_frm, NULL);

            rem_frms -= prev_frm_count;
        }

        if(rem_frms) {
            frame_t* next_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

            if(next_frm == NULL) {
                lock_release(ctx->lock);
                return -1;
            }

            next_frm->frame_address = f->frame_address + f->frame_count * FRAME_SIZE;
            next_frm->frame_count = rem_frms;
            next_frm->type = FRAME_TYPE_USED;
            next_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, next_frm, next_frm, NULL);
        }

        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        if(new_frm == NULL) {
            lock_release(ctx->lock);
            return -1;
        }

        new_frm->frame_address = f->frame_address;
        new_frm->frame_count = f->frame_count;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = tmp_frame->frame_attributes;

        ctx->allocated_frame_count -= new_frm->frame_count;
        ctx->free_frame_count += new_frm->frame_count;

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm, NULL);
        list_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);

        memory_free_ext(ctx->heap, (void*)tmp_frame);
    }

    lock_release(ctx->lock);


    return 0;
}

int8_t fa_release_acpi_reclaim_memory(frame_allocator_t* self) {
    if(self == NULL) {
        return -1;
    }

    frame_allocator_context_t* ctx = self->context;

    lock_acquire(ctx->lock);

    iterator_t* iter;
    list_t* frms;


    frms = list_create_sortedlist_with_heap(ctx->heap, frame_allocator_cmp_by_size);

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if((f->frame_attributes & FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY) && !(f->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED)) {
            list_sortedlist_insert(frms, f);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = list_iterator_create(frms);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        ctx->reserved_frames_by_address->delete(ctx->reserved_frames_by_address, f, NULL);

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        f->frame_attributes &= ~FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY;
        f->type = FRAME_TYPE_FREE;

        ctx->allocated_frame_count -= f->frame_count;
        ctx->free_frame_count += f->frame_count;

        list_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f, NULL);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    list_destroy(frms);

    lock_release(ctx->lock);

    return 0;
}

int8_t fa_cleanup(frame_allocator_t* self) {
    if(self == NULL) {
        return -1;
    }

    frame_allocator_context_t* ctx = self->context;

    lock_acquire(ctx->lock);

    iterator_t* iter;
    list_t* frms;


    frms = list_create_sortedlist_with_heap(ctx->heap, frame_allocator_cmp_by_size);

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if(f->frame_attributes & FRAME_ATTRIBUTE_OLD_RESERVED) {
            list_sortedlist_insert(frms, f);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = list_iterator_create(frms);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        ctx->reserved_frames_by_address->delete(ctx->reserved_frames_by_address, f, NULL);

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        f->frame_attributes &= ~FRAME_ATTRIBUTE_OLD_RESERVED;
        f->type = FRAME_TYPE_FREE;

        ctx->allocated_frame_count -= f->frame_count;
        ctx->free_frame_count += f->frame_count;

        list_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f, NULL);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    list_destroy(frms);


    frms = list_create_sortedlist_with_heap(ctx->heap, frame_allocator_cmp_by_size);

    iter = ctx->allocated_frames_by_address->create_iterator(ctx->allocated_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if(f->frame_attributes & FRAME_ATTRIBUTE_OLD_RESERVED) {
            list_sortedlist_insert(frms, f);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = list_iterator_create(frms);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        ctx->allocated_frames_by_address->delete(ctx->allocated_frames_by_address, f, NULL);

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        f->frame_attributes &= ~FRAME_ATTRIBUTE_OLD_RESERVED;
        f->type = FRAME_TYPE_FREE;

        ctx->allocated_frame_count -= f->frame_count;
        ctx->free_frame_count += f->frame_count;

        list_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f, NULL);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    list_destroy(frms);

    lock_release(ctx->lock);

    return 0;
}


frame_t* fa_get_reserved_frames_of_address(frame_allocator_t* self, void* address){
    if(self == NULL) {
        return NULL;
    }

    frame_allocator_context_t* ctx = self->context;

    frame_t f = {((((uint64_t)address) >> 12) << 12), 1, 0, 0};

    frame_t* res = (frame_t*)ctx->reserved_frames_by_address->find(ctx->reserved_frames_by_address, &f);

    return res;
}

frame_type_t fa_get_fa_type(efi_memory_type_t efi_m_type){
    if(efi_m_type == EFI_LOADER_CODE || efi_m_type == EFI_LOADER_DATA) {
        return FRAME_TYPE_FREE;
    }

    if(efi_m_type == EFI_BOOT_SERVICES_CODE || efi_m_type == EFI_RUNTIME_SERVICES_CODE || efi_m_type == EFI_PAL_CODE) {
        return FRAME_TYPE_ACPI_CODE;
    }

    if(efi_m_type == EFI_BOOT_SERVICES_DATA || efi_m_type == EFI_RUNTIME_SERVICES_DATA) {
        return FRAME_TYPE_ACPI_DATA;
    }

    if(efi_m_type == EFI_CONVENTIONAL_MEMORY) {
        return FRAME_TYPE_FREE;
    }

    if(efi_m_type == EFI_ACPI_RECLAIM_MEMORY) {
        return FRAME_TYPE_ACPI_RECLAIM_MEMORY;
    }

    return FRAME_TYPE_RESERVED;
}

static int8_t frame_allocator_clone_key(memory_heap_t* heap, const void* key, void** out) {
    frame_t* f = (frame_t*)key;

    frame_t* new_frm = memory_malloc_ext(heap, sizeof(frame_t), 0);

    if(new_frm == NULL) {
        return -1;
    }

    memory_memcopy(f, new_frm, sizeof(frame_t));

    *out = new_frm;

    return 0;
}

static int8_t frame_allocator_destroy_key(memory_heap_t* heap, void* key) {
    memory_free_ext(heap, key);

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
frame_allocator_t* frame_allocator_new_ext(memory_heap_t* heap) {
    frame_allocator_context_t* ctx = memory_malloc_ext(heap, sizeof(frame_allocator_context_t), 0);

    if(ctx == NULL) {
        return NULL;
    }

    frame_allocator_t* fa = memory_malloc_ext(heap, sizeof(frame_allocator_t), 0);

    if(fa == NULL) {
        memory_free_ext(ctx->heap, ctx);
        return NULL;
    }

    ctx->heap = heap;
    ctx->free_frames_sorted_by_size = list_create_sortedlist_with_heap(heap, frame_allocator_cmp_by_size);
    list_set_equality_comparator(ctx->free_frames_sorted_by_size, frame_allocator_cmp_by_address);

    ctx->acpi_frames = list_create_sortedlist_with_heap(heap, frame_allocator_cmp_by_address);

    ctx->free_frames_by_address = bplustree_create_index_with_heap_and_unique(heap, 64, frame_allocator_cmp_by_address, true);
    bplustree_set_key_cloner(ctx->free_frames_by_address, frame_allocator_clone_key);
    bplustree_set_key_destroyer(ctx->free_frames_by_address, frame_allocator_destroy_key);

    ctx->allocated_frames_by_address = bplustree_create_index_with_heap_and_unique(heap, 64, frame_allocator_cmp_by_address, true);
    bplustree_set_key_cloner(ctx->allocated_frames_by_address, frame_allocator_clone_key);
    bplustree_set_key_destroyer(ctx->allocated_frames_by_address, frame_allocator_destroy_key);

    ctx->reserved_frames_by_address = bplustree_create_index_with_heap_and_unique(heap, 64, frame_allocator_cmp_by_address, true);
    bplustree_set_key_cloner(ctx->reserved_frames_by_address, frame_allocator_clone_key);
    bplustree_set_key_destroyer(ctx->reserved_frames_by_address, frame_allocator_destroy_key);

    ctx->lock = lock_create_with_heap(heap);

    efi_memory_descriptor_t* mem_desc;

    mem_desc = (efi_memory_descriptor_t*)(SYSTEM_INFO->mmap_data);

    uint64_t frame_start = mem_desc->physical_start;
    uint64_t frame_count = mem_desc->page_count;
    frame_type_t type = fa_get_fa_type(mem_desc->type);
    uint64_t frame_attr = mem_desc->attribute;

    uint64_t mmap_ent_cnt = SYSTEM_INFO->mmap_size / SYSTEM_INFO->mmap_descriptor_size;
    for(size_t i = 1; i < mmap_ent_cnt; i++) {
        mem_desc = (efi_memory_descriptor_t*)(SYSTEM_INFO->mmap_data + (i * SYSTEM_INFO->mmap_descriptor_size));

        if(type == fa_get_fa_type(mem_desc->type) && (frame_start + frame_count * FRAME_SIZE) == mem_desc->physical_start && frame_attr == mem_desc->attribute) {
            frame_count += mem_desc->page_count;
        } else {


            frame_t* f = memory_malloc_ext(heap, sizeof(frame_t), 0);

            if(f == NULL) {
                memory_free_ext(ctx->heap, fa);
                memory_free_ext(ctx->heap, ctx);

                return NULL;
            }


            f->frame_address = frame_start;
            f->frame_count = frame_count;
            f->type = type;
            f->frame_attributes = mem_desc->attribute;

            if((frame_start + frame_count * FRAME_SIZE) <= (1 << 20)) {
                type = FRAME_TYPE_RESERVED;
                f->type = type;
            }

            ctx->total_frame_count += frame_count;

            switch (type) {
            case FRAME_TYPE_FREE:
                ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f, NULL);
                list_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
                ctx->free_frame_count += frame_count;
                break;
            case FRAME_TYPE_USED:
                ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, f, f, NULL);
                ctx->allocated_frame_count += frame_count;
                break;
            case FRAME_TYPE_RESERVED:
                ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, f, f, NULL);
                ctx->allocated_frame_count += frame_count;
                break;
            case FRAME_TYPE_ACPI_RECLAIM_MEMORY:
                f->frame_attributes |= FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY;
                ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, f, f, NULL);
                ctx->allocated_frame_count += frame_count;
                break;
            case FRAME_TYPE_ACPI_CODE:
            case FRAME_TYPE_ACPI_DATA:
                f->frame_attributes |= FRAME_ATTRIBUTE_ACPI;
                list_sortedlist_insert(ctx->acpi_frames, f);
                ctx->allocated_frame_count += frame_count;
            }


            frame_start = mem_desc->physical_start;
            frame_count = mem_desc->page_count;
            type = fa_get_fa_type(mem_desc->type);
        }

    }

    fa->context = ctx;
    fa->allocate_frame_by_count = fa_allocate_frame_by_count;
    fa->allocate_frame = fa_allocate_frame;
    fa->release_frame = fa_release_frame;
    fa->cleanup = fa_cleanup;
    fa->get_reserved_frames_of_address = fa_get_reserved_frames_of_address;
    fa->reserve_system_frames = fa_reserve_system_frames;
    fa->release_acpi_reclaim_memory = fa_release_acpi_reclaim_memory;
    fa->get_free_frame_count = fa_get_free_frame_count;
    fa->get_total_frame_count = fa_get_total_frame_count;
    fa->get_allocated_frame_count = fa_get_allocated_frame_count;

    return fa;
}
#pragma GCC diagnostic pop

// TODO: delete me
void frame_allocator_print(frame_allocator_t* fa) {
    if(fa == NULL) {
        return;
    }

    frame_allocator_context_t* ctx = fa->context;

    iterator_t* iter;

    iter = ctx->free_frames_by_address->create_iterator(ctx->free_frames_by_address);

    printf("free frames by address\n");

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        printf("0x%016llx \t 0x%016llx \t 0x%016llx\n", f->frame_address, f->frame_count, f->frame_attributes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = list_iterator_create(ctx->free_frames_sorted_by_size);

    printf("free frames by size\n");

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        printf("0x%016llx \t 0x%016llx \t 0x%016llx\n", f->frame_address, f->frame_count, f->frame_attributes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = ctx->allocated_frames_by_address->create_iterator(ctx->allocated_frames_by_address);

    printf("used frames by address\n");

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        printf("0x%016llx \t 0x%016llx \t 0x%016llx\n", f->frame_address, f->frame_count, f->frame_attributes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    printf("reserved frames by address\n");

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        printf("0x%016llx \t 0x%016llx \t 0x%016llx\n", f->frame_address, f->frame_count, f->frame_attributes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = list_iterator_create(ctx->acpi_frames);

    printf("acpi code/data frames by address\n");

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        printf("0x%016llx \t 0x%016llx \t 0x%016llx\n", f->frame_address, f->frame_count, f->frame_attributes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

}

void frame_allocator_map_page_of_acpi_code_data_frames(frame_allocator_t* fa) {
    if(fa == NULL) {
        return;
    }

    frame_allocator_context_t* ctx = fa->context;

    iterator_t* iter = list_iterator_create(ctx->acpi_frames);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if(f->type == FRAME_TYPE_ACPI_CODE) {
            memory_paging_add_va_for_frame(f->frame_address, f, MEMORY_PAGING_PAGE_TYPE_UNKNOWN);
        } else if(f->type == FRAME_TYPE_ACPI_DATA) {
            memory_paging_add_va_for_frame(f->frame_address, f, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
        } else {
            PRINTLOG(FRAMEALLOCATOR, LOG_WARNING, "unknown acpi runtime frame type 0x%x", f->type);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);
}

