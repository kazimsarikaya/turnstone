/**
 * @file video_edid.64.c
 * @brief EDID parser
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/video_edid.h>
#include <memory.h>
#include <video.h>


MODULE("turnstone.kernel.hw.video");

void video_edid_get_max_resolution(uint8_t* edid_data, uint32_t* max_width, uint32_t* max_height) {
    video_edid_t* edid = (video_edid_t*) edid_data;

    uint64_t max_pixels = 0;

    for(int32_t i = 0; i < 8; i++) {
        video_edid_standart_timing_t* timing = &edid->standard_timings[i];

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

        if(pixels > max_pixels) {
            max_pixels = pixels;
            *max_width = width;
            *max_height = height;
        }

    }


}
