/**
 * @file windowmanager_init.64.c
 * @brief Window Manager implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <video.h>
#include <logging.h>
#include <memory.h>

MODULE("turnstone.user.programs.windowmanager.init");

boolean_t windowmanager_initialized = false;

boolean_t windowmanager_is_initialized(void) {
    return windowmanager_initialized;
}
