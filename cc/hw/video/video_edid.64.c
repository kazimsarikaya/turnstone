/**
 * @file video_edid.64.c
 * @brief EDID parser
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/video_edid.h>
#include <memory.h>
#include <logging.h>
#include <device/mmio.h>


MODULE("turnstone.kernel.hw.video");

typedef struct video_edid_established_timing_list_t {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
} video_edid_established_timing_list_t;

static const video_edid_established_timing_list_t established_timings_list[] = {
    // byte 0
    { 800,  600,  60},
    { 800,  600,  56},
    { 640,  480,  75},
    { 640,  480,  72},
    { 640,  480,  67},
    { 640,  480,  60},
    { 720,  400,  88},
    { 720,  400,  70},
    // byte 1
    {1280, 1024,  75},
    {1024,  768,  75},
    {1024,  768,  70},
    {1024,  768,  60},
    {1024,  768,  87},
    { 832,  624,  75},
    { 800,  600,  75},
    { 800,  600,  72},
    // byte 3
    {   0,    0,   0},
    {   0,    0,   0},
    {   0,    0,   0},
    {   0,    0,   0},
    {   0,    0,   0},
    {   0,    0,   0},
    {1152,  870,  75}
};

int8_t __attribute__((target("general-regs-only")))  video_edid_get_max_resolution(uint8_t* edid_data, uint32_t* max_width, uint32_t* max_height) {
    video_edid_t* edid = (video_edid_t*) edid_data;

    if(edid == NULL || edid_data == NULL || max_width == NULL || max_height == NULL) {
        PRINTLOG(VIDEO, LOG_ERROR, "Invalid parameters");
        return -1;
    }

    uint64_t header = mmio_read((uint64_t)edid_data, 8);

    if(header != 0x00ffffffffffff00) {
        PRINTLOG(VIDEO, LOG_ERROR, "EDID header is invalid: 0x%llx", header);
        return -1;
    }

    uint8_t checksum = 0;

    for(int32_t i = 0; i < 128; i++) {
        checksum += mmio_read((uint64_t)(edid_data + i), 1);
    }

    if(checksum != 0) {
        PRINTLOG(VIDEO, LOG_ERROR, "EDID checksum is invalid: 0x%x", checksum);
        return -1;
    }

    uint64_t max_pixels = 0;

    boolean_t timing_found = false;

    for(size_t i = 0; i < ARRAY_SIZE(edid->established_timings); i++) {
        uint8_t timing = edid->established_timings[i];

        if(timing == 0) {
            continue;
        }

        for(int32_t j = 0; j < 8; j++) {
            if(timing & BIT(j)) {
                uint32_t width = established_timings_list[i * 8 + j].width;
                uint32_t height = established_timings_list[i * 8 + j].height;
                uint32_t refresh_rate = established_timings_list[i * 8 + j].refresh_rate;
                uint64_t pixels = width * height;

                if(pixels == 0) {
                    continue;
                }

                PRINTLOG(VIDEO, LOG_INFO, "EDID established timing found: %dx%d@%dHz", width, height, refresh_rate);

                if(pixels > max_pixels) {
                    timing_found = true;
                    max_pixels = pixels;
                    *max_width = width;
                    *max_height = height;
                }
            }
        }
    }

    for(size_t i = 0; i < ARRAY_SIZE(edid->standard_timings); i++) {
        video_edid_standart_timing_t* timing = &edid->standard_timings[i];

        if(timing->raw == 0x0101) {
            continue;
        }

        if(timing->horizontal_active_pixels == 0) {
            continue;
        }

        uint16_t width = (timing->horizontal_active_pixels + 31) * 8;
        uint16_t height = 0;

        switch(timing->aspect_ratio) {
        case 0:
            height = width * 10 / 16;
            break;
        case 1:
            height = width * 3 / 4;
            break;
        case 2:
            height = width * 4 / 5;
            break;
        case 3:
            height = width * 9 / 16;
            break;
        default:
            height = width * 10 / 16;
            break;
        }

        uint64_t pixels = width * height;

        PRINTLOG(VIDEO, LOG_INFO, "EDID standard timing found: %dx%d@%dHz", width, height, timing->vertical_frequency + 60);

        if(pixels > max_pixels) {
            timing_found = true;
            max_pixels = pixels;
            *max_width = width;
            *max_height = height;
        }

    }

    return timing_found ? 0 : -1;

    // TODO: parse detailed timings and extensions
    for(size_t i = 0; i < ARRAY_SIZE(edid->detailed_timings); i++) {
        video_edid_detailed_timing_t* timing = &edid->detailed_timings[i];

        if(timing->horizontal_frequency == 0 || timing->vertical_frequency == 0) {
            continue;
        }

        uint16_t width = ((timing->horizontal_active_pixels_msb << 8) | timing->horizontal_active_pixels_lsb);
        uint16_t height = ((timing->vertical_active_pixels_msb << 8) | timing->vertical_active_pixels_lsb);
        uint64_t pixels = width * height;

        PRINTLOG(VIDEO, LOG_INFO, "EDID detailed timing found: %dx%d", width, height);

        if(pixels > max_pixels) {
            timing_found = true;
            max_pixels = pixels;
            *max_width = width;
            *max_height = height;
        }
    }

    return timing_found ? 0 : -1;
}
