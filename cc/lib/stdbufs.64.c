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

#if ___KERNELBUILD == 1
#include <spool.h>
#endif

MODULE("turnstone.lib.stdbufs");


buffer_t* stdbufs_default_input_buffer = NULL;
buffer_t* stdbufs_default_output_buffer = NULL;
buffer_t* stdbufs_default_error_buffer = NULL;

static void stdbufs_video_null_printer(const char_t* text) {
    // do nothing
    UNUSED(text);
}

stdbufs_video_printer stdbufs_video_print = stdbufs_video_null_printer;

int8_t stdbufs_init_buffers(stdbufs_video_printer video_printer) {
    stdbufs_video_print = video_printer;

    memory_heap_t* heap = NULL;

#if ___KERNELBUILD == 1
    heap = spool_get_heap();
#endif

    if (stdbufs_default_input_buffer == NULL) {
        stdbufs_default_input_buffer = buffer_new_with_capacity(heap, 1024);

        if(!stdbufs_default_input_buffer) {
            return -1;
        }
    }

    if (stdbufs_default_output_buffer == NULL) {
        stdbufs_default_output_buffer = buffer_new_with_capacity(heap, 1024);

        if(!stdbufs_default_output_buffer) {
            return -1;
        }
    }

    if (stdbufs_default_error_buffer == NULL) {
        stdbufs_default_error_buffer = buffer_new_with_capacity(heap, 1024);

        if(!stdbufs_default_error_buffer) {
            return -1;
        }
    }

#if ___KERNELBUILD == 1
    spool_add("stdbufs", 3, stdbufs_default_input_buffer, stdbufs_default_output_buffer, stdbufs_default_error_buffer);
#endif

    return 0;
}


#if ___TESTMODE != 1

#include <windowmanager.h>

typedef buffer_t * (*stdbuf_task_buffer_getter_f)(void);
stdbuf_task_buffer_getter_f stdbufs_task_get_input_buffer = NULL;
stdbuf_task_buffer_getter_f stdbufs_task_get_output_buffer = NULL;
stdbuf_task_buffer_getter_f stdbufs_task_get_error_buffer = NULL;


static buffer_t* stdbufs_get_task_get_input_buffer(void) {
    if(stdbufs_task_get_input_buffer) {
        return stdbufs_task_get_input_buffer();
    }

    return NULL;
}

static buffer_t* stdbufs_get_task_get_output_buffer(void) {
    if(stdbufs_task_get_output_buffer) {
        return stdbufs_task_get_output_buffer();
    }

    return NULL;
}

static buffer_t* stdbufs_get_task_get_error_buffer(void) {
    if(stdbufs_task_get_error_buffer) {
        return stdbufs_task_get_error_buffer();
    }

    return NULL;
}


buffer_t* buffer_get_io_buffer(uint64_t buffer_io_id) {
    buffer_t* buffer = NULL;

    switch (buffer_io_id) {
    case BUFFER_IO_INPUT:
        buffer = stdbufs_get_task_get_input_buffer();

        if(!buffer) {
            buffer = stdbufs_default_input_buffer;
        }

        break;
    case BUFFER_IO_OUTPUT:
        buffer = stdbufs_get_task_get_output_buffer();

        if(!buffer) {
            buffer = stdbufs_default_output_buffer;
        }

        break;
    case BUFFER_IO_ERROR:
        buffer = stdbufs_get_task_get_error_buffer();

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

    int64_t ret = buffer_vprintf(buffer, format, ap);

    if(!windowmanager_is_initialized()) {
        stdbufs_flush_buffer(buffer);
    }

    return ret;
}

#endif

int64_t stdbufs_flush_buffer(buffer_t* buffer) {
    uint64_t new_position = buffer_get_length(buffer);
    uint64_t old_position = buffer_get_mark_position(buffer);

    uint64_t length = new_position - old_position;

    if (length == 0) {
        return 0;
    }

    char_t* buffer_data = (char_t*)buffer_get_view_at_position(buffer, old_position, new_position - old_position);

    if (buffer_data == NULL) {
        return -1;
    }

    stdbufs_video_print(buffer_data);

    buffer_set_mark_position(buffer, new_position);

    return strlen(buffer_data);
}
