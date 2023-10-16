/**
 * @file setup.h
 * @brief EFI Setup header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SETUP_H
/*! macro for avoiding multiple inclusion */
#define ___SETUP_H 0

#include <types.h>
#include <logging.h>
#include <efi.h>
#include <disk.h>
#include <memory.h>

/**
 * @brief opens an efi protocol based disk
 * @param[in] bio efi block io protocol
 * @return a disk implementation based on efi protocols.
 */
disk_t* efi_disk_impl_open(efi_block_io_t* bio);

/*! opaque type for frame allocator */
typedef struct frame_allocator_t frame_allocator_t;

/**
 * @brief initializes frame allocator
 * @return status of initialization
 */
efi_status_t efi_frame_allocator_init(void);

/*! opaque type for time_t */
typedef uint64_t time_t;

/**
 * @brief gets current time in nanoseconds
 * @param[out] t pointer to time_t
 * @return current time in nanoseconds
 */
time_t time_ns(time_t* t);

#endif
