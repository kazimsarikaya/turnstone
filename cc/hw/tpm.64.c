/**
 * @file tpm.64.c
 * @brief TPM (Trusted Platform Module) related functions and definitions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/tpm.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <logging.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <utils.h>

MODULE("turnstone.kernel.hw.tpm");

typedef enum tpm_registers_t {
    TPM_REG_ACCESS    = 0x00,
    TPM_REG_STS       = 0x18,
    TPM_REG_DATA_FIFO = 0x24,
} tpm_registers_t;

// STS Bits
typedef enum tpm_status_bits_t {
    TPM_STS_EXPECT     = 0x08, // Bit 3: TPM expects more data
    TPM_STS_DATA_AVAIL = 0x10, // Bit 4: Data is ready to be read
    TPM_STS_GO         = 0x20, // Bit 5: Execute command
    TPM_STS_READY      = 0x40, // Bit 6: TPM is ready for a new command
    TPM_STS_VALID      = 0x80, // Bit 7: Status register is valid
} tpm_status_bits_t;

#define TPM_ADDR(reg) (tpm2_device->base_address + (reg))

struct tpm2_device_t {
    uint64_t base_address;
    uint64_t size;
};

typedef enum tpm2_command_tags_t : uint16_t {
    TPM_ST_NO_SESSIONS = 0x8001,
    TPM_ST_SESSIONS = 0x8002,
} tpm2_command_tags_t;

typedef enum tpm2_command_codes_t : uint32_t {
    TPM_CC_GetRandom = 0x17B,
} tpm2_command_codes_t;

typedef struct tpm2_command_header_t {
    tpm2_command_tags_t  tag;
    uint32_t             size;
    tpm2_command_codes_t code;
} __attribute__((packed)) tpm2_command_header_t;

_Static_assert(sizeof(tpm2_command_header_t) == 10, "tpm2_command_header_t must be 10 bytes");

tpm2_device_t* tpm2_device = NULL;

int8_t tpm2_init(void) {
    acpi_sdt_header_t* tpm2 = acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "TPM2");

    PRINTLOG(TPM, LOG_DEBUG, "tpm2 table %p", tpm2);

    if(!tpm2) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 table not found");
        return -2; // not found
    }

    const acpi_aml_device_t* tpm2_dev = acpi_device_lookup_by_name(ACPI_CONTEXT->acpi_parser_context, "TPM_");

    PRINTLOG(TPM, LOG_DEBUG, "tpm2 device %p", tpm2_dev);

    if(!tpm2_dev) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not found");
        return -2; // not found
    }

    if(!tpm2_dev->memory_ranges || list_size(tpm2_dev->memory_ranges) == 0) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device memory range not found");
        return -1; // error
    }

    if(list_size(tpm2_dev->memory_ranges) > 1) {
        PRINTLOG(TPM, LOG_WARNING, "tpm2 device has more than 1 memory range. Only first one will be used");
    }

    uint64_t tpm2_base = ((acpi_aml_device_memory_range_t*)list_get_data_at_position(tpm2_dev->memory_ranges, 0))->min;
    uint64_t tpm2_size = ((acpi_aml_device_memory_range_t*)list_get_data_at_position(tpm2_dev->memory_ranges, 0))->max - tpm2_base + 1;

    PRINTLOG(TPM, LOG_DEBUG, "tpm2 device memory range base 0x%llx size 0x%llx", tpm2_base, tpm2_size);

    frame_t* tpm2_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)tpm2_base);

    uint64_t tpm2_frm_cnt = (tpm2_size + FRAME_SIZE - 1) / FRAME_SIZE;
    frame_t tpm2_req_frm  = {tpm2_base, tpm2_frm_cnt, FRAME_TYPE_RESERVED, 0};

    uint64_t tpm2_base_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tpm2_base);

    if(tpm2_frames == NULL) {
        if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &tpm2_req_frm) != 0) {
            PRINTLOG(TPM, LOG_ERROR, "cannot allocate frame");

            return -1;
        }
    }

    memory_paging_add_va_for_frame(tpm2_base_va, &tpm2_req_frm,
                                   MEMORY_PAGING_PAGE_TYPE_NOEXEC |
                                   MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE |
                                   MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH);

    tpm2_device = memory_malloc(sizeof(tpm2_device_t));

    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "cannot allocate memory for tpm2 device");
        return -1;
    }

    tpm2_device->base_address = tpm2_base_va;
    tpm2_device->size = tpm2_size;

    return 0;
}

static int8_t tpm2_wait_for_status_bit(uint32_t bit_mask, boolean_t set, int32_t timeout) {
    volatile uint32_t* sts = (uint32_t*)TPM_ADDR(TPM_REG_STS);

    boolean_t current_bit_state = (*sts & bit_mask) != 0;
    while (current_bit_state != set && --timeout) {
        current_bit_state = (*sts & bit_mask) != 0; // Re-read status
    }

    if(timeout == 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM status bit 0x%08x to be %s. current status 0x%08x",
                 bit_mask, set ? "set" : "clear", *sts);
    }

    return (timeout == 0) ? -1 : 0;
}

static int8_t tpm2_set_locality(uint8_t locality) {
    volatile uint8_t* access = (uint8_t*)TPM_ADDR(TPM_REG_ACCESS);

    // Request locality
    *access = 1 << locality;

    int32_t timeout = 100000;
    while (((*access & (1 << (4 + locality))) == 0) && --timeout) {;}

    return (timeout == 0) ? -1 : 0;
}

static int8_t tpm2_write_fifo(const uint8_t* data, size_t size) {
    volatile uint8_t* fifo = (uint8_t*)TPM_ADDR(TPM_REG_DATA_FIFO);

    const uint32_t* data_u4 = (const uint32_t*)data;
    size_t u4_count = size / sizeof(uint32_t);

    for (size_t i = 0; i < u4_count; i++) {
        *(volatile uint32_t*)fifo = *data_u4;
        data_u4++;
    }

    size_t remaining_bytes = size % 4;
    data += u4_count * sizeof(uint32_t);

    for (size_t i = 0; i < remaining_bytes; i++) {
        *fifo = data[i];
    }

    return 0;
}

static int8_t tpm2_read_fifo(uint8_t* buffer, size_t size) {
    volatile uint8_t* fifo = (uint8_t*)TPM_ADDR(TPM_REG_DATA_FIFO);
    volatile uint32_t* fifo_u4 = (volatile uint32_t*)fifo;

    uint32_t* buffer_u4 = (uint32_t*)buffer;
    size_t u4_count = size / sizeof(uint32_t);

    for (size_t i = 0; i < u4_count; i++) {
        if (tpm2_wait_for_status_bit(TPM_STS_DATA_AVAIL, true, 100000) != 0) {
            PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM data to be available");
            return -1;
        }
        *buffer_u4 = *fifo_u4;
        buffer_u4++;
    }

    size_t remaining_bytes = size % 4;
    buffer += u4_count * sizeof(uint32_t);

    for (size_t i = 0; i < remaining_bytes; i++) {
        if (tpm2_wait_for_status_bit(TPM_STS_DATA_AVAIL, true, 100000) != 0) {
            PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM data to be available");
            return -1;
        }
        buffer[i] = *fifo;
    }

    return 0;
}

static int8_t tpm2_send_command(uint16_t tag, uint32_t cc_code, const uint8_t* cmd, size_t cmd_size) {
    volatile uint32_t* sts = (uint32_t*)TPM_ADDR(TPM_REG_STS);

    // Ensure TPM is ready
    if (tpm2_wait_for_status_bit(TPM_STS_READY, true, 100000) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM to be ready");
        return -1;
    }

    tpm2_command_header_t cmd_header = {
        .tag  = BYTE_SWAP16(tag),
        .size = BYTE_SWAP32(sizeof(tpm2_command_header_t) + cmd_size),
        .code = BYTE_SWAP32(cc_code),
    };

    if (tpm2_write_fifo((uint8_t*)&cmd_header, sizeof(cmd_header)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to write TPM command header");
        return -1;
    }

    if (tpm2_write_fifo(cmd, cmd_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to write TPM command data");
        return -1;
    }

    if (tpm2_wait_for_status_bit(TPM_STS_EXPECT, false, 100000) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM to be ready for more data");
        return -1;
    }

    // Execute command
    *sts = TPM_STS_GO;

    return 0;
}

static int8_t tpm2_read_response(size_t expected_size, uint8_t** buffer, size_t* buffer_size) {
    if(tpm2_device == NULL) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(expected_size && (!buffer || !buffer_size)) {
        PRINTLOG(TPM, LOG_ERROR, "invalid buffer or buffer size provided");
        return -1; // invalid arguments
    }

    // Wait for the TPM to be ready and data to be available
    if (tpm2_wait_for_status_bit(TPM_STS_DATA_AVAIL, true, 100000) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM response data to be available");
        return -1;
    }

    tpm2_command_header_t header;

    if (tpm2_read_fifo((uint8_t*)&header, sizeof(header)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM response header");
        return -1;
    }

    header.tag  = BYTE_SWAP16(header.tag);
    header.size = BYTE_SWAP32(header.size);
    header.code = BYTE_SWAP32(header.code);

    if(header.tag != TPM_ST_NO_SESSIONS) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected TPM response tag 0x%04x", header.tag);
        return -1;
    }

    if(header.code != 0) {
        PRINTLOG(TPM, LOG_ERROR, "TPM command failed with code 0x%08x", header.code);
        return -1;
    }

    size_t resp_size = header.size - sizeof(header);

    // when not expected_size -1ULL, resp_size should be same as expected_size
    if(expected_size && expected_size != -1ULL && resp_size != expected_size) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected TPM response size. Expected %llu, got %llu", expected_size, resp_size);
        return -1; // unexpected response size
    }

    if(expected_size == 0) {
        PRINTLOG(TPM, LOG_DEBUG, "TPM response size is 0, no data to read");
        *buffer = NULL;
        *buffer_size = 0;
        return 0;
    }

    *buffer = memory_malloc(resp_size);
    if (!*buffer) {
        PRINTLOG(TPM, LOG_ERROR, "failed to allocate memory for TPM response");
        return -1; // allocation failed
    }
    *buffer_size = resp_size;

    if (tpm2_read_fifo(*buffer, header.size - sizeof(header)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM response data");
        memory_free(*buffer);
        return -1;
    }

    return 0;
}

int8_t tpm2_get_random(uint8_t* buffer, uint16_t requested_sz) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(requested_sz > 0xFF) {
        PRINTLOG(TPM, LOG_ERROR, "requested size exceeds maximum allowed (255 bytes)");
        return -1; // TPM command size limit
    }

    if(tpm2_set_locality(1) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    uint16_t cmd_data = BYTE_SWAP16(requested_sz);

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM_CC_GetRandom, (uint8_t*)&cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM GetRandom command");
        return -1; // failed to send command
    }

    size_t expected_size = sizeof(uint16_t) + requested_sz;
    uint8_t* response_data = NULL;
    size_t response_size = 0;

    if (tpm2_read_response(expected_size, &response_data, &response_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM GetRandom response");
        return -1;
    }

    // first 2 bytes of response data contain the actual size of random data returned by TPM
    response_size = BYTE_SWAP16(*(uint16_t*)response_data);

    if (response_size > requested_sz) {
        PRINTLOG(TPM, LOG_ERROR, "TPM returned more data than requested");
        memory_free(response_data);
        return -1;
    }

    memory_memcopy(response_data + sizeof(uint16_t), buffer, response_size);

    memory_free(response_data);

    return 0;
}
