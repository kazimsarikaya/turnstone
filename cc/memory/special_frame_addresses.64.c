/**
 * @file special_frame_addresses.64.c
 * @brief special frame addresses used by kernel.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <memory/special_frame_addresses.h>
#include <utils.h>

MODULE("turnstone.kernel.memory.frame");

const frame_t special_frames_reserved_list[] = {
    {0, SMP_TRAMPOLINE_CODE, SMP_TRAMPOLINE_CODE_FRAME_COUNT, FRAME_TYPE_UNDER_2M_RESERVED, 0},
    {0, SMP_TRAMPOLINE_SHARED_DATA, SMP_TRAMPOLINE_SHARED_DATA_FRAME_COUNT, FRAME_TYPE_UNDER_2M_RESERVED, 0},
    {0, SMP_TRAMPOLINE_PAGE_TABLE, SMP_TRAMPOLINE_PAGE_TABLE_FRAME_COUNT, FRAME_TYPE_UNDER_2M_RESERVED, 0},
    {0, IDT_BASE_ADDRESS, IDT_FRAME_COUNT, FRAME_TYPE_UNDER_2M_RESERVED, 0},
};

const uint64_t special_frames_reserved_list_size = ARRAY_SIZE(special_frames_reserved_list);
