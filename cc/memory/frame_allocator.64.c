/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <memory/frame.h>
#include <linkedlist.h>
#include <bplustree.h>
#include <systeminfo.h>
#include <efi.h>
#include <cpu.h>
#include <cpu/sync.h>
#include <cpu/descriptor.h>
#include <memory/paging.h>


frame_allocator_t* KERNEL_FRAME_ALLOCATOR = NULL;

typedef struct {
    memory_heap_t* heap;
    linkedlist_t   free_frames_sorted_by_size;
    index_t*       free_frames_by_address;
    index_t*       allocated_frames_by_address;
    index_t*       reserved_frames_by_address;
    lock_t         lock;
} frame_allocator_context_t;

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

int8_t fa_reserve_system_frames(struct frame_allocator* self, frame_t* f){
    frame_allocator_context_t* ctx = (frame_allocator_context_t*)self->context;

    lock_acquire(ctx->lock);

    uint64_t rem_frm_cnt = f->frame_count;
    uint64_t rem_frm_start = f->frame_address;



    while(rem_frm_cnt) {

        frame_t search_frm = {rem_frm_start, 1, 0, 0};

        iterator_t* iter = ctx->reserved_frames_by_address->search(ctx->reserved_frames_by_address, &search_frm, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

        if(iter == NULL) {
            break;
        }

        if(iter->end_of_iterator(iter) == 0) {
            iter->destroy(iter);

            break;
        }

        frame_t* frm = (frame_t*)iter->get_item(iter);
        iter->destroy(iter);

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

        iterator_t* iter = ctx->free_frames_by_address->search(ctx->free_frames_by_address, &search_frm, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

        if(iter == NULL) {
            frame_t* new_r_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
            new_r_frm->frame_address = rem_frm_start;
            new_r_frm->frame_count = rem_frm_cnt;
            new_r_frm->type = FRAME_TYPE_RESERVED;
            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_r_frm, new_r_frm);

            PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "no used frame found, inserted into reserveds, frame start 0x%llx count 0x%llx", rem_frm_start, rem_frm_cnt);

            break;
        }

        if(iter->end_of_iterator(iter) == 0) {
            iter->destroy(iter);

            frame_t* new_r_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
            new_r_frm->frame_address = rem_frm_start;
            new_r_frm->frame_count = rem_frm_cnt;
            new_r_frm->type = FRAME_TYPE_RESERVED;
            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_r_frm, new_r_frm);

            PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "no used frame found, inserted into reserveds, frame start 0x%llx count 0x%llx", rem_frm_start, rem_frm_cnt);

            break;
        }

        PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "area inside free frames, frame start 0x%llx count 0x%llx", rem_frm_start, rem_frm_cnt);

        frame_t* frm = (frame_t*)iter->get_item(iter);
        iter->destroy(iter);

        if(frm == NULL) {
            frame_t new_frm = {rem_frm_start, rem_frm_cnt, FRAME_TYPE_RESERVED, 0};
            self->allocate_frame(self, &new_frm);

            break;
        }

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
        iterator_t* iter = linkedlist_iterator_create(ctx->free_frames_sorted_by_size);

        if(iter == NULL) {
            lock_release(ctx->lock);
            PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "cannot list free frames");

            return -1;
        }

        frame_t* tmp_frm = NULL;

        while(iter->end_of_iterator(iter) != 0) {
            frame_t* item = (frame_t*)iter->get_item(iter);

            if(item->frame_count >= count) {
                iter->delete_item(iter);
                ctx->free_frames_by_address->delete(ctx->free_frames_by_address, item, NULL);
                tmp_frm = item;

                break;
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

        if(fa_type & FRAME_ALLOCATION_TYPE_OLD_RESERVED) {
            new_frm->frame_attributes |= FRAME_ATTRIBUTE_OLD_RESERVED;
        }

        *fs = new_frm;

        if(fa_type & FRAME_ALLOCATION_TYPE_RESERVED) {
            new_frm->type = FRAME_TYPE_RESERVED;
            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_frm, new_frm);
        } else {
            new_frm->type = FRAME_TYPE_USED;
            ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, new_frm, new_frm);
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

            ctx->free_frames_by_address->insert(ctx->free_frames_by_address, free_rem_frm, free_rem_frm);
            linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, free_rem_frm);
        }

        memory_free_ext(ctx->heap, tmp_frm);

        lock_release(ctx->lock);

        return 0;
    } else {
        lock_release(ctx->lock);
        PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "unknown alloctation type for frames");

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

    iterator_t* iter = ctx->free_frames_by_address->search(ctx->free_frames_by_address, f, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    if(iter == NULL) {
        lock_release(ctx->lock);
        PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "frame not found 0x%llx 0x%llx", f->frame_address, f->frame_count);

        return -1;
    }

    if(iter->end_of_iterator(iter) == 0) {
        iter->destroy(iter);
        lock_release(ctx->lock);
        PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "frame not found 0x%llx 0x%llx", f->frame_address, f->frame_count);

        return -1;
    }

    frame_t* frm = (frame_t*)iter->get_item(iter);
    iter->destroy(iter);

    if(frm == NULL) {
        lock_release(ctx->lock);
        PRINTLOG(FRAMEALLOCATOR, LOG_ERROR, "frame not found 0x%llx 0x%llx", f->frame_address, f->frame_count);

        return -1;
    }

    uint64_t rem_frms = frm->frame_count;

    linkedlist_sortedlist_delete(ctx->free_frames_sorted_by_size, frm);
    ctx->free_frames_by_address->delete(ctx->free_frames_by_address, frm, NULL);

    if(frm->frame_address < f->frame_address) {
        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        new_frm->frame_address = frm->frame_address;
        new_frm->frame_count = (f->frame_address - frm->frame_address) / FRAME_SIZE;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = frm->frame_attributes;

        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm);

        rem_frms -= new_frm->frame_count;
    }

    rem_frms -= f->frame_count;

    if(rem_frms) {
        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

        new_frm->frame_address = f->frame_address + f->frame_count * FRAME_SIZE;
        new_frm->frame_count = rem_frms;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = frm->frame_attributes;

        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm);
    }


    frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);

    new_frm->frame_address = f->frame_address;
    new_frm->frame_count = f->frame_count;
    new_frm->type = f->type != FRAME_TYPE_FREE?f->type:FRAME_TYPE_USED;
    new_frm->frame_attributes = f->frame_attributes?f->frame_attributes:frm->frame_attributes;

    if(new_frm->type == FRAME_TYPE_USED) {
        ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, new_frm, new_frm);
    } else {
        ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, new_frm, new_frm);
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

    iterator_t* iter;

    iter = ctx->allocated_frames_by_address->search(ctx->allocated_frames_by_address, f, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    if(iter->end_of_iterator(iter) != 0) {
        frame_t* tmp_frame = iter->get_item(iter);

        ctx->allocated_frames_by_address->delete(ctx->allocated_frames_by_address, tmp_frame, NULL);

        uint64_t rem_frms = tmp_frame->frame_count - f->frame_count;

        if(tmp_frame->frame_address < f->frame_address) {
            uint64_t prev_frm_count = (f->frame_address - tmp_frame->frame_address) / FRAME_SIZE;

            frame_t* prev_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
            prev_frm->frame_address = tmp_frame->frame_address;
            prev_frm->frame_count = prev_frm_count;
            prev_frm->type = FRAME_TYPE_USED;
            prev_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, prev_frm, prev_frm);

            rem_frms -= prev_frm_count;
        }

        if(rem_frms) {
            frame_t* next_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
            next_frm->frame_address = f->frame_address + f->frame_count * FRAME_SIZE;
            next_frm->frame_count = rem_frms;
            next_frm->type = FRAME_TYPE_USED;
            next_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, next_frm, next_frm);
        }

        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
        new_frm->frame_address = f->frame_address;
        new_frm->frame_count = f->frame_count;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = tmp_frame->frame_attributes;

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm);
        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);

        iter->destroy(iter);

        memory_free_ext(ctx->heap, tmp_frame);

        lock_release(ctx->lock);

        return 0;
    }

    iter->destroy(iter);


    iter = ctx->reserved_frames_by_address->search(ctx->reserved_frames_by_address, f, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    if(iter->end_of_iterator(iter) != 0) {
        frame_t* tmp_frame = iter->get_item(iter);

        ctx->reserved_frames_by_address->delete(ctx->reserved_frames_by_address, tmp_frame, NULL);

        uint64_t rem_frms = tmp_frame->frame_count - f->frame_count;

        if(tmp_frame->frame_address < f->frame_address) {
            uint64_t prev_frm_count = (f->frame_address - tmp_frame->frame_address) / FRAME_SIZE;

            frame_t* prev_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
            prev_frm->frame_address = tmp_frame->frame_address;
            prev_frm->frame_count = prev_frm_count;
            prev_frm->type = FRAME_TYPE_USED;
            prev_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, prev_frm, prev_frm);

            rem_frms -= prev_frm_count;
        }

        if(rem_frms) {
            frame_t* next_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
            next_frm->frame_address = f->frame_address + f->frame_count * FRAME_SIZE;
            next_frm->frame_count = rem_frms;
            next_frm->type = FRAME_TYPE_USED;
            next_frm->frame_attributes = tmp_frame->frame_attributes;

            ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, next_frm, next_frm);
        }

        frame_t* new_frm = memory_malloc_ext(ctx->heap, sizeof(frame_t), 0);
        new_frm->frame_address = f->frame_address;
        new_frm->frame_count = f->frame_count;
        new_frm->type = FRAME_TYPE_FREE;
        new_frm->frame_attributes = tmp_frame->frame_attributes;

        for(uint64_t i = 0; i < f->frame_count; i++) {
            memory_paging_add_page(0x1000, f->frame_address + i * FRAME_SIZE, MEMORY_PAGING_PAGE_TYPE_4K);
            memory_memclean((void*)(0x1000), FRAME_SIZE);
            memory_paging_delete_page(0x1000, NULL);
        }

        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, new_frm, new_frm);
        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, new_frm);

        memory_free_ext(ctx->heap, tmp_frame);
    }

    iter->destroy(iter);

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
    linkedlist_t frms;


    frms = linkedlist_create_sortedlist_with_heap(ctx->heap, frame_allocator_cmp_by_size);

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if((f->frame_attributes & FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY) && !(f->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED)) {
            linkedlist_sortedlist_insert(frms, f);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = linkedlist_iterator_create(frms);

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

        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy(frms);

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
    linkedlist_t frms;


    frms = linkedlist_create_sortedlist_with_heap(ctx->heap, frame_allocator_cmp_by_size);

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if(f->frame_attributes & FRAME_ATTRIBUTE_OLD_RESERVED) {
            linkedlist_sortedlist_insert(frms, f);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = linkedlist_iterator_create(frms);

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

        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy(frms);


    frms = linkedlist_create_sortedlist_with_heap(ctx->heap, frame_allocator_cmp_by_size);

    iter = ctx->allocated_frames_by_address->create_iterator(ctx->allocated_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);

        if(f->frame_attributes & FRAME_ATTRIBUTE_OLD_RESERVED) {
            linkedlist_sortedlist_insert(frms, f);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    iter = linkedlist_iterator_create(frms);

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

        linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
        ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy(frms);

    lock_release(ctx->lock);

    return 0;
}


frame_t* fa_get_reserved_frames_of_address(struct frame_allocator* self, void* address){
    if(self == NULL) {
        return NULL;
    }

    frame_allocator_context_t* ctx = self->context;

    frame_t f = {((((uint64_t)address) >> 12) << 12), 1, 0, 0};

    frame_t* res = NULL;

    iterator_t* iter = ctx->reserved_frames_by_address->search(ctx->reserved_frames_by_address, &f, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

    if(iter->end_of_iterator(iter) != 0) {
        res = (frame_t*)iter->get_item(iter);
    }

    iter->destroy(iter);

    return res;
}

int8_t fa_rebuild_reserved_mmap(frame_allocator_t* self) {
    if(self == NULL) {
        return -1;
    }

    frame_allocator_context_t* ctx = self->context;

    lock_acquire(ctx->lock);

    uint64_t mmap_entry_cnt = 0;

    iterator_t* iter;

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        mmap_entry_cnt++;

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    memory_free_ext(SYSTEM_INFO->heap, SYSTEM_INFO->reserved_mmap_data);

    SYSTEM_INFO->reserved_mmap_size = SYSTEM_INFO->mmap_descriptor_size * mmap_entry_cnt;
    SYSTEM_INFO->reserved_mmap_data = memory_malloc_ext(SYSTEM_INFO->heap, SYSTEM_INFO->reserved_mmap_size, 0);

    if(SYSTEM_INFO->reserved_mmap_data == NULL) {
        PRINTLOG(FRAMEALLOCATOR, LOG_FATAL, "Cannot create reserved mmap data. Halting...");
        cpu_hlt();
    }

    PRINTLOG(FRAMEALLOCATOR, LOG_DEBUG, "reserved map ent count 0x%llx size 0x%llx", mmap_entry_cnt, SYSTEM_INFO->reserved_mmap_size);

    uint64_t i = 0;

    iter = ctx->reserved_frames_by_address->create_iterator(ctx->reserved_frames_by_address);

    while(iter->end_of_iterator(iter) != 0) {
        frame_t* f = (frame_t*)iter->get_item(iter);
        efi_memory_descriptor_t* mem_desc = (efi_memory_descriptor_t*)(SYSTEM_INFO->reserved_mmap_data + (i * SYSTEM_INFO->mmap_descriptor_size));

        mem_desc->type = EFI_RESERVED_MEMORY_TYPE;
        mem_desc->physical_start = f->frame_address;
        mem_desc->virtual_start = f->frame_address;
        mem_desc->page_count = f->frame_count;
        mem_desc->attribute = f->frame_attributes | FRAME_ATTRIBUTE_OLD_RESERVED;

        PRINTLOG(FRAMEALLOCATOR, LOG_DEBUG, "reserved mmap start 0x%016llx count 0x%llx", mem_desc->physical_start, mem_desc->page_count);

        iter = iter->next(iter);
        i++;
    }

    lock_release(ctx->lock);

    return 0;
}

frame_type_t fa_get_fa_type(efi_memory_type_t efi_m_type){
    if(efi_m_type >= EFI_LOADER_CODE && efi_m_type <= EFI_BOOT_SERVICES_DATA) {
        return FRAME_TYPE_FREE;
    }

    if(efi_m_type == EFI_CONVENTIONAL_MEMORY) {
        return FRAME_TYPE_FREE;
    }

    if(efi_m_type == EFI_ACPI_RECLAIM_MEMORY) {
        return FRAME_TYPE_ACPI_RECLAIM_MEMORY;
    }

    return FRAME_TYPE_RESERVED;
}

frame_allocator_t* frame_allocator_new_ext(memory_heap_t* heap) {
    frame_allocator_context_t* ctx = memory_malloc_ext(heap, sizeof(frame_allocator_context_t), 0);

    ctx->heap = heap;
    ctx->free_frames_sorted_by_size = linkedlist_create_sortedlist_with_heap(heap, frame_allocator_cmp_by_size);
    linkedlist_set_equality_comparator(ctx->free_frames_sorted_by_size, frame_allocator_cmp_by_address);

    ctx->free_frames_by_address = bplustree_create_index_with_heap(heap, 64, frame_allocator_cmp_by_address);
    ctx->allocated_frames_by_address = bplustree_create_index_with_heap(heap, 64, frame_allocator_cmp_by_address);
    ctx->reserved_frames_by_address = bplustree_create_index_with_heap(heap, 64, frame_allocator_cmp_by_address);
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
            f->frame_address = frame_start;
            f->frame_count = frame_count;
            f->type = type;
            f->frame_attributes = mem_desc->attribute;

            if((frame_start + frame_count * FRAME_SIZE) <= (1 << 20)) {
                type = FRAME_TYPE_RESERVED;
                f->type = type;
            }

            switch (type) {
            case FRAME_TYPE_FREE:
                ctx->free_frames_by_address->insert(ctx->free_frames_by_address, f, f);
                linkedlist_sortedlist_insert(ctx->free_frames_sorted_by_size, f);
                break;
            case FRAME_TYPE_USED:
                ctx->allocated_frames_by_address->insert(ctx->allocated_frames_by_address, f, f);
                break;
            case FRAME_TYPE_RESERVED:
                ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, f, f);
                break;
            case FRAME_TYPE_ACPI_RECLAIM_MEMORY:
                f->frame_attributes |= FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY;
                ctx->reserved_frames_by_address->insert(ctx->reserved_frames_by_address, f, f);
                break;
            }


            frame_start = mem_desc->physical_start;
            frame_count = mem_desc->page_count;
            type = fa_get_fa_type(mem_desc->type);
        }

    }

    frame_allocator_t* fa = memory_malloc_ext(heap, sizeof(frame_allocator_t), 0);

    fa->context = ctx;
    fa->allocate_frame_by_count = fa_allocate_frame_by_count;
    fa->allocate_frame = fa_allocate_frame;
    fa->release_frame = fa_release_frame;
    fa->rebuild_reserved_mmap = fa_rebuild_reserved_mmap;
    fa->cleanup = fa_cleanup;
    fa->get_reserved_frames_of_address = fa_get_reserved_frames_of_address;
    fa->reserve_system_frames = fa_reserve_system_frames;
    fa->release_acpi_reclaim_memory = fa_release_acpi_reclaim_memory;

    if(SYSTEM_INFO->reserved_mmap_size && SYSTEM_INFO->reserved_mmap_data) {
        uint64_t resv_mmap_ent_cnt = SYSTEM_INFO->reserved_mmap_size / SYSTEM_INFO->mmap_descriptor_size;
        for(size_t i = 1; i < resv_mmap_ent_cnt; i++) {
            mem_desc = (efi_memory_descriptor_t*)(SYSTEM_INFO->reserved_mmap_data + (i * SYSTEM_INFO->mmap_descriptor_size));



            frame_t f = {mem_desc->physical_start, mem_desc->page_count, FRAME_TYPE_USED, mem_desc->attribute};

            iterator_t* iter = ctx->reserved_frames_by_address->search(ctx->reserved_frames_by_address, &f, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

            if(iter->end_of_iterator(iter) != 0) {
                iter->destroy(iter);

                continue;
            }

            iter->destroy(iter);

            if(f.frame_address == IDT_BASE_ADDRESS && f.frame_count == 1) {
                f.type = FRAME_TYPE_RESERVED;
                f.frame_attributes &= ~FRAME_ATTRIBUTE_OLD_RESERVED;
            }

            fa->allocate_frame(fa, &f);

            PRINTLOG(FRAMEALLOCATOR, LOG_TRACE, "old reserved frame start 0x%016llx count 0x%llx\n", mem_desc->physical_start, mem_desc->page_count);

        }
    }else {
        PRINTLOG(FRAMEALLOCATOR, LOG_DEBUG, "no old reserved mmap data (size: 0x%llx)", SYSTEM_INFO->reserved_mmap_size);
    }

    return fa;
}

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

    iter = linkedlist_iterator_create(ctx->free_frames_sorted_by_size);

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

}
