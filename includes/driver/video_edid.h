/**
 * @file video_edid.h
 * @brief Video EDID
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef __VIDEO_EDID_H
#define __VIDEO_EDID_H 0

#include <types.h>
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct video_edid_standart_timing_t {
    uint8_t horizontal_active_pixels;
    uint8_t vertical_frequency : 6;
    uint8_t aspect_ratio       : 2;
} __attribute__((packed)) video_edid_standart_timing_t;

_Static_assert(sizeof(video_edid_standart_timing_t) == 2, "video_edid_standart_timing_t is not 2 bytes");

typedef struct video_edid_detailed_timing_t {
    uint8_t horizontal_frequency;
    uint8_t vertical_frequency;
    uint8_t horizontal_active_pixels_lsb;
    uint8_t horizontal_blanking_pixels_lsb;
    uint8_t horizontal_active_pixels_msb   : 4;
    uint8_t horizontal_blanking_pixels_msb : 4;
    uint8_t vertical_active_pixels_lsb;
    uint8_t vertical_blanking_pixels_lsb;
    uint8_t vertical_active_pixels_msb   : 4;
    uint8_t vertical_blanking_pixels_msb : 4;
    uint8_t horizontal_sync_offset;
    uint8_t horizontal_sync_pulse_width;
    uint8_t vertical_sync_offset_msb        : 4;
    uint8_t vertical_sync_pulse_width_msb   : 4;
    uint8_t horizontal_sync_pulse_width_msb : 2;
    uint8_t horizontal_sync_offset_msb      : 2;
    uint8_t vertical_sync_pulse_width       : 2;
    uint8_t vertical_sync_offset            : 2;
    uint8_t horizontal_image_size_lsb;
    uint8_t vertical_image_size_lsb;
    uint8_t horizontal_image_size_msb : 4;
    uint8_t vertical_image_size_msb   : 4;
    uint8_t horizontal_border_pixels;
    uint8_t vertical_border_pixels;
    uint8_t flags;
} __attribute__((packed)) video_edid_detailed_timing_t;

_Static_assert(sizeof(video_edid_detailed_timing_t) == 18, "video_edid_detailed_timing_t is not 18 bytes");

typedef struct video_edid_t {
    uint8_t                      header[8];
    uint16_t                     manufacturer_id;
    uint16_t                     product_id;
    uint32_t                     serial_number;
    uint8_t                      manufacture_week;
    uint8_t                      manufacture_year;
    uint8_t                      edid_version;
    uint8_t                      edid_revision;
    uint8_t                      video_input_definition;
    uint8_t                      horizontal_size;
    uint8_t                      vertical_size;
    uint8_t                      gamma;
    uint8_t                      features;
    uint8_t                      color_characteristics[10];
    uint8_t                      established_timings[3];
    video_edid_standart_timing_t standard_timings[8];
    video_edid_detailed_timing_t detailed_timings[4];
    uint8_t                      extensions;
    uint8_t                      checksum;
} __attribute__((packed)) video_edid_t;

_Static_assert(sizeof(video_edid_t) == 128, "video_edid_t is not 128 bytes");

void video_edid_get_max_resolution(uint8_t* edid_data, uint32_t* max_width, uint32_t* max_height);

#ifdef __cplusplus
}
#endif

#endif /* __VIDEO_EDID_H */
