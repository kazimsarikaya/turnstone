/**
 * @file paging.h
 * @brief memory paging and frame allocation interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___MEMORY_PAGE_H
/*! prevent duplicate header error macro */
#define ___MEMORY_PAGE_H 0

#include <memory.h>
#include <memory/frame.h>

/*! how many index has a page */
#define MEMORY_PAGING_INDEX_COUNT 512
/*! page size (4K) */
#define MEMORY_PAGING_PAGE_SIZE   0x1000

/**
 * @struct memory_page_entry_s
 * @brief page entry struct
 *
 * each entry is 64bit long
 */
typedef struct memory_page_entry_t {
    uint8_t  present               : 1; ///< bit 0 page present?
    uint8_t  writable              : 1; ///< bit 1 page can be writen?
    uint8_t  user_accessible       : 1; ///< bit 2 page can be accessable by user space
    uint8_t  write_through_caching : 1; ///< bit 3 how cache will be writen
    uint8_t  disable_cache         : 1; ///< bit 4 for disable caching of page when 1
    uint8_t  accessed              : 1; ///< bit 5 page is accessed by cpu, cpu sets this bit
    uint8_t  dirty                 : 1; ///< bit 6 page is writen, cpu sets this bits
    uint8_t  hugepage              : 1; ///< bit 7 hugepage flag for p3 (1g) and p2 (2m)
    uint8_t  global                : 1; ///< bit 8 page is global? for caching while page switches needs cr4.pge 1
    uint8_t  os_avail01            : 1; ///< bit 9 is available for os
    uint8_t  os_avail02            : 1; ///< bit 10 is available for os
    uint8_t  os_avail03            : 1; ///< bit 11 is available for os
    uint64_t physical_address      : 40; ///< bits 12-51 physical address 40 bits, shifted by 12 (long mode)
    uint8_t  os_avail04            : 1; ///< bit 52 is available for os
    uint8_t  os_avail05            : 1; ///< bit 53 is available for os
    uint8_t  os_avail06            : 1; ///< bit 54 is available for os
    uint8_t  os_avail07            : 1; ///< bit 55 is available for os
    uint8_t  os_avail08            : 1; ///< bit 56 is available for os
    uint8_t  os_avail09            : 1; ///< bit 57 is available for os
    uint8_t  os_avail10            : 1; ///< bit 58 is available for os
    uint8_t  os_avail11            : 1; ///< bit 59 is available for os or pke if cr4.pke 1 orr cr4.pks 1
    uint8_t  os_avail12            : 1; ///< bit 60 is available for os or pke if cr4.pke 1 orr cr4.pks 1
    uint8_t  os_avail13            : 1; ///< bit 61 is available for os or pke if cr4.pke 1 orr cr4.pks 1
    uint8_t  os_avail14            : 1; ///< bit 62 is available for os or pke if cr4.pke 1 orr cr4.pks 1
    uint8_t  no_execute            : 1; ///< bit 63 prevents execution of page by kernel programs
} __attribute__((packed)) memory_page_entry_t; ///< short hand for struct

/**
 * @struct memory_page_table_s
 * @brief page table struct
 *
 * in long mode a page table is consist of 512 page entries.
 * a page table is 4K page aligned.
 */
typedef struct memory_page_table_t {
    memory_page_entry_t pages[MEMORY_PAGING_INDEX_COUNT]; ///< page table entries
} __attribute__((packed)) memory_page_table_t; ///< short hand for struct

#define MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT 0x200

typedef enum memory_paging_internal_frame_init_state_t {
    MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_UNINITIALIZED = 0,
    MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING = 1,
    MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZED = 2,
} memory_paging_internal_frame_init_state_t;

