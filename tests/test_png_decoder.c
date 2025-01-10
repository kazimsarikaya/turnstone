/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000 // 16 MB
#include "setup.h"
#include <strings.h>
#include <buffer.h>
#include <crc.h>
#include <compression.h>
#include <zpack.h>
#include <deflate.h>
#include <quicksort.h>
#include <graphics/png.h>
#include <errno.h>

static void cleanup_file(FILE** file) {
    if(*file) {
        fclose(*file);
        *file = NULL;
    }
}

#if 0
static void cleanup_buffer(buffer_t** buffer) {
    if(*buffer) {
        buffer_destroy(*buffer);
        *buffer = NULL;
    }
}
#endif

static void cleanup_memory(void** memory) {
    if(*memory) {
        memory_free(*memory);
        *memory = NULL;
    }
}

#define defervar(var) \
        __attribute__((cleanup(cleanup_memory))) void* _cleanup_ ## var ## _ ## __LINE__ = (void*)var;



int32_t main(uint32_t argc, char_t** argv);

int32_t main(uint32_t argc, char_t** argv) {
    crc32_init_table();

    if(argc != 2) {
        print_error("not enough paramters");
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "usage png_decoder <pngfile>");

        return -1;
    }

    argc--;
    argv++;

    const char_t* png_file = *argv;

    if(strlen(png_file) == 0) {
        print_error("invalid file name");
        return -1;
    }

    __attribute__((cleanup(cleanup_file))) FILE* file = fopen(png_file, "rb");

    if(!file) {
        print_error("file not found");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    uint64_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if(file_size == 0) {
        print_error("file is empty");
        return -1;
    }

    uint8_t* file_data = memory_malloc(file_size);
    defervar(file_data);

    if(!file_data) {
        print_error("memory allocation failed");
        return -1;
    }

    uint64_t read_size = fread(file_data, 1, file_size, file);

    if(read_size != file_size) {
        print_error("file read failed read size is not equal to file size: %lli != %lli", read_size, file_size);
        return -1;
    }



    graphics_raw_image_t* image = graphics_load_png_image(file_data, file_size);

    if(!image) {
        print_error("png decoder failed to build image");

        return -1;
    }

    print_success("png file decoded successfully");

    FILE* out = fopen("tmp/out.raw", "wb");
    fwrite(image->data, 1, image->width * image->height * sizeof(pixel_t), out);
    fclose(out);

    uint64_t new_png_size = 0;
    uint8_t* new_png_data = graphics_save_png_image(image, &new_png_size);
    defervar(new_png_data);

    if(!new_png_data) {
        print_error("png encoder failed to build image");

        return -1;
    }

    FILE* out2 = fopen("tmp/out.png", "wb");
    fwrite(new_png_data, 1, new_png_size, out2);
    fclose(out2);

    memory_free(image->data);
    memory_free(image);

    return 0;
}
