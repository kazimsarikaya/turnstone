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

typedef enum tpm2_capabilities_t : uint32_t {
    TPM_CAP_ALGS            = 0x00,
    TPM_CAP_HANDLES         = 0x01,
    TPM_CAP_COMMANDS        = 0x02,
    TPM_CAP_PP_COMMANDS     = 0x03,
    TPM_CAP_AUDIT_COMMANDS  = 0x04,
    TPM_CAP_PCRS            = 0x05,
    TPM_CAP_TPM_PROPERTIES  = 0x06,
    TPM_CAP_ECC_CURVES      = 0x08,
    TPM_CAP_VENDOR_PROPERTY = 0x09,
} tpm2_capabilities_t;

#define TPM2_HANDLE_PCRS 0x40000000
#define TPM2_HANDLE_TRANSIENT 0x80000000
#define TPM2_HANDLE_PERMANENT 0x81000000

#define TPM2_NONCE_SIZE 0x20

typedef enum tpm2_handes_t : uint32_t {
    TPM_RH_OWNER       = 0x40000001,
    TPM_RH_NULL        = 0x40000007,
    TPM_RS_PW          = 0x40000009,
    TPM_RH_LOCKOUT     = 0x4000000A,
    TPM_RH_ENDORSEMENT = 0x4000000B,
    TPM_RH_PLATFORM    = 0x4000000C,
} tpm2_handes_t;

typedef enum tpm2_startup_types : uint16_t {
    TPM2_SU_CLEAR = 0x0000,
    TPM2_SU_STATE = 0x0001,
} tpm2_startup_types;

typedef enum tpm2_command_tags_t : uint16_t {
    TPM_ST_NO_SESSIONS = 0x8001,
    TPM_ST_SESSIONS    = 0x8002,
    TPM_ST_HASHCHECK   = 0x8024,
} tpm2_command_tags_t;

typedef enum tpm2_command_code_t : uint32_t {
    TPM2_CC_CLEAR              = 0x0126,
    TPM2_CC_CLEARCONTROL       = 0x0127,
    TPM2_CC_HIERCHANGEAUTH     = 0x0129,
    TPM2_CC_PCR_SETAUTHPOL     = 0x012C,
    TPM2_CC_DAM_RESET          = 0x0139,
    TPM2_CC_DAM_PARAMETERS     = 0x013A,
    TPM2_CC_SELF_TEST          = 0x0143,
    TPM2_CC_STARTUP            = 0x0144,
    TPM2_CC_SHUTDOWN           = 0x0145,
    TPM2_CC_NV_READ            = 0x014E,
    TPM2_CC_SIGN               = 0x15D,
    TPM2_CC_READ_PUBLIC        = 0x173,
    TPM2_CC_START_AUTH_SESSION = 0x176,
    TPM2_CC_GET_CAPABILITIES   = 0x17A,
    TPM2_CC_GET_RANDOM         = 0x17B,
    TPM2_CC_PCR_READ           = 0x017E,
    TPM2_CC_PCR_EXTEND         = 0x0182,
    TPM2_CC_PCR_SETAUTHVAL     = 0x0183,
} tpm2_command_code_t;

typedef enum tpm2_return_code_t : uint32_t {
    TPM2_RC_SUCCESS      = 0x0000,
    TPM2_RC_BAD_TAG      = 0x001E,
    TPM2_RC_FMT1         = 0x0080,
    TPM2_RC_HASH         = TPM2_RC_FMT1 + 0x0003,
    TPM2_RC_VALUE        = TPM2_RC_FMT1 + 0x0004,
    TPM2_RC_SIZE         = TPM2_RC_FMT1 + 0x0015,
    TPM2_RC_BAD_AUTH     = TPM2_RC_FMT1 + 0x0022,
    TPM2_RC_HANDLE       = TPM2_RC_FMT1 + 0x000B,
    TPM2_RC_VER1         = 0x0100,
    TPM2_RC_INITIALIZE   = TPM2_RC_VER1 + 0x0000,
    TPM2_RC_FAILURE      = TPM2_RC_VER1 + 0x0001,
    TPM2_RC_DISABLED     = TPM2_RC_VER1 + 0x0020,
    TPM2_RC_AUTH_MISSING = TPM2_RC_VER1 + 0x0025,
    TPM2_RC_COMMAND_CODE = TPM2_RC_VER1 + 0x0043,
    TPM2_RC_AUTHSIZE     = TPM2_RC_VER1 + 0x0044,
    TPM2_RC_AUTH_CONTEXT = TPM2_RC_VER1 + 0x0045,
    TPM2_RC_NEEDS_TEST   = TPM2_RC_VER1 + 0x0053,
    TPM2_RC_WARN         = 0x0900,
    TPM2_RC_TESTING      = TPM2_RC_WARN + 0x000A,
    TPM2_RC_REFERENCE_H0 = TPM2_RC_WARN + 0x0010,
    TPM2_RC_LOCKOUT      = TPM2_RC_WARN + 0x0021,
} tpm2_return_code_t;

typedef struct tpm2_command_header_t {
    tpm2_command_tags_t tag;
    uint32_t            size;
    tpm2_command_code_t code;
} __attribute__((packed)) tpm2_command_header_t;

_Static_assert(sizeof(tpm2_command_header_t) == 10, "tpm2_command_header_t must be 10 bytes");

