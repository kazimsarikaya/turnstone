/**
 * @file usb_audo.64.c
 * @brief USB Audio driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <logging.h>
#include <math.h>


MODULE("turnstone.kernel.hw.usb");

/*
 * Class-specific control requests
 */
#define USB_AUDIO_CR_SET_CUR      0x01
#define USB_AUDIO_CR_GET_CUR      0x81
#define USB_AUDIO_CR_SET_MIN      0x02
#define USB_AUDIO_CR_GET_MIN      0x82
#define USB_AUDIO_CR_SET_MAX      0x03
#define USB_AUDIO_CR_GET_MAX      0x83
#define USB_AUDIO_CR_SET_RES      0x04
#define USB_AUDIO_CR_GET_RES      0x84
#define USB_AUDIO_CR_SET_MEM      0x05
#define USB_AUDIO_CR_GET_MEM      0x85
#define USB_AUDIO_CR_GET_STAT     0xff

/*
 * Feature Unit Control Selectors
 */
#define USB_AUDIO_MUTE_CONTROL                    0x01
#define USB_AUDIO_VOLUME_CONTROL                  0x02
#define USB_AUDIO_BASS_CONTROL                    0x03
#define USB_AUDIO_MID_CONTROL                     0x04
#define USB_AUDIO_TREBLE_CONTROL                  0x05
#define USB_AUDIO_GRAPHIC_EQUALIZER_CONTROL       0x06
#define USB_AUDIO_AUTOMATIC_GAIN_CONTROL          0x07
#define USB_AUDIO_DELAY_CONTROL                   0x08
#define USB_AUDIO_BASS_BOOST_CONTROL              0x09
#define USB_AUDIO_LOUDNESS_CONTROL                0x0a

typedef enum usb_audio_ac_sub_desc_type_t {
    USB_AUDIO_AC_DESCRIPTOR_UNDEFINED = 0x00,
    USB_AUDIO_AC_HEADER = 0x01,
    USB_AUDIO_AC_INPUT_TERMINAL = 0x02,
    USB_AUDIO_AC_OUTPUT_TERMINAL = 0x03,
    USB_AUDIO_AC_MIXER_UNIT = 0x04,
    USB_AUDIO_AC_SELECTOR_UNIT = 0x05,
    USB_AUDIO_AC_FEATURE_UNIT = 0x06,
    USB_AUDIO_AC_PROCESSING_UNIT = 0x07,
    USB_AUDIO_AC_EXTENSION_UNIT = 0x08,
} usb_audio_ac_sub_desc_type_t;

typedef struct usb_audio_cs_dummy_desc_t {
    uint8_t length;
    uint8_t type;
    uint8_t subtype;
    uint8_t extra_data[];
}__attribute__((packed)) usb_audio_cs_dummy_desc_t;

typedef struct usb_audio_ac_input_terminal_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint8_t  subtype;
    uint8_t  terminal_id;
    uint16_t terminal_type;
    uint8_t  assoc_terminal;
    uint8_t  num_channels;
    uint16_t channel_config;
    uint8_t  channel_names;
    uint8_t  extra_data[];
}__attribute__((packed)) usb_audio_ac_input_terminal_desc_t;

typedef struct usb_audio_ac_output_terminal_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint8_t  subtype;
    uint8_t  terminal_id;
    uint16_t terminal_type;
    uint8_t  assoc_terminal;
    uint8_t  source_id;
    uint8_t  extra_data[];
}__attribute__((packed)) usb_audio_ac_output_terminal_desc_t;

typedef struct usb_audio_ac_feature_unit_desc_t {
    uint8_t length;
    uint8_t type;
    uint8_t subtype;
    uint8_t unit_id;
    uint8_t source_id;
    uint8_t control_size;
    uint8_t extra_data[];
}__attribute__((packed)) usb_audio_ac_feature_unit_desc_t;

typedef enum usb_audio_driver_type_t {
    USB_AUDIO_DRIVER_TYPE_CONTROL = 0,
    USB_AUDIO_DRIVER_TYPE_STREAMING = 1,
} usb_audio_driver_type_t;

typedef struct usb_audio_channel_volume_t {
    int16_t volume;
    int16_t min_volume;
    int16_t max_volume;
    int16_t volume_resolution;
} usb_audio_channel_volume_t;

