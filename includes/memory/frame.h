/**
 * @file frame.h
 * @brief frame allocator header
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MEMORY_FRAME_H
/*! prevent duplicate header error macro */
#define ___MEMORY_FRAME_H 0

#include <types.h>
#include <memory.h>

/*! frame size (4K) */
#define FRAME_SIZE 4096

/*! frame attribure for reserved frames before relinked start */
#define FRAME_ATTRIBUTE_OLD_RESERVED           0x0000000100000000
/*! frame attribure for acpi reclaim memory */
#define FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY    0x0000000200000000
/*! frame attribure for acpi memory */
#define FRAME_ATTRIBUTE_ACPI                   0x0000000400000000
/*! frame attribure for reserved pages which are mapped */
#define FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED   0x0000000800000000

/**
 * @enum frame_type_t
 * @brief frame types
 */
typedef enum frame_type_t {
    FRAME_TYPE_FREE, ///< free frame
    FRAME_TYPE_USED, ///< frames are allocated
    FRAME_TYPE_RESERVED, ///< frames are for reserved memory
    FRAME_TYPE_ACPI_RECLAIM_MEMORY, ///< frames for acpi area
    FRAME_TYPE_ACPI_CODE, ///< frames for acpi code
    FRAME_TYPE_ACPI_DATA, ///< frames for acpi data
} frame_type_t; ///< short hand for enum frame_type_e

/**
 * @enum frame_allocation_type_t
 * @brief frame allocation types
 */
typedef enum frame_allocation_type_t {
    FRAME_ALLOCATION_TYPE_RELAX = 1 << 1, ///< frames reserved non blockly
    FRAME_ALLOCATION_TYPE_BLOCK = 1 << 2, ///< frames should be continuous
    FRAME_ALLOCATION_TYPE_USED = 1 << 7, ///< frames for using
    FRAME_ALLOCATION_TYPE_RESERVED = 1 << 8, ///< frames for reserved area
    FRAME_ALLOCATION_TYPE_OLD_RESERVED = 1 << 15, ///<frames for old reserved area (reserved areas before relinking)
} frame_allocation_type_t; ///< short hand for enum frame_allocation_type_e

/**
 * @struct frame_t
 * @brief frame definition
 */
typedef struct frame_t {
    uint64_t     frame_address; ///< frame start address (physical)
    uint64_t     frame_count; ///< how many frames
    frame_type_t type; ///< frame type
    uint64_t     frame_attributes; ///< frame attributes
} frame_t; ///< short hand for struct frame_s

/*! opaque struct for frame_allocator_t */
struct frame_allocator_t;

/**
 * @brief allocate frame with count
 * @param[in] self frame allocator
 * @param[in] count frame count for allocation
 * @param[in] fa_type frame allocation types can be or'ed values
 * @param[out] fs list of frames that allocated
 * @param[out] alloc_list_size fs length
 * @return 0 if succeed.
 */
typedef int8_t (* fa_allocate_frame_by_count_f)(struct frame_allocator_t* self, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size);

/**
 * @brief allocate frame with given reference frame, ref frame not used in anywhere as is. it is only for reference.
 * @param[in] self frame allocator
 * @param[in] f referance frame,
 * @return 0 if succeed.
 */
typedef int8_t (* fa_allocate_frame_f)(struct frame_allocator_t* self, frame_t* f);

/**
 * @brief release frame with given reference frame, ref frame not used in anywhere as is. it is only for reference.
 * @param[in] self frame allocator
 * @param[in] f referance frame,
 * @return 0 if succeed.
 */
typedef int8_t (* fa_release_frame_f)(struct frame_allocator_t* self, frame_t* f);

/**
 * @brief returns the frame that address resides or null
 * @param[in] self frame allocator
 * @param[in] address search address
 * @return frame of given address or null
 */
typedef frame_t * (* fa_get_reserved_frames_of_address_f)(struct frame_allocator_t* self, void* address);

/**
 * @brief rebuilds reserved memory mappings
 * @param[in] self frame allocator
 * @return 0 if succeed.
 */
typedef int8_t (* fa_rebuild_reserved_mmap_f)(struct frame_allocator_t* self);

/**
 * @brief cleans old reserved frames
 * @param[in] self frame allocator
 * @return 0 if succeed.
 */
typedef int8_t (* fa_cleanup_f)(struct frame_allocator_t* self);

/**
 * @brief reserve frames for mmio
 * @param[in] self frame allocator
 * @param[in] f reference frame not used as is.
 * @return 0 if succeed.
 */

typedef int8_t (* fa_reserve_system_frames_f)(struct frame_allocator_t* self, frame_t* f);

/**
 * @brief release frames with attribute @ref FRAME_TYPE_ACPI_RECLAIM_MEMORY
 * @param[in] self frame allocator
 * @return 0 if succeed.
 */
typedef int8_t (* fa_release_acpi_reclaim_memory_f)(struct frame_allocator_t* self);

/**
 * @struct frame_allocator_t
 * @brief frame allocator class
 **/
typedef struct frame_allocator_t {
    void*                               context; ///< internal struct for frame allocator
    fa_allocate_frame_by_count_f        allocate_frame_by_count; ///< allocate frame with count
    fa_allocate_frame_f                 allocate_frame; ///< allocate frame with given reference frame, ref frame not used in anywhere as is. it is only for reference.
    fa_release_frame_f                  release_frame; ///< release frame with given reference frame, ref frame not used in anywhere as is. it is only for reference.
    fa_get_reserved_frames_of_address_f get_reserved_frames_of_address; ///< returns the frame that address resides or null
    fa_rebuild_reserved_mmap_f          rebuild_reserved_mmap; ///< rebuilds reserved memory mappings
    fa_cleanup_f                        cleanup; ///< cleans old reserved frames
    fa_reserve_system_frames_f          reserve_system_frames; ///< reserve frames for mmio
    fa_release_acpi_reclaim_memory_f    release_acpi_reclaim_memory; ///< release frames with attribute @ref FRAME_TYPE_ACPI_RECLAIM_MEMORY
} frame_allocator_t; ///< short hand for struct frame_allocator_t

/**
 * @brief creates a frame allocator
 * @param[in] heap where frame will be reside
 * @return frame allocator
 */
frame_allocator_t* frame_allocator_new_ext(memory_heap_t* heap);

/*! creates frame allocator at default heap*/
#define frame_allocator_new() frame_allocator_new_ext(NULL);

/**
 * @brief prints frame allocator contents
 * @param[in] fa frame allocator
 */
void frame_allocator_print(frame_allocator_t* fa);

/**
 * @brief map virtual pages of acpi code and data frames
 * @param[in] fa frame allocator
 */
void frame_allocator_map_page_of_acpi_code_data_frames(frame_allocator_t * fa);

/*! kernel frame allocator */
extern frame_allocator_t* KERNEL_FRAME_ALLOCATOR;

#endif
