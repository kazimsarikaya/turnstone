/**
 * @file image.64.c
 * @brief image converter methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <graphics/tga.h>
#include <memory.h>

MODULE("turnstone.kernel.graphics.image");

graphics_raw_image_t* graphics_load_tga_image(graphics_tga_image_t* tga, uint32_t size) {
    if(!tga) {
        return NULL;
    }

    if(size < sizeof(graphics_tga_image_t)) {
        return NULL;
    }

    graphics_raw_image_t* image = memory_malloc(sizeof(graphics_raw_image_t));

    if(!image) {
        return NULL;
    }

    image->width = tga->width;
    image->height = tga->height;

    image->data = memory_malloc(image->width * image->height * sizeof(pixel_t));

    if(!image->data) {
        memory_free(image);
        return NULL;
    }

    uint32_t data_offset = sizeof(graphics_tga_image_t) + (tga->color_map? tga->color_map_length * tga->color_map_entry_size : 0);

    uint32_t i = 0, j = 0, k = 0, x = 0, y = 0;

    uint8_t* data = (uint8_t*)tga;

    if(tga->encoding == 1) {
        if(tga->color_map_length != 0 ||
           tga->color_map_origin != 0 ||
           (tga->color_map_entry_size != 24 && tga->color_map_entry_size != 32)) {
            memory_free(image->data);
            memory_free(image);

            return NULL;
        }

        for(y = i = 0; y < tga->height; y++) {
            k = ((!tga->y_origin?tga->height - y - 1:y) * tga->width);

            for(x = 0; x < tga->width; x++) {
                j = data[data_offset + k++] * (tga->color_map_entry_size >> 3) + 18;
                image->data[i++] = ((tga->color_map_entry_size == 32?data[j + 3]:0xFF) << 24) | // alpha
                                   (data[j + 2] << 16) | // red
                                   (data[j + 1] << 8) | // green
                                   data[j]; // blue
            }
        }
    } else if(tga->encoding == 2) {
        if(tga->color_map != 0 ||
           tga->color_map_origin != 0 ||
           tga->color_map_entry_size != 0 ||
           (tga->bpp != 24 && tga->bpp != 32)) {
            memory_free(image->data);
            memory_free(image);

            return NULL;
        }

        for(y = i = 0; y < tga->height; y++) {
            j = ((!tga->y_origin?tga->height - y - 1:y) * tga->width * (tga->bpp >> 3));

            for(x = 0; x < tga->width; x++) {
                image->data[i++] = ((tga->bpp == 32?data[j + 3]:0xFF) << 24) | // alpha
                                   (data[j + 2] << 16) | // red
                                   (data[j + 1] << 8) | // green
                                   data[j]; // blue
                j += tga->bpp >> 3;
            }
        }
    } else if(tga->encoding == 9) {
        if(tga->color_map_length != 0 ||
           tga->color_map_origin != 0 ||
           (tga->color_map_entry_size != 24 && tga->color_map_entry_size != 32)) {
            memory_free(image->data);
            memory_free(image);

            return NULL;
        }

        y = i = 0;

        for(x = 0; x < tga->width * tga->height && data_offset < size;) {
            k = data[data_offset++];

            if(k > 127) {
                k -= 127;
                x += k;
                j = data[data_offset++] * (tga->color_map_entry_size >> 3) + 18;

                while(k--) {
                    if(!(i % tga->width)) {
                        i = ((!tga->y_origin?tga->height - y - 1:y) * tga->width);
                        y++;
                    }

                    image->data[i++] = ((tga->color_map_entry_size == 32?data[j + 3]:0xFF) << 24) | // alpha
                                       (data[j + 2] << 16) | // red
                                       (data[j + 1] << 8) | // green
                                       data[j]; // blue
                }
            } else {
                k++;
                x += k;

                while(k--) {
                    j = data[data_offset++] * (tga->color_map_entry_size >> 3) + 18;

                    if(!(i % tga->width)) {
                        i = ((!tga->y_origin?tga->height - y - 1:y) * tga->width);
                        y++;
                    }

                    image->data[i++] = ((tga->color_map_entry_size == 32?data[j + 3]:0xFF) << 24) | // alpha
                                       (data[j + 2] << 16) | // red
                                       (data[j + 1] << 8) | // green
                                       data[j]; // blue
                }
            }
        }
    } else if(tga->encoding == 10) {
        if(tga->color_map != 0 ||
           tga->color_map_origin != 0 ||
           tga->color_map_entry_size != 0 ||
           (tga->bpp != 24 && tga->bpp != 32)) {
            memory_free(image->data);
            memory_free(image);

            return NULL;
        }

        y = i = 0;

        for(x = 0; x < tga->width * tga->height && data_offset < size;) {
            k = data[data_offset++];

            if(k > 127) {
                k -= 127;
                x += k;

                while(k--) {
                    if(!(i % tga->width)) {
                        i = ((!tga->y_origin?tga->height - y - 1:y) * tga->width);
                        y++;
                    }

                    image->data[i++] = ((tga->bpp == 32?data[data_offset + 3]:0xFF) << 24) | // alpha
                                       (data[data_offset + 2] << 16) | // red
                                       (data[data_offset + 1] << 8) | // green
                                       data[data_offset]; // blue
                }

                data_offset += tga->bpp >> 3;
            } else {
                k++;
                x += k;

                while(k--) {
                    if(!(i % tga->width)) {
                        i = ((!tga->y_origin?tga->height - y - 1:y) * tga->width);
                        y++;
                    }

                    image->data[i++] = ((tga->bpp == 32?data[data_offset + 3]:0xFF) << 24) | // alpha
                                       (data[data_offset + 2] << 16) | // red
                                       (data[data_offset + 1] << 8) | // green
                                       data[data_offset]; // blue

                    data_offset += tga->bpp >> 3;
                }
            }
        }

    } else {
        // Unsupported encoding
        memory_free(image->data);
        memory_free(image);
        return NULL;
    }

    return image;
}

