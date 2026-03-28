/**
 * @file special_frame_addresses.h
 * @brief special frame addresses used by kernel.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___FRAMES_RESERVED_H
/*! prevent duplicate header error macro */
#define ___FRAMES_RESERVED_H 0

#include <types.h>
#include <memory/frame.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMP_TRAMPOLINE_CODE        0x8000
#define SMP_TRAMPOLINE_CODE_FRAME_COUNT 1
#define SMP_TRAMPOLINE_SHARED_DATA 0x9000
#define SMP_TRAMPOLINE_SHARED_DATA_FRAME_COUNT 1
#define SMP_TRAMPOLINE_PAGE_TABLE  0xb000
#define SMP_TRAMPOLINE_PAGE_TABLE_FRAME_COUNT 3
#define IDT_BASE_ADDRESS (1 << 20)
#define IDT_FRAME_COUNT 1

extern const frame_t special_frames_reserved_list[];
extern const uint64_t special_frames_reserved_list_size;

#ifdef __cplusplus
}
#endif

#endif