typedef struct memory_page_table_context_t {
    memory_page_table_t *                     page_table; ///< page table
    memory_paging_internal_frame_init_state_t internal_frame_init_state; ///< internal frame init state
    uint64_t                                  internal_frames_1_current; ///< internal frames type 1 current
    uint64_t                                  internal_frames_1_start; ///< internal frames type 1
    uint64_t                                  internal_frames_1_count; ///< internal frames type 1 count
    uint64_t                                  internal_frames_2_start; ///< internal frames type 2
    uint64_t                                  internal_frames_2_count; ///< internal frames type 2 count
    uint64_t                                  internal_frames_helper_frame; ///< internal frames helper frame
} memory_page_table_context_t; ///< short hand for struct


/**
 * @enum memory_paging_page_type_e
 * @brief page type enum.
 */
typedef enum memory_paging_page_type_e {
    MEMORY_PAGING_PAGE_TYPE_UNKNOWN= 0, ///< 4k page
    MEMORY_PAGING_PAGE_TYPE_4K = 1 << 0, ///< 4k page
    MEMORY_PAGING_PAGE_TYPE_2M = 1 << 1, ///< 2m page aka hugepage
    MEMORY_PAGING_PAGE_TYPE_1G = 1 << 2, ///< 1g page aka big hugepage
    MEMORY_PAGING_PAGE_TYPE_READONLY = 1 << 4, ///< read only
    MEMORY_PAGING_PAGE_TYPE_NOEXEC = 1 << 5, ///< no executable
    MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE = 1 << 6, ///< no executable
    MEMORY_PAGING_PAGE_TYPE_INTERNAL = 1 << 15, ///< no executable
    MEMORY_PAGING_PAGE_TYPE_WILL_DELETED = 1 << 16, ///< no executable
} memory_paging_page_type_t; ///< short hand for enum

/*! memory alignment for a page (4k) */
#define MEMORY_PAGING_PAGE_ALIGN MEMORY_PAGING_PAGE_SIZE

/*! page length for 4K */
#define MEMORY_PAGING_PAGE_LENGTH_4K (1 << 12)
/*! page length for 2M */
#define MEMORY_PAGING_PAGE_LENGTH_2M (1 << 21)
/*! page length for 1G */
#define MEMORY_PAGING_PAGE_LENGTH_1G (1 << 30)

/**
 * @brief switches p4 page table
 * @param[in]  new_table new p4 table
 * @return        old p4 table
 *
 * change value of cr3 register.
 * if new_table is NULL returns only current table
 */
memory_page_table_context_t* memory_paging_switch_table(const memory_page_table_context_t* new_table);
/*! return p4 table */
#define memory_paging_get_table() memory_paging_switch_table(NULL)

/**
 * @brief creates virtual address frame mapping with adding page
 * @param  heap            the heap where variables will be created in
 * @param  p4              p4 page table
 * @param  virtual_address virtual start address of page
 * @param  frame_address    frame address of page links
 * @param  type            page type, see also \ref memory_paging_page_type_t
 * @return  0 if successed
 *
 * if heap is NULL, the pages created in default heap
 */
int8_t memory_paging_add_page_ext(memory_page_table_context_t* p4,
                                  uint64_t virtual_address, uint64_t frame_address,
                                  memory_paging_page_type_t type);
/*! add virtual address va to pt page table with frame address fa and page type t uses default heap for mallocs */
#define memory_paging_add_page_with_p4(pt, va, fa, t)  memory_paging_add_page_ext(pt, va, fa, t)
/*! add va and fa to defeault p4 table*/
#define memory_paging_add_page(va, fa, t)  memory_paging_add_page_ext(NULL, va, fa, t)

int8_t memory_paging_delete_page_ext_with_heap(memory_page_table_context_t* p4, uint64_t virtual_address, uint64_t* frame_address);
#define memory_paging_delete_page_ext(p4, va, faptr) memory_paging_delete_page_ext_with_heap(p4, va, faptr)
#define memory_paging_delete_page(va, faptr) memory_paging_delete_page_ext_with_heap(NULL, va, faptr)

memory_page_table_t* memory_paging_clone_pagetable_ext(memory_page_table_context_t* table_context);
#define memory_paging_clone_pagetable() memory_paging_clone_pagetable_ext(NULL)
#define memory_paging_move_pagetable(h) memory_paging_clone_pagetable_ext(NULL)