typedef struct tpm2_response_header_t {
    tpm2_command_tags_t tag;
    uint32_t            size;
    tpm2_return_code_t  code;
} __attribute__((packed)) tpm2_response_header_t;

_Static_assert(sizeof(tpm2_response_header_t) == 10, "tpm2_response_header_t must be 10 bytes");

tpm2_device_t* tpm2_device = NULL;

static int8_t tpm2_wait_for_status_bit(uint32_t bit_mask, boolean_t set, int32_t timeout) {
    volatile uint32_t* sts = (uint32_t*)TPM_ADDR(TPM_REG_STS);

    if(set) {
        *sts |= bit_mask; // Set the bit
    }

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

static int8_t tpm2_set_locality(boolean_t locality) {
    volatile uint8_t* access = (uint8_t*)TPM_ADDR(TPM_REG_ACCESS);

    // Request locality
    uint8_t request_bit = 1 << (locality ? 1 : 0);
    *access = request_bit;

    int32_t timeout = 100000;
    while (((*access & (1 << 5)) == 0) && --timeout) {;}

    if(timeout == 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while requesting TPM locality %s. access register value 0x%02x", locality ? "true" : "false", *access);
    }

    return (timeout == 0) ? -1 : 0;
}

static int8_t tpm2_write_fifo(const uint8_t* data, size_t size) {
    volatile uint8_t* fifo = (uint8_t*)TPM_ADDR(TPM_REG_DATA_FIFO);

    const uint32_t* data_u4 = (const uint32_t*)(void*)data;
    size_t u4_count         = size / sizeof(uint32_t);

    for (size_t i = 0; i < u4_count; i++) {
        *(volatile uint32_t*)(void*)fifo = *data_u4;
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
    volatile uint8_t* fifo     = (uint8_t*)TPM_ADDR(TPM_REG_DATA_FIFO);
    volatile uint32_t* fifo_u4 = (volatile uint32_t*)(void*)fifo;

    uint32_t* buffer_u4 = (uint32_t*)(void*)buffer;
    size_t u4_count     = size / sizeof(uint32_t);

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

typedef struct tpm2_send_command_args_t {
    uint16_t       tag;
    uint32_t       cc_code;
    const uint8_t* cmd;
    size_t         cmd_size;
} tpm2_send_command_args_t;

#define tpm2_send_command(a_tag, a_cc_code, ...) ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
        int8_t __rc = tpm2_send_command_internal((tpm2_send_command_args_t){ \
        .cmd=NULL, .cmd_size=0, \
        .tag=(a_tag), .cc_code=(a_cc_code) \
        ,##__VA_ARGS__}); \
        _Pragma("GCC diagnostic pop") \
        __rc; \
        })

static int8_t tpm2_send_command_internal(tpm2_send_command_args_t args) {
    volatile uint32_t* sts = (uint32_t*)TPM_ADDR(TPM_REG_STS);

    // Ensure TPM is ready
    if (tpm2_wait_for_status_bit(TPM_STS_READY, true, 100000) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM to be ready");
        return -1;
    }

    tpm2_command_header_t cmd_header = {
        .tag  = BYTE_SWAP16(args.tag),
        .size = BYTE_SWAP32(sizeof(tpm2_command_header_t) + args.cmd_size),
        .code = BYTE_SWAP32(args.cc_code),
    };

    if (tpm2_write_fifo((uint8_t*) &cmd_header, sizeof(cmd_header)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to write TPM command header");
        return -1;
    }

    if (args.cmd_size && tpm2_write_fifo(args.cmd, args.cmd_size) != 0) {
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

typedef struct tpm2_read_response_args_t {
    size_t              expected_size;
    tpm2_command_tags_t tag;
    uint8_t**           buffer;
    size_t*             buffer_size;
    tpm2_return_code_t* return_code;
} tpm2_read_response_args_t;

#define tpm2_read_response(a_expected_size, a_tag, ...) ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
        int8_t __rc = tpm2_read_response_internal((tpm2_read_response_args_t){ \
        .expected_size=0, .buffer=NULL, .buffer_size=NULL, .return_code=NULL, \
        .expected_size=(a_expected_size), .tag=(a_tag) \
        ,##__VA_ARGS__}); \
        _Pragma("GCC diagnostic pop") \
        __rc; \
        })

static int8_t tpm2_read_response_internal(tpm2_read_response_args_t args) {
    if(tpm2_device == NULL) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(args.expected_size && (!args.buffer || !args.buffer_size)) {
        PRINTLOG(TPM, LOG_ERROR, "invalid buffer or buffer size provided");
        return -1; // invalid arguments
    }

    // Wait for the TPM to be ready and data to be available
    if (tpm2_wait_for_status_bit(TPM_STS_DATA_AVAIL, true, 100000) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "timeout while waiting for TPM response data to be available");
        return -1;
    }

    tpm2_response_header_t header;

    if (tpm2_read_fifo((uint8_t*) &header, sizeof(header)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM response header");
        return -1;
    }

    header.tag  = BYTE_SWAP16(header.tag);
    header.size = BYTE_SWAP32(header.size);
    header.code = BYTE_SWAP32(header.code);

    if(header.tag != args.tag) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected TPM response tag 0x%04x", header.tag);
        return -1;
    }

    if(header.code != TPM2_RC_SUCCESS) {
        if(args.return_code) {
            if(header.code != *args.return_code) {
                PRINTLOG(TPM, LOG_ERROR, "unexpected TPM response code. Expected 0x%08x, got 0x%08x", *args.return_code, header.code);
                return -1; // unexpected return code
            }
            *args.return_code = header.code;
        } else {
            PRINTLOG(TPM, LOG_ERROR, "TPM command failed with code 0x%08x", header.code);
            return -1;
        }
    }

    size_t resp_size = header.size - sizeof(header);

    // when not expected_size -1ULL, resp_size should be same as expected_size
    if(args.expected_size && args.expected_size != -1ULL && resp_size != args.expected_size) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected TPM response size. Expected %llu, got %llu", args.expected_size, resp_size);
        return -1; // unexpected response size
    }

    if(args.expected_size == 0) {
        PRINTLOG(TPM, LOG_DEBUG, "TPM response size is 0, no data to read");
        args.buffer      = NULL;
        args.buffer_size = 0;
        return 0;
    }

    *args.buffer = memory_malloc(resp_size);
    if (!*args.buffer) {
        PRINTLOG(TPM, LOG_ERROR, "failed to allocate memory for TPM response");
        return -1; // allocation failed
    }
    *args.buffer_size = resp_size;

    if (tpm2_read_fifo(*args.buffer, header.size - sizeof(header)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM response data");
        memory_free(*args.buffer);
        return -1;
    }

    return 0;
}

int8_t tpm2_get_random (uint8_t * buffer, uint16_t requested_sz) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(requested_sz > 0xFF) {
        PRINTLOG(TPM, LOG_ERROR, "requested size exceeds maximum allowed (255 bytes)");
        return -1; // TPM command size limit
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    uint16_t cmd_data = BYTE_SWAP16(requested_sz);

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM2_CC_GET_RANDOM, (uint8_t*)&cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM GetRandom command");
        return -1; // failed to send command
    }

    size_t expected_size   = sizeof(uint16_t) + requested_sz;
    uint8_t* response_data = NULL;
    size_t response_size   = 0;

    if (tpm2_read_response(expected_size, TPM_ST_NO_SESSIONS, &response_data, &response_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM GetRandom response");
        return -1;
    }

    // first 2 bytes of response data contain the actual size of random data returned by TPM
    response_size = BYTE_SWAP16(*(uint16_t*)(void*) response_data);

    if (response_size > requested_sz) {
        PRINTLOG(TPM, LOG_ERROR, "TPM returned more data than requested");
        memory_free(response_data);
        return -1;
    }

    memory_memcopy(response_data + sizeof(uint16_t), buffer, response_size);

    memory_free(response_data);

    return 0;
}

static int8_t tpm2_get_permanent_handles (uint32_t ** handles, size_t* handle_count) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(!handles || !handle_count) {
        PRINTLOG(TPM, LOG_ERROR, "invalid handles or handle_count provided");
        return -1; // invalid arguments
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    // Command parameters for TPM_CC_GET_CAPABILITIES
    // capability: TPM_CAP_HANDLES
    // property: TPM2_HANDLE_PERMANENT
    // propertyCount: 8 (max number of handles to return in one go)
    typedef struct tpm2_get_capabilities_cmd_t {
        tpm2_capabilities_t capability;
        uint32_t            property;
        uint32_t            property_count;
    } __attribute__((packed)) tpm2_get_capabilities_cmd_t;

    tpm2_get_capabilities_cmd_t cmd_data = {
        .capability     = BYTE_SWAP32(TPM_CAP_HANDLES),
        .property       = BYTE_SWAP32(TPM2_HANDLE_PERMANENT),
        .property_count = BYTE_SWAP32(8), // Request 8 handles at a time
    };

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM2_CC_GET_CAPABILITIES, (uint8_t*)&cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM GetCapabilities command for persistent handles");
        return -1; // failed to send command
    }

    // Response structure for TPM_CC_GET_CAPABILITIES
    typedef struct tpm2_get_capabilities_resp_t {
        uint8_t             more_data;
        tpm2_capabilities_t capability;
        uint32_t            handle_count; // Number of handles returned in this response
        // Followed by data (list of handles)
    } __attribute__((packed)) tpm2_get_capabilities_resp_t;

    uint8_t* response_data = NULL;
    size_t response_size   = 0;

    // We don't know the exact size of the data part, so we pass -1ULL
    if (tpm2_read_response(-1ULL, TPM_ST_NO_SESSIONS, &response_data, &response_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM GetCapabilities response for persistent handles");
        return -1;
    }

    if (response_size < sizeof(tpm2_get_capabilities_resp_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM GetCapabilities response too small");
        memory_free(response_data);
        return -1;
    }

    tpm2_get_capabilities_resp_t* resp = (tpm2_get_capabilities_resp_t*) response_data;

    resp->capability   = BYTE_SWAP32(resp->capability);
    resp->handle_count = BYTE_SWAP32(resp->handle_count);

    if (resp->capability != TPM_CAP_HANDLES) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected capability in TPM GetCapabilities response: 0x%08x", resp->capability);
        memory_free(response_data);
        return -1;
    }

    if (resp->handle_count == 0) {
        PRINTLOG(TPM, LOG_DEBUG, "no persistent handles found in TPM");
        memory_free(response_data);
        return 0;
    }

    *handle_count = resp->handle_count;
    *handles      = memory_malloc(*handle_count * sizeof(uint32_t));

    if (!*handles) {
        PRINTLOG(TPM, LOG_ERROR, "failed to allocate memory for persistent handles");
        memory_free(response_data);
        return -1;
    }

    uint32_t* raw_handles = (uint32_t*)(void*)(response_data + sizeof(tpm2_get_capabilities_resp_t));
    for (size_t i = 0; i < *handle_count; i++) {
        (*handles)[i] = BYTE_SWAP32(raw_handles[i]);
    }

    // If more_data is true, we would need to loop and fetch more handles.
    // For simplicity, we assume all handles are returned in one go for now.
    if (resp->more_data) {
        PRINTLOG(TPM, LOG_WARNING, "TPM has more persistent handles than returned in one request. Not all handles fetched.");
    }

    memory_free(response_data);

    return 0;
}

static int8_t tpm2_read_public (uint32_t handle, uint8_t** data, size_t* data_len) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(!data || !data_len) {
        PRINTLOG(TPM, LOG_ERROR, "invalid data or data_len provided");
        return -1; // invalid arguments
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    PRINTLOG(TPM, LOG_DEBUG, "reading public area for handle 0x%08x", handle);

    // Command parameters for TPM_CC_READ_PUBLIC
    // handle: The handle of the object to be read
    typedef struct tpm2_read_public_cmd_t {
        uint32_t handle;
    } __attribute__((packed)) tpm2_read_public_cmd_t;

    tpm2_read_public_cmd_t cmd_data = {
        .handle = BYTE_SWAP32(handle),
    };

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM2_CC_READ_PUBLIC, (uint8_t*) &cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM ReadPublic command for handle 0x%08x", handle);
        return -1; // failed to send command
    }

    // Response structure for TPM_CC_READ_PUBLIC
    // The response contains a TPM2B_PUBLIC structure, followed by a TPM2B_NAME and a TPM2B_ATTEST.
    // We are interested in the TPM2B_PUBLIC part for now.
    // The exact size is not known beforehand, so we read the whole response.
    uint8_t* response_data = NULL;
    size_t response_size   = 0;

    if (tpm2_read_response(-1ULL, TPM_ST_NO_SESSIONS, &response_data, &response_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM ReadPublic response for handle 0x%08x", handle);
        return -1;
    }

    // The response starts with a TPM2B_PUBLIC structure.
    // The first two bytes of TPM2B_PUBLIC indicate the size of the public area.
    if (response_size < sizeof(uint16_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM ReadPublic response too small to contain public area size");
        memory_free(response_data);
        return -1;
    }

    uint16_t public_area_size = BYTE_SWAP16(*(uint16_t*)(void*) response_data);

    if (response_size < sizeof(uint16_t) + public_area_size) {
        PRINTLOG(TPM, LOG_ERROR, "TPM ReadPublic response size mismatch with public area size. Expected at least %lu, got %llu",
                 sizeof(uint16_t) + public_area_size, response_size);
        memory_free(response_data);
        return -1;
    }

    // Allocate memory for the public data (TPM2B_PUBLIC)
    *data = memory_malloc(public_area_size);
    if (!*data) {
        PRINTLOG(TPM, LOG_ERROR, "failed to allocate memory for public data");
        memory_free(response_data);
        return -1;
    }
    *data_len = public_area_size;

    // Copy the TPM2B_PUBLIC structure (publicArea)
    memory_memcopy(response_data + sizeof(uint16_t), *data, *data_len);

    memory_free(response_data);

    return 0;
}

static boolean_t tpm2_public_is_ecc_with_point (const uint8_t * public, size_t public_len,
                                                tpm_ecc_curve_t expected_curve_id, const uint8_t* point) {
    typedef struct tpm_public_header_t {
        tpm_alg_t type;
        uint16_t  nameAlg;
        uint32_t  objectAttributes;
        uint16_t  authPolicySize;
        uint8_t   authPolicy[]; // Flexible array member for authPolicy data
    } __attribute__((packed)) tpm_public_header_t;

    if (public_len < sizeof(tpm_public_header_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM public area data too small to contain header");
        return false;
    }

    tpm_public_header_t* header = (tpm_public_header_t*) public;

    header->type             = BYTE_SWAP16(header->type);
    header->nameAlg          = BYTE_SWAP16(header->nameAlg);
    header->objectAttributes = BYTE_SWAP32(header->objectAttributes);
    header->authPolicySize   = BYTE_SWAP16(header->authPolicySize);


    // Check if the type is TPM_ALG_ECC
    if (header->type != TPM_ALG_ECC) { // TPM_ALG_ECC
        PRINTLOG(TPM, LOG_WARNING, "TPM public area is not of type ECC. Found type 0x%04x", header->type);
        return false;
    }

    typedef struct tpm_simetric_params_t {
        tpm_alg_t algorithm; // TPM_ALG_ID for symmetric algorithm (e.g., TPM_ALG_AES)
        uint16_t  key_bits; // Key size in bits (e.g., 128, 192, 256 for AES)
        uint16_t  mode; // Mode of operation (e.g., TPM_ALG_CFB for AES)
    } __attribute__((packed)) tpm_symmetric_params_t;

    if (public_len < sizeof(tpm_public_header_t) + header->authPolicySize + sizeof(tpm_alg_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM public area data too small to contain symmetric parameters");
        return false;
    }

    tpm_symmetric_params_t* sym_params = (tpm_symmetric_params_t*)(public + sizeof(tpm_public_header_t) + header->authPolicySize);

    sym_params->algorithm = BYTE_SWAP16(sym_params->algorithm);

    size_t sym_params_size = 0;
    if(sym_params->algorithm != TPM_ALG_NULL) {
        sym_params->key_bits = BYTE_SWAP16(sym_params->key_bits);
        sym_params->mode     = BYTE_SWAP16(sym_params->mode);
        sym_params_size      = sizeof(tpm_symmetric_params_t);
    } else {
        sym_params_size = sizeof(tpm_alg_t); // Only the algorithm field is present
    }

    typedef struct tpm_ecc_params_t {
        tpm_alg_t       ecc_scheme; // TPM_ALG_ID for ECC scheme (e.g., TPM_ALG_NULL, TPM_ALG_ECDSA)
        tpm_ecc_curve_t curve_id; // TPM_ECC_CURVE
        tpm_alg_t       kdf_scheme; // TPM_ALG_ID for KDF scheme
    } __attribute__((packed)) tpm_ecc_params_t;

    if (public_len < sizeof(tpm_public_header_t) +
        header->authPolicySize +
        sym_params_size +
        sizeof(tpm_ecc_params_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM public area data too small to contain ECC parameters");
        return false;
    }

    tpm_ecc_params_t* ecc_params = (tpm_ecc_params_t*)(public + sizeof(tpm_public_header_t) + header->authPolicySize + sym_params_size);

    ecc_params->ecc_scheme = BYTE_SWAP16(ecc_params->ecc_scheme);
    ecc_params->curve_id   = BYTE_SWAP16(ecc_params->curve_id);
    ecc_params->kdf_scheme = BYTE_SWAP16(ecc_params->kdf_scheme);

    if (ecc_params->curve_id != expected_curve_id) {
        PRINTLOG(TPM, LOG_WARNING, "TPM ECC public key has unexpected curve ID. Expected 0x%04x, found 0x%04x", expected_curve_id, ecc_params->curve_id);
        return false;
    }

    typedef struct tpm_ecc_point_t {
        uint16_t point_size;
        uint8_t  point[]; // Flexible array member for x coordinate
    } __attribute__((packed)) tpm_ecc_point_t;

    if (public_len < sizeof(tpm_public_header_t) +
        header->authPolicySize +
        sym_params_size +
        sizeof(tpm_ecc_params_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM public area data too small to contain ECC point size");
        return false;
    }

    tpm_ecc_point_t* ecc_point_x = (tpm_ecc_point_t*)(public +
                                                      sizeof(tpm_public_header_t) +
                                                      header->authPolicySize +
                                                      sym_params_size +
                                                      sizeof(tpm_ecc_params_t));

    ecc_point_x->point_size = BYTE_SWAP16(ecc_point_x->point_size);

    if (public_len < sizeof(tpm_public_header_t) +
        header->authPolicySize +
        sym_params_size +
        sizeof(tpm_ecc_params_t) +
        sizeof(tpm_ecc_point_t) + // x_size field
        ecc_point_x->point_size) {
        PRINTLOG(TPM, LOG_ERROR, "TPM public area data too small to contain ECC point x coordinate");
        return false;
    }

    tpm_ecc_point_t* ecc_point_y = (tpm_ecc_point_t*)(public +
                                                      sizeof(tpm_public_header_t) +
                                                      header->authPolicySize +
                                                      sym_params_size +
                                                      sizeof(tpm_ecc_params_t) +
                                                      sizeof(tpm_ecc_point_t) + ecc_point_x->point_size);

    ecc_point_y->point_size = BYTE_SWAP16(ecc_point_y->point_size);

    if (public_len < sizeof(tpm_public_header_t) +
        header->authPolicySize +
        sym_params_size +
        sizeof(tpm_ecc_params_t) +
        sizeof(tpm_ecc_point_t) + ecc_point_x->point_size
        + sizeof(tpm_ecc_point_t) + ecc_point_y->point_size) {
        PRINTLOG(TPM, LOG_ERROR, "TPM public area data too small to contain ECC point y coordinate");
        return false;
    }


    if(memory_memcompare(ecc_point_x->point, point, ecc_point_x->point_size) != 0 ||
       memory_memcompare(ecc_point_y->point, point + ecc_point_x->point_size, ecc_point_y->point_size) != 0) {
        PRINTLOG(TPM, LOG_WARNING, "TPM ECC public key x coordinate does not match expected value");
        return false;
    }

    return true;
}

int8_t tpm2_find_ecc_key_with_point (tpm_ecc_curve_t curve_id, const uint8_t* point, uint32_t* handle) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(!point || !handle) {
        PRINTLOG(TPM, LOG_ERROR, "invalid point_x or handle provided");
        return -1; // invalid arguments
    }

    uint32_t* handles   = NULL;
    size_t handle_count = 0;

    if(tpm2_get_permanent_handles(&handles, &handle_count) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to get permanent handles from TPM");
        return -1;
    }

    if(handle_count == 0) {
        PRINTLOG(TPM, LOG_DEBUG, "no permanent handles found in TPM");
        memory_free(handles);
        return -1;
    }

    PRINTLOG(TPM, LOG_DEBUG, "checking %llu permanent handles for ECC key with curve ID 0x%04x and matching x coordinate", handle_count, curve_id);

    int8_t ret = -1;
    for (size_t i = 0; i < handle_count; i++) {
        uint8_t* public_data   = NULL;
        size_t public_data_len = 0;

        if(tpm2_read_public(handles[i], &public_data, &public_data_len) != 0) {
            PRINTLOG(TPM, LOG_WARNING, "failed to read public data for handle 0x%08x. Skipping.", handles[i]);
            continue;
        }

        if(tpm2_public_is_ecc_with_point(public_data, public_data_len, curve_id, point)) {
            *handle = handles[i];
            ret     = 0;
            memory_free(public_data);
            break;
        }

        memory_free(public_data);
    }

    memory_free(handles);
    return ret;
}

int8_t tpm2_start_auth_session (uint32_t * session_handle) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(!session_handle) {
        PRINTLOG(TPM, LOG_ERROR, "invalid session_handle provided");
        return -1; // invalid arguments
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    // Command parameters for TPM_CC_START_AUTH_SESSION
    typedef struct tpm2_start_auth_session_cmd_t {
        uint32_t  tpmKey;
        uint32_t  bind;
        uint16_t  nonceSize;
        uint8_t   nonce[TPM2_NONCE_SIZE]; // Max nonce size is 32 bytes for TPM2.0
        uint16_t  encryptedSaltSize;
        uint8_t   sessionType;
        tpm_alg_t symmetric;
        tpm_alg_t authHash;
    } __attribute__((packed)) tpm2_start_auth_session_cmd_t;

    tpm2_start_auth_session_cmd_t cmd_data = {
        .tpmKey      = BYTE_SWAP32(TPM_RH_NULL), // No bound object
        .bind        = BYTE_SWAP32(TPM_RH_NULL), // No bind object
        .nonceSize   = BYTE_SWAP16(sizeof(cmd_data.nonce)), // No nonce
        .sessionType = BYTE_SWAP16(0), // HMAC session
        .symmetric   = BYTE_SWAP16(TPM_ALG_NULL), // No symmetric algorithm
        .authHash    = BYTE_SWAP16(TPM_ALG_SHA256), // Use SHA-256 for HMAC
    };

    if(tpm2_get_random(cmd_data.nonce, sizeof(cmd_data.nonce)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to get random nonce for TPM StartAuthSession command");
        return -1; // failed to get random data
    }

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM2_CC_START_AUTH_SESSION, (uint8_t*) &cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM StartAuthSession command");
        return -1; // failed to send command
    }

    // Response structure for TPM_CC_START_AUTH_SESSION
    typedef struct tpm2_start_auth_session_resp_t {
        uint32_t sessionHandle;
        uint16_t nonceSize;
        uint8_t  nonce[TPM2_NONCE_SIZE]; // Max nonce size is 32 bytes for TPM2.0
    } __attribute__((packed)) tpm2_start_auth_session_resp_t;

    uint8_t* response_data = NULL;
    size_t response_size   = 0;

    if (tpm2_read_response(sizeof(tpm2_start_auth_session_resp_t), TPM_ST_NO_SESSIONS, &response_data, &response_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM StartAuthSession response");
        return -1;
    }

    if (response_size != sizeof(tpm2_start_auth_session_resp_t)) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected TPM StartAuthSession response size. Expected %lu, got %llu", sizeof(tpm2_start_auth_session_resp_t), response_size);
        memory_free(response_data);
        return -1;
    }

    tpm2_start_auth_session_resp_t* resp = (tpm2_start_auth_session_resp_t*) response_data;
    resp->sessionHandle = BYTE_SWAP32(resp->sessionHandle);
    resp->nonceSize     = BYTE_SWAP16(resp->nonceSize);
    // We don't need the nonce for this use case, so we ignore it.memory_free(response_data);

    *session_handle = resp->sessionHandle;

    return 0;
}

int8_t tpm2_sign_ecc_with_hash (uint32_t handle, tpm_alg_t digest_alg,
                                const uint8_t* digest, uint8_t** sign, size_t* sign_len) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(!digest || !sign || !sign_len) {
        PRINTLOG(TPM, LOG_ERROR, "invalid digest, der_sig or der_sig_len provided");
        return -1; // invalid arguments
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    uint32_t session_handle;
    if (tpm2_start_auth_session(&session_handle) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to start auth session for TPM signing");
        return -1; // failed to start auth session
    }

    // Determine digest size based on algorithm
    size_t digest_size = 0;
    switch (digest_alg) {
        case TPM_ALG_SHA1:   digest_size = 20; break;
        case TPM_ALG_SHA256: digest_size = 32; break;
        case TPM_ALG_SHA384: digest_size = 48; break;
        case TPM_ALG_SHA512: digest_size = 64; break;
        default:
            PRINTLOG(TPM, LOG_ERROR, "unsupported digest algorithm 0x%04x for signing", digest_alg);
            return -1;
    }

    typedef struct tpm_sign_header_t {
        uint32_t handle;
    } __attribute__((packed)) tpm_sign_header_t;

    typedef struct tpm_auth_header_t {
        uint32_t auth_area_size;
        uint32_t session_handle;
    } __attribute__((packed)) tpm_auth_header_t;

    typedef struct tpm_auth_nonce_t {
        uint16_t size;
        uint8_t  data[]; // Flexible array member for nonce data
    } __attribute__((packed)) tpm_auth_nonce_t;

    typedef struct tpm_auth_session_attributes_t {
        uint8_t attributes;
    } __attribute__((packed)) tpm_auth_session_attributes_t;

    typedef struct tpm_auth_hmac_t {
        uint16_t hmac_size;
        uint8_t  hmac[]; // Flexible array member for HMAC data
    } __attribute__((packed)) tpm_auth_hmac_t;

    typedef struct tpm_digest_header_t {
        uint16_t digest_size;
        uint8_t  digest[]; // Flexible array member for digest data
    } __attribute__((packed)) tpm_digest_header_t;

    typedef struct tpm_digest_footer_t {
        tpm_alg_t scheme;
        tpm_alg_t hash_alg;
    } __attribute__((packed)) tpm_digest_footer_t;

    typedef struct tpm_tk_hashcheck_t {
        uint16_t ticket_tag;
        uint32_t hierarchy;
        uint16_t digest_size;
        uint8_t  digest[]; // Flexible array member for validation ticket digest
    } __attribute__((packed)) tpm_tk_hashcheck_t;

    uint32_t auth_area_size =  sizeof(tpm_auth_header_t) +
                              sizeof(tpm_auth_nonce_t) + 32 +
                              sizeof(tpm_auth_session_attributes_t) +
                              sizeof(tpm_auth_hmac_t) -
                              sizeof(uint32_t);

    size_t cmd_total_size = sizeof(tpm_sign_header_t) +
                            sizeof(tpm_auth_header_t) + auth_area_size - sizeof(uint32_t) +
                            sizeof(tpm_digest_header_t) + digest_size +  sizeof(tpm_digest_footer_t) +
                            sizeof(tpm_tk_hashcheck_t);
    uint8_t* cmd_buffer = memory_malloc(cmd_total_size);
    if (!cmd_buffer) {
        PRINTLOG(TPM, LOG_ERROR, "failed to allocate memory for TPM Sign command");
        return -1;
    }

    uint8_t* current_ptr = cmd_buffer;

    tpm_sign_header_t* sign_header = (tpm_sign_header_t*) current_ptr;

    sign_header->handle = BYTE_SWAP32(handle);

    current_ptr += sizeof(tpm_sign_header_t);

    tpm_auth_header_t* auth_header = (tpm_auth_header_t*) current_ptr;
    auth_header->auth_area_size = BYTE_SWAP32(auth_area_size);
    auth_header->session_handle = BYTE_SWAP32(session_handle);

    current_ptr += sizeof(tpm_auth_header_t);

    tpm_auth_nonce_t* auth_nonce = (tpm_auth_nonce_t*) current_ptr;
    auth_nonce->size = BYTE_SWAP16(TPM2_NONCE_SIZE); // 32 bytes nonce for TPM2.0

    if(tpm2_get_random(auth_nonce->data, TPM2_NONCE_SIZE) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to get random nonce for TPM Sign command");
        memory_free(cmd_buffer);
        return -1; // failed to get random data
    }

    current_ptr += sizeof(tpm_auth_nonce_t) + 32;

    tpm_auth_session_attributes_t* auth_attrs = (tpm_auth_session_attributes_t*) current_ptr;
    auth_attrs->attributes = 0x01; // ContinueSession attribute set, no encryption or audit

    current_ptr += sizeof(tpm_auth_session_attributes_t);

    tpm_auth_hmac_t* auth_hmac = (tpm_auth_hmac_t*) current_ptr;
    auth_hmac->hmac_size = BYTE_SWAP16(0); // No HMAC

    current_ptr += sizeof(tpm_auth_hmac_t);

    tpm_digest_header_t* digest_header = (tpm_digest_header_t*) current_ptr;

    digest_header->digest_size = BYTE_SWAP16(digest_size);

    memory_memcopy(digest, digest_header->digest, digest_size);

    current_ptr += sizeof(tpm_digest_header_t) + digest_size;

    tpm_digest_footer_t* digest_footer = (tpm_digest_footer_t*) current_ptr;
    digest_footer->scheme   = BYTE_SWAP16(TPM_ALG_ECDSA); // Using ECDSA scheme for signing
    digest_footer->hash_alg = BYTE_SWAP16(digest_alg);

    current_ptr += sizeof(tpm_digest_footer_t);

    tpm_tk_hashcheck_t* validation_ticket = (tpm_tk_hashcheck_t*) current_ptr;
    validation_ticket->ticket_tag = BYTE_SWAP16(TPM_ST_HASHCHECK);
    validation_ticket->hierarchy  = BYTE_SWAP32(TPM_RH_NULL); // TPM_RH_NULL

    if(tpm2_send_command(TPM_ST_SESSIONS, TPM2_CC_SIGN, cmd_buffer, cmd_total_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM Sign command for handle 0x%08x", handle);
        memory_free(cmd_buffer);
        return -1; // failed to send command
    }

    memory_free(cmd_buffer);

    uint8_t* response_data = NULL;
    size_t response_size   = 0;

    if (tpm2_read_response(-1ULL, TPM_ST_SESSIONS, &response_data, &response_size) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM Sign response for handle 0x%08x", handle);
        return -1;
    }

    typedef struct tpm_sign_t {
        uint32_t  sign_len;
        tpm_alg_t sign_alg;
        tpm_alg_t hash_alg;
    }__attribute__((packed)) tpm_sign_t;

    if(response_size < sizeof(tpm_sign_t)) {
        PRINTLOG(TPM, LOG_ERROR, "TPM Sign response too small to contain signature header");
        memory_free(response_data);
        return -1;
    }

    tpm_sign_t* tpm_sign = (tpm_sign_t*) response_data;

    tpm_sign->sign_len = BYTE_SWAP32(tpm_sign->sign_len);
    tpm_sign->sign_alg = BYTE_SWAP16(tpm_sign->sign_alg);
    tpm_sign->hash_alg = BYTE_SWAP16(tpm_sign->hash_alg);

    if(response_size < tpm_sign->sign_len) {
        PRINTLOG(TPM, LOG_ERROR, "TPM Sign response size mismatch with signature length. Expected at least %u, got %llu",
                 tpm_sign->sign_len, response_size);
        memory_free(response_data);
        return -1;
    }

    if(tpm_sign->sign_alg != TPM_ALG_ECDSA) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected signature algorithm in TPM Sign response: 0x%04x", tpm_sign->sign_alg);
        memory_free(response_data);
        return -1;
    }

    if(tpm_sign->hash_alg != digest_alg) {
        PRINTLOG(TPM, LOG_ERROR, "unexpected hash algorithm in TPM Sign response: 0x%04x", tpm_sign->hash_alg);
        memory_free(response_data);
        return -1;
    }

    typedef struct tpm_ecdsa_signature_part_t {
        uint16_t part_size;
        uint8_t  part_buffer[];
        // Followed by s_size and s_buffer
    } __attribute__((packed)) tpm_ecdsa_signature_part_t;

    tpm_ecdsa_signature_part_t* ecdsa_sig_r = (tpm_ecdsa_signature_part_t*)(void*)(response_data +
                                                                                   sizeof(tpm_sign_t));

    ecdsa_sig_r->part_size = BYTE_SWAP16(ecdsa_sig_r->part_size);

    tpm_ecdsa_signature_part_t* ecdsa_sig_s = (tpm_ecdsa_signature_part_t*)(void*)(response_data +
                                                                                   sizeof(tpm_sign_t) +
                                                                                   sizeof(tpm_ecdsa_signature_part_t) +
                                                                                   ecdsa_sig_r->part_size);

    ecdsa_sig_s->part_size = BYTE_SWAP16(ecdsa_sig_s->part_size);

    *sign_len = ecdsa_sig_r->part_size + ecdsa_sig_s->part_size;
    *sign     = memory_malloc(*sign_len);
    if (!*sign) {
        PRINTLOG(TPM, LOG_ERROR, "failed to allocate memory for signature");
        memory_free(response_data);
        return -1;
    }

    // Copy r and s parts into a single buffer (concatenated)
    memory_memcopy(ecdsa_sig_r->part_buffer, *sign, ecdsa_sig_r->part_size);
    memory_memcopy(ecdsa_sig_s->part_buffer, *sign + ecdsa_sig_r->part_size, ecdsa_sig_s->part_size);

    memory_free(response_data);

    return 0;
}

static int8_t tpm2_self_test (void) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    typedef struct tpm2_self_test_cmd_t {
        uint8_t fullTest; // 0 for partial self-test, 1 for full self-test
    } __attribute__((packed)) tpm2_self_test_cmd_t;

    tpm2_self_test_cmd_t cmd_data = {
        .fullTest = 1, // Request a full self-test
    };

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM2_CC_SELF_TEST, (uint8_t*) &cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM SelfTest command");
        return -1; // failed to send command
    }

    if (tpm2_read_response(0, TPM_ST_NO_SESSIONS) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM SelfTest response");
        return -1;
    }

    // We can optionally check the response code here if needed.

    return 0;
}

static int8_t tpm2_startup (tpm2_startup_types startup_type) {
    if(!tpm2_device) {
        PRINTLOG(TPM, LOG_ERROR, "tpm2 device not initialized");
        return -1; // TPM not initialized
    }

    if(tpm2_set_locality(true) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to set TPM locality");
        return -1; // failed to set locality
    }

    typedef struct tpm2_startup_cmd_t {
        uint16_t startupType;
    } __attribute__((packed)) tpm2_startup_cmd_t;

    tpm2_startup_cmd_t cmd_data = {
        .startupType = BYTE_SWAP16(startup_type),
    };

    if(tpm2_send_command(TPM_ST_NO_SESSIONS, TPM2_CC_STARTUP, (uint8_t*) &cmd_data, sizeof(cmd_data)) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to send TPM Startup command");
        return -1; // failed to send command
    }

    tpm2_return_code_t resp_code = TPM2_RC_INITIALIZE; // TPM may return TPM2_RC_INITIALIZE if it was already started, so we handle that case in the response reading.

    if (tpm2_read_response(0, TPM_ST_NO_SESSIONS, .return_code = &resp_code) != 0 && !(resp_code == TPM2_RC_INITIALIZE || resp_code == TPM2_RC_SUCCESS)) {
        PRINTLOG(TPM, LOG_ERROR, "failed to read TPM Startup response");
        return -1;
    }

    // We can optionally check the response code here if needed.

    return 0;
}

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

    frame_t* tpm2_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*) tpm2_base);

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
    tpm2_device->size         = tpm2_size;

    if(tpm2_startup(TPM2_SU_CLEAR) != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to startup TPM");
        return -1;
    }

    if(tpm2_self_test() != 0) {
        PRINTLOG(TPM, LOG_ERROR, "failed to perform TPM self test");
        return -1;
    }

    return 0;
}