typedef struct usb_driver_t {
    usb_device_t*                        usb_device;
    usb_interface_t*                     interface;
    usb_pipeline_callback_f              pipeline_callback;
    usb_audio_driver_type_t              driver_type;
    int32_t                              num_channels;
    boolean_t                            is_muted;
    usb_audio_channel_volume_t*          channels;
    usb_audio_ac_output_terminal_desc_t* output_terminal;
    usb_audio_ac_input_terminal_desc_t * input_terminal;
    usb_audio_ac_feature_unit_desc_t*    feature_unit;
} usb_driver_t;

static int16_t* usb_audio_beep = NULL;
static size_t usb_audio_beep_samples = 0;

int8_t usb_audio_control_init(usb_device_t* device, usb_interface_t* interface) {
    if(!device || !interface || !interface->desc) {
        PRINTLOG(USB, LOG_ERROR, "invalid params");
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "audio control device found");

    usb_driver_t* driver = memory_malloc(sizeof(usb_driver_t));

    if(!driver) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for driver");
        return -1;
    }

    driver->usb_device = device;
    driver->interface  = interface;
    driver->pipeline_callback = NULL;
    driver->driver_type = USB_AUDIO_DRIVER_TYPE_CONTROL;

    interface->driver = driver;

    for(uint32_t i = 0; i < interface->num_cs_interfaces; i++) {
        usb_audio_cs_dummy_desc_t* cs_desc = (usb_audio_cs_dummy_desc_t*)interface->cs_interfaces[i];

        if(!cs_desc || cs_desc->length < 3) {
            continue;
        }

        if(cs_desc->subtype == USB_AUDIO_AC_OUTPUT_TERMINAL) {
            driver->output_terminal = (usb_audio_ac_output_terminal_desc_t*)cs_desc;
            PRINTLOG(USB, LOG_INFO, "found output terminal with id %d", driver->output_terminal->terminal_id);
        } else if(cs_desc->subtype == USB_AUDIO_AC_FEATURE_UNIT) {
            driver->feature_unit = (usb_audio_ac_feature_unit_desc_t*)cs_desc;
            PRINTLOG(USB, LOG_INFO, "found feature unit with id %d", driver->feature_unit->unit_id);
        } else if(cs_desc->subtype == USB_AUDIO_AC_INPUT_TERMINAL) {
            driver->input_terminal = (usb_audio_ac_input_terminal_desc_t*)cs_desc;
            PRINTLOG(USB, LOG_INFO, "found input terminal with id %d", driver->input_terminal->terminal_id);
        }
    }

    if(!driver->output_terminal) {
        PRINTLOG(USB, LOG_ERROR, "cannot find output terminal of audio device");
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    if(!driver->input_terminal) {
        PRINTLOG(USB, LOG_ERROR, "cannot find input terminal of audio device");
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    if(!driver->feature_unit) {
        PRINTLOG(USB, LOG_ERROR, "cannot find feature unit of audio device");
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    int32_t num_channels = driver->input_terminal->num_channels;

    if(num_channels <= 0 || num_channels > 2) {
        PRINTLOG(USB, LOG_ERROR, "invalid number of channels %d of audio device", num_channels);
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    driver->num_channels = num_channels;
    driver->channels = memory_malloc(sizeof(usb_audio_channel_volume_t) * num_channels);

    if(!driver->channels) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for channels");
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    uint16_t index = (driver->feature_unit->unit_id << 8) | interface->desc->interface_number;


    if(!usb_device_request(device,
                           interface,
                           USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_AUDIO_CR_GET_CUR,
                           (USB_AUDIO_MUTE_CONTROL << 8) | 0x00, index,
                           1, &driver->is_muted)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get mute status of audio device");
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    for(int32_t i = 0; i < driver->num_channels; i++) {
        driver->channels[i].volume = 0;

        if(!usb_device_request(device,
                               interface,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_AUDIO_CR_GET_CUR,
                               (USB_AUDIO_VOLUME_CONTROL << 8) | (i + 1), index,
                               2, &driver->channels[i].volume)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get volume of channel %d of audio device", i);
            memory_free(driver);
            interface->driver = NULL;
            return -1;
        }

        driver->channels[i].min_volume = 0;

        if(!usb_device_request(device,
                               interface,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_AUDIO_CR_GET_MIN,
                               (USB_AUDIO_VOLUME_CONTROL << 8) | (i + 1), index,
                               2, &driver->channels[i].min_volume)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get min volume of channel %d of audio device", i);
            memory_free(driver);
            interface->driver = NULL;
            return -1;
        }

        driver->channels[i].max_volume = 0;

        if(!usb_device_request(device,
                               interface,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_AUDIO_CR_GET_MAX,
                               (USB_AUDIO_VOLUME_CONTROL << 8) | (i + 1), index,
                               2, &driver->channels[i].max_volume)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get max volume of channel %d of audio device", i);
            memory_free(driver);
            interface->driver = NULL;
            return -1;
        }

        driver->channels[i].volume_resolution = 0;

        if(!usb_device_request(device,
                               interface,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_AUDIO_CR_GET_RES,
                               (USB_AUDIO_VOLUME_CONTROL << 8) | (i + 1), index,
                               2, &driver->channels[i].volume_resolution)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get volume resolution of channel %d of audio device", i);
            memory_free(driver);
            interface->driver = NULL;
            return -1;
        }

    }

    PRINTLOG(USB, LOG_INFO, "audio device mute status: %s", driver->is_muted ? "muted" : "unmuted");
    for(int32_t i = 0; i < driver->num_channels; i++) {
        PRINTLOG(USB, LOG_INFO, "audio device channel %d volume: %d (min: %d, max: %d, res: %d)",
                 i,
                 driver->channels[i].volume,
                 driver->channels[i].min_volume,
                 driver->channels[i].max_volume,
                 driver->channels[i].volume_resolution);
    }

    // set volume to max value
    for(int32_t i = 0; i < driver->num_channels; i++) {
        int16_t volume = driver->channels[i].max_volume;
        if(!usb_device_request(device,
                               interface,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                               USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_AUDIO_CR_SET_CUR,
                               (USB_AUDIO_VOLUME_CONTROL << 8) | (i + 1), index,
                               2, &volume)) {
            PRINTLOG(USB, LOG_ERROR, "cannot set volume of channel %d of audio device", i);
            memory_free(driver);
            interface->driver = NULL;
            return -1;
        }
        driver->channels[i].volume = volume;
    }


    PRINTLOG(USB, LOG_INFO, "audio control device initialized");

    return 0;
}


static int16_t* usb_audio_generate_beep(float64_t freq, int32_t duration_ms,
                                        int32_t amplitude, int32_t sample_rate,
                                        size_t* out_samples) {
    size_t total_samples = (size_t)((duration_ms / 1000.0) * sample_rate);
    *out_samples = total_samples;

    int16_t* buffer = memory_malloc(total_samples * 2 * sizeof(int16_t)); // stereo
    if (!buffer) return NULL;

    float64_t phase_step = 2.0 * PI * freq / sample_rate;
    float64_t phase = 0.0;

    for (size_t i = 0; i < total_samples; i++) {
        int16_t sample = (int16_t)(math_sin(phase) * amplitude);
        buffer[2 * i]     = sample; // Left
        buffer[2 * i + 1] = sample; // Right
        phase += phase_step;
        if (phase > 2.0 * PI) phase -= 2.0 * PI;
    }

    return buffer;
}

int8_t usb_audio_streaming_init(usb_device_t* device, usb_interface_t* interface) {
    if(!device || !interface || !interface->desc) {
        PRINTLOG(USB, LOG_ERROR, "invalid params");
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "audio streaming device found");

    usb_driver_t* driver = memory_malloc(sizeof(usb_driver_t));

    if(!driver) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for driver");
        return -1;
    }

    driver->usb_device = device;
    driver->interface  = interface;
    driver->pipeline_callback = NULL;
    driver->driver_type = USB_AUDIO_DRIVER_TYPE_CONTROL;

    interface->driver = driver;

    if(!usb_audio_beep) {
        usb_audio_beep = usb_audio_generate_beep(440.0, 1000, 100000, 48000, &usb_audio_beep_samples);

        if(!usb_audio_beep) {
            PRINTLOG(USB, LOG_ERROR, "cannot generate beep sound");
            memory_free(driver);
            interface->driver = NULL;
            return -1;
        }
    }

    usb_transfer_t ut = {0};

    ut.driver = driver;
    ut.endpoint = interface->endpoints[0];
    ut.length = usb_audio_beep_samples * 2 * sizeof(int16_t);
    ut.data = (uint8_t*)usb_audio_beep;
    ut.is_async = true;
    ut.is_isochronous = true;

    if(device->controller->data_transfer(device->controller, &ut) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot send beep data to audio device");
        memory_free(driver);
        interface->driver = NULL;
        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "audio streaming device initialized");

    return 0;
}
