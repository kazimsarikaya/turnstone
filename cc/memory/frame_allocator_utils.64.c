/**
 * @file frame_allocator_utils.64.c
 * @brief Physical Frame allocator implementation for 64-bit systems.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <memory/frame.h>

MODULE("turnstone.kernel.memory.frame.utils");

static frame_allocator_t* frame_allocator_default = NULL;

frame_allocator_t* frame_get_allocator(void) {
    return frame_allocator_default;
}

void frame_set_allocator(frame_allocator_t* fa) {
    frame_allocator_default = fa;
}
