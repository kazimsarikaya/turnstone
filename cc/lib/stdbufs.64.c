/**
 * @file stdbufs.64.c
 * @brief standard buffers for input and output
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <stdbufs.h>
#include <cpu/task.h>
#include <video.h>
#include <strings.h>

MODULE("turnstone.lib");


buffer_t* stdbufs_default_input_buffer = NULL;
buffer_t* stdbufs_default_output_buffer = NULL;
buffer_t* stdbufs_default_error_buffer = NULL;

extern boolean_t windowmanager_initialized;
uint64_t stdbufs_default_output_buffer_last_position = 0;

int8_t stdbufs_init_buffers(void) {
    if (stdbufs_default_input_buffer == NULL) {
        stdbufs_default_input_buffer = buffer_new_with_capacity(NULL, 1024);

        if(!stdbufs_default_input_buffer) {
            return -1;
        }
    }

    if (stdbufs_default_output_buffer == NULL) {
        stdbufs_default_output_buffer = buffer_new_with_capacity(NULL, 1024);

        if(!stdbufs_default_output_buffer) {
            return -1;
        }
    }

    if (stdbufs_default_error_buffer == NULL) {
        stdbufs_default_error_buffer = buffer_new_with_capacity(NULL, 1024);

        if(!stdbufs_default_error_buffer) {
            return -1;
        }
    }

    return 0;
}


#if ___TESTMODE != 1

buffer_t* buffer_get_io_buffer(uint64_t buffer_io_id) {
    buffer_t* buffer = NULL;
    switch (buffer_io_id) {
    case BUFFER_IO_INPUT:
        buffer = task_get_input_buffer();
        if(!buffer) {
            buffer = stdbufs_default_input_buffer;
        }
        break;
    case BUFFER_IO_OUTPUT:
        buffer = task_get_output_buffer();
        if(!buffer) {
            buffer = stdbufs_default_output_buffer;
        }
        break;
    case BUFFER_IO_ERROR:
        buffer = task_get_error_buffer();
        if(!buffer) {
            buffer = stdbufs_default_error_buffer;
        }
        break;
    default:
        break;
    }
    return buffer;
}

int64_t printf(const char * format, ...) {
    va_list ap;
    va_start(ap, format);
    int64_t ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

int64_t vprintf(const char * format, va_list ap) {
    buffer_t* buffer = buffer_get_io_buffer(BUFFER_IO_OUTPUT);

    stdbufs_default_output_buffer_last_position = buffer_get_length(buffer);

    int64_t ret = buffer_vprintf(buffer, format, ap);

    if(!windowmanager_initialized) {
        stdbufs_flush_buffer(buffer, stdbufs_default_output_buffer_last_position);
    }

    return ret;
}

#endif

int64_t stdbufs_flush_buffer(buffer_t* buffer, uint64_t old_position) {
    uint64_t new_position = buffer_get_length(buffer);

    char_t* buffer_data = (char_t*)buffer_get_view_at_position(buffer, old_position, new_position - old_position);

    video_print(buffer_data);

    return strlen(buffer_data);
}