memory_page_table_t* memory_paging_clone_pagetable_to_frames_ext(memory_page_table_context_t* table_context, uint64_t fa);
#define memory_paging_clone_current_pagetable_to_frames_ext(h, fa) memory_paging_clone_pagetable_to_frames_ext(NULL, fa)
#define memory_paging_clone_current_pagetable_to_frames(fa) memory_paging_clone_pagetable_to_frames_ext(NULL, fa)

int8_t memory_paging_destroy_pagetable_ext(memory_page_table_context_t* table_context);
#define memory_paging_destroy_pagetable(p) memory_paging_destroy_pagetable_ext(p)

int8_t memory_paging_get_physical_address_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, uint64_t* physical_address);
#define memory_paging_get_physical_address(va, paptr) memory_paging_get_frame_address_ext(NULL, va, paptr)
#define memory_paging_get_frame_address_ext(p4, va, fa) ((memory_paging_get_physical_address_ext(p4, va, fa) >> 12) << 12)
#define memory_paging_get_frame_address(va, faptr) memory_paging_get_frame_address_ext(NULL, va, faptr)


int8_t memory_paging_toggle_attributes_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, memory_paging_page_type_t type);
#define memory_paging_toggle_attributes(va, t) memory_paging_toggle_attributes_ext(NULL, va, t)
int8_t memory_paging_set_user_accessible_ext(memory_page_table_context_t* table_context, uint64_t virtual_address);
#define memory_paging_set_user_accessible(va) memory_paging_set_user_accessible_ext(NULL, va)

typedef enum memory_paging_clear_type_t {
    MEMORY_PAGING_CLEAR_TYPE_DIRTY =1,
    MEMORY_PAGING_CLEAR_TYPE_ACCESSED=2,
} memory_paging_clear_type_t;

int8_t memory_paging_clear_page_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, memory_paging_clear_type_t type);
#define memory_paging_clear_page(va, t) memory_paging_clear_page_ext(NULL, va, t)

/*! gets p4 index of virtual address at long mode */
#define MEMORY_PT_GET_P4_INDEX(u64) ((u64 >> 39) & 0x1FF)
/*! gets p3 index of virtual address at long mode */
#define MEMORY_PT_GET_P3_INDEX(u64) ((u64 >> 30) & 0x1FF)
/*! gets p2 index of virtual address at long mode */
#define MEMORY_PT_GET_P2_INDEX(u64) ((u64 >> 21) & 0x1FF)
/*! gets p1 index of virtual address at long mode */
#define MEMORY_PT_GET_P1_INDEX(u64) ((u64 >> 12) & 0x1FF)

#if ___KERNELBUILD == 1
/*! returns frame address of reserved virtual address */
#define MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(va)  ((typeof(va))(((64ULL << 40 ) - 1) & (uint64_t)(va)))
/*! computes reserved virtual address of frame adddress */
#define MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(fa)  ((typeof(fa))((64ULL << 40) | (uint64_t)(fa)))
#else
#define MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(va)  ((typeof(va))((uint64_t)(va)))
#define MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(fa)  ((typeof(fa))((uint64_t)(fa)))
#endif

int8_t memory_paging_add_va_for_frame_ext(memory_page_table_context_t* table_context, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type);
#define memory_paging_add_va_for_frame(vas, f, t) memory_paging_add_va_for_frame_ext(NULL, vas, f, t)

int8_t memory_paging_delete_va_for_frame_ext(memory_page_table_context_t* table_context, uint64_t va_start, frame_t* frm);
#define memory_paging_delete_va_for_frame(vas, f) memory_paging_add_va_for_frame_ext(NULL, vas, f)

memory_page_table_context_t* memory_paging_build_empty_table(uint64_t internal_frame_address);
int8_t                       memory_paging_reserve_current_page_table_frames(void);
#endif
