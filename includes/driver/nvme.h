/**
 * @file nvme.h
 * @brief nvme interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___DRIVER_NVME_H
/*! prevent duplicate header error macro */
#define ___DRIVER_NVME_H 0

#include <types.h>
#include <memory.h>
#include <linkedlist.h>
#include <pci.h>
#include <utils.h>
#include <future.h>
#include <hashmap.h>
#include <disk.h>

/**
 * @brief initialize nvme devices
 * @param[in] heap the heap for storing data
 * @param[in] nvme_pci_devices pci device list contains nvmes
 * @return nvme disk count
 */
int8_t nvme_init(memory_heap_t* heap, linkedlist_t* nvme_pci_devices);

/**
 * @union nvme_controller_cap_t
 * @brief nvme \ref nvme_controller_registers_t nvme cap field
 */
typedef union nvme_controller_cap_t {
    struct nvme_controller_cap_fields_t {
        uint64_t mqes      : 16; ///< maximum queue entries supported
        uint64_t cqr       : 1; ///< contiguous queues required
        uint64_t ams       : 2; ///< arbitration mechanism supported
        uint64_t reserved0 : 5; ///< reserved
        uint64_t timeout   : 8; ///< timeout in 500ms units
        uint64_t dstrd     : 4; ///< doorbell stride
        uint64_t nssrs     : 1; ///< nvme subsystem reset supported
        uint64_t css       : 8; ///< command sets supported
        uint64_t bps       : 1; ///< boot partition support
        uint64_t reserved1 : 2; ///< reserved
        uint64_t mpsmin    : 4; ///< memory page size minimum
        uint64_t mpsmax    : 4; ///< memory page size maximum
        uint64_t pmrs      : 1; ///< persistent memory region supported
        uint64_t cmbs      : 1; ///< controller memory buffer supported
        uint64_t reserved2 : 6; ///< reserved
    } __attribute__((packed))       fields; ///< detailed fields
    uint64_t bits; ///< bit group
} __attribute__((packed)) nvme_controller_cap_t; ///< union shorthand

/**
 * @struct nvme_controller_version_t
 * @brief nvme \ref nvme_controller_registers_t version field
 */
typedef struct nvme_controller_version_t {
    uint32_t reserved_or_ter : 8; ///< for >=1.2.1 it is tertiary version otherwise reserved
    uint32_t minor           : 8; ///<minor version
    uint32_t major           : 16; ///< major version
}__attribute__((packed)) nvme_controller_version_t; ///< shorthand for struct

/**
 * @struct nvme_controller_cfg_t
 * @brief nvme controller configuration
 */
typedef struct nvme_controller_cfg_t {
    uint32_t enable    : 1; ///< enable controller
    uint32_t reserved0 : 3; ///< reserved
    uint32_t css       : 3; ///< io command set selected
    uint32_t mps       : 4; ///< memory page size
    uint32_t ams       : 3; ///< arbitration mechanism selected
    uint32_t shn       : 2; ///< shutdown notification
    uint32_t iosqes    : 4; ///< io submission queue entry size
    uint32_t iocqes    : 4; ///< io completion queue entry size
    uint32_t reserved1 : 8; ///< reserved
}__attribute__((packed)) nvme_controller_cfg_t; ///< shorthand for struct

/**
 * @struct nvme_controller_sts_t
 * @brief nvme controller status filed
 */
typedef struct nvme_controller_sts_t {
    uint32_t ready    : 1; ///< controller is ready
    uint32_t cfs      : 1; ///< controller fatal status
    uint32_t shst     : 2; ///< shutdown status
    uint32_t nssro    : 1; ///< nvme subsystem reset occurred
    uint32_t pp       : 1; ///< processing paused
    uint32_t reserved : 26; ///< reserved
}__attribute__((packed)) nvme_controller_sts_t; ///< shorthand for struct

/**
 * @struct nvme_controller_aqa_t
 * @brief nvme admin queue attributes
 */
typedef union nvme_controller_aqa_t {
    struct {
        uint32_t asqs      : 12; ///< admin submission queue size
        uint32_t reserved0 : 4; ///< reserved
        uint32_t acqs      : 12; ///< admin completion queue size
        uint32_t reserved1 : 4; ///< reserved
    }__attribute__((packed)) fields; ///< detailed fields
    uint32_t bits; ///< bit group
}__attribute__((packed)) nvme_controller_aqa_t; ///< shorthand for struct

/**
 * @struct nvme_controller_cmbloc_t
 * @brief controller memory buffer location
 */
typedef struct nvme_controller_cmbloc_t {
    uint32_t bir      : 2; ///< base indicator support
    uint32_t cqmms    : 1; ///< cmb queue mixed memory support
    uint32_t cqpds    : 1; ///< cmb physical discontiguous support
    uint32_t cdpmls   : 1; ///< cmb data pointer mixed locations support
    uint32_t cdpcils  : 1; ///< cmb data pointer andi command independent locations support
    uint32_t cdmmms   : 1; ///< cmd data metadata mixed memory support
    uint32_t cqda     : 1; ///< cmd data dword alignment
    uint32_t reserved : 3; ///< reserved
    uint32_t offset   : 20; ///< offset
}__attribute__((packed)) nvme_controller_cmbloc_t; ///< shorthand for struct

/**
 * @struct nvme_controller_cmbsz_t
 * @brief nvme controller memory buffer size field
 */
typedef struct nvme_controller_cmbsz_t {
    uint32_t sqs      : 1; ///< submission queue support
    uint32_t cqs      : 1; ///< completion queue support
    uint32_t lists    : 1; ///< prp sgl lis support
    uint32_t rds      : 1; ///< read data support
    uint32_t wds      : 1; ///< write data support
    uint32_t reserved : 3; ///<reserved
    uint32_t szu      : 4; ///< size unit
    uint32_t size     : 20; ///< size
}__attribute__((packed)) nvme_controller_cmbsz_t; ///< shorthand for struct

/**
 * @struct nvme_controller_bpinfo_t
 * @brief nvme controller boot partition information
 */
typedef struct nvme_controller_bpinfo_t {
    uint32_t bpsz      : 15; ///< boot partition size
    uint32_t reserved0 : 9; ///< reserved
    uint32_t brs       : 2; ///< boot read status
    uint32_t reserved1 : 5; ///< reserved
    uint32_t abpid     : 1; ///< active boot partition id
}__attribute__((packed)) nvme_controller_bpinfo_t; ///<shorthand for struct

/**
 * @struct nvme_controller_bprsel_t
 * @brief nvme controller boot partition read select field
 */
typedef struct nvme_controller_bprsel_t {
    uint32_t bprsz    : 10; ///< boot partition read size
    uint32_t bprof    : 20; ///< boot partition read offset
    uint32_t reserved : 1; ///< reserved
    uint32_t bpid     : 1; ///< boot partition id
}__attribute__((packed)) nvme_controller_bprsel_t; ///< shorthand for struct

/**
 * @struct nvme_controller_cmbmsc_t
 * @brief nvme controller controller memory buffer memory space control field
 */
typedef struct nvme_controller_cmbmsc_t {
    uint64_t cre      : 1; ///< capabilities register enable
    uint64_t cmse     : 1; ///< controller memory space enable
    uint64_t reserved : 10; ///< reserved
    uint64_t cba      : 52; ///< controller base address
}__attribute__((packed)) nvme_controller_cmbmsc_t; ///< shorthand for struct

/**
 * @struct nvme_controller_cmbsts_t
 * @brief nvme controller controller memory buffer status field
 */
typedef struct nvme_controller_cmbsts_t {
    uint32_t cbai     : 1; ///< controller base address invalid
    uint32_t reserved : 31; ///< reserved
}__attribute__((packed)) nvme_controller_cmbsts_t;


/**
 * @struct nvme_controller_registers_t
 * @brief nvme controller register at bar0 and bar1
 */
typedef struct nvme_controller_registers_t {
    nvme_controller_cap_t     capabilities; ///< capabilities of nvme controller
    nvme_controller_version_t version; ///< version
    uint32_t                  intms; ///< interrupt mask set
    uint32_t                  intmc; ///< interrupt mask clear
    nvme_controller_cfg_t     config; ///< controller configuration
    uint8_t                   reserved0[0x1B - 0x18 + 1]; ///< reserved
    nvme_controller_sts_t     status; ///< controller status
    uint32_t                  nssr; ///< nvm subsystem reset
    nvme_controller_aqa_t     aqa; ///< admin queue attributes
    uint64_t                  asq; ///< admin submission queue base address
    uint64_t                  acq; ///< admin completion queue base address
    nvme_controller_cmbloc_t  cmbloc; ///< controller memory buffer location
    nvme_controller_cmbsz_t   cmbsz; ///< controller memory buffer size
    nvme_controller_bpinfo_t  bpinfo; ///< boot partition information
    nvme_controller_bprsel_t  bprsel; ///< boot partition read select
    uint64_t                  bpmbl; ///< boot partition memory buffer location
    nvme_controller_cmbmsc_t  cmbmsc;///< controller memory buffer memory space control
    nvme_controller_cmbsts_t  cmbsts; ///< controller memory buffer status
    uint8_t                   reserved1[0xDFF - 0x5C + 1];///< reserved
    uint32_t                  pmrcap; ///< persistent memory capabilities
    uint32_t                  pmrctl; ///< persistent memory region control
    uint32_t                  pmrsts; ///< persistent memory region status
    uint32_t                  pmrebs; ///< persistent memory region elasticity buffer size
    uint32_t                  pmrswtp; ///< persistent memory region sustained write throughput
    uint64_t                  pmrmsc; ///< persistent memory region controller memory space control
    uint8_t                   reserved2[0xFFF - 0xE1C + 1]; ///< reserved command set specific
}__attribute__((packed)) nvme_controller_registers_t; ///<shorthand for struct

/**
 * @struct nvme_submission_queue_entry_t
 * @brief nvme submission queue entry
 */
typedef struct nvme_submission_queue_entry_t {
    uint32_t opc       : 8; ///< operation common_header
    uint32_t fuse      : 2; ///< fused command (two command)
    uint32_t reserved0 : 4; ///< reserved
    uint32_t psdt      : 2; ///< prp or sgl
    uint32_t cid       : 16; ///< command identifier
    uint32_t nsid; ///< namespace id
    uint64_t reserved1; ///< reserved
    uint64_t mptr; ///< metadata pointer
    union nvme_submission_queue_entry_datapointer_t {
        struct nvme_submission_queue_entry_prplist_t {
            uint64_t prp1; ///< first prp list item
            uint64_t prp2; ///< second prp list item
        }__attribute__((packed)) prplist; ///< prp list
        uint8_t sgl[16]; ///< sgl list entry
    }__attribute__((packed)) dptr; ///< data pointer
    uint32_t cdw10; ///< command dword 10
    uint32_t cdw11; ///< command dword 11
    uint32_t cdw12; ///< command dword 12
    uint32_t cdw13; ///< command dword 13
    uint32_t cdw14; ///< command dword 14
    uint32_t cdw15; ///< command dword 15
}__attribute__((packed)) nvme_submission_queue_entry_t;

/**
 * @struct nvme_completion_queue_entry_t
 * @brief nvme completion queue entry fields
 */
typedef struct nvme_completion_queue_entry_t {
    uint32_t cdw0; ///< completion dword 0 command specific
    uint32_t reserved; ///< reserved
    uint32_t sqhd                : 16; ///< submission queue head pointer
    uint32_t sqid                : 16; ///< submission queue identifier
    uint32_t cid                 : 16; ///< command identifier
    uint32_t p                   : 1; ///< phase tag
    uint32_t status_code         : 8; ///< status of command
    uint32_t status_type         : 3; ///< status type
    uint32_t command_retry_delay : 2; ///< command retry delay
    uint32_t more                : 1; ///< more
    uint32_t do_not_retry        : 1; ///< do not retry
}__attribute__((packed)) nvme_completion_queue_entry_t; ///< shorthand for struct

/**
 * @struct nvme_identify_t
 * @brief nvme identify data fields
 */
typedef struct nvme_identify_t {
    uint16_t vid; ///< PCI vendor ID
    uint16_t ssvid; ///< PCI subsystem vendor ID
    char_t   sn[20]; ///< Serial number
    char_t   mn[40]; ///< model number
    uint64_t fr; ///< Firmware revision
    uint8_t  rab; ///< Recommended arbitration burst
    uint8_t  ieee[3]; ///< ieee data
    uint8_t  cmic; ///< Controller multi-path I/O and namespace sharing capabilities
    uint8_t  mdts; ///< Maximum data transfer size
    uint16_t cntlid; ///< Controller ID
    uint32_t ver; ///< Version
    uint32_t rtd3r; ///< RTD3 resume latency
    uint32_t rtd3e; ///< RTD3 entry latency
    uint32_t oaes; ///< Optional asynchronous events supported
    uint32_t ctratt; ///< Controller attributes
    uint8_t  reserved0[256 - 100]; ///< reserved
    uint16_t oacs; ///< Optional admin command support
    uint8_t  acl; ///< Abort command limit
    uint8_t  aerl; ///< Asynchronous event request limit
    uint8_t  frmw; ///< Firmware updates
    uint8_t  lpa; ///< Log page attributes
    uint8_t  elpe; ///< Error log page attributes
    uint8_t  npss; ///< Number of power states supported
    uint8_t  avscc; ///< Admin vendor specific command configuration
    uint8_t  apsta; ///< Autonomous power state transition attributes
    uint16_t wctemp; ///< Warning composite temperature threshold
    uint16_t cctemp; ///< Critical composite temperature threshold
    uint16_t mtfa; ///< maximum time for firmware activation
    uint32_t hmpre; ///< Host memory buffer preferred size
    uint32_t hmmin; ///< Host memory buffer minimum size
    uint64_t tnvmcap_lo; ///< Total NVM capacity low 64 bits
    uint64_t tnvmcap_hi; ///< Total NVM capacity high 64 bits
    uint64_t unvmcap_lo; ///< Unallocated NVM capacity low 64 bits
    uint64_t unvmcap_hi; ///< Unallocated NVM capacity high 64 bits
    uint32_t rpmbs; ///< Replay protected memory block support
    uint32_t reserved1; ///< reserved
    uint16_t kas; ///< Keepalive support
    uint8_t  reserved2[190]; ///< reserved
    uint8_t  sqes; ///< Submission queue entry size
    uint8_t  cqes; ///< Completion queue entry size
    uint16_t maxcmd; ///< Maximum outstanding commands
    uint32_t nn; ///< Number of namespaces
    uint16_t oncs; ///< Optional nvm command support
    uint16_t fuses; ///< Fused operation support
    uint8_t  vna; ///< Format NVM attributes
    uint8_t  vwc; ///< Volatile write cache
    uint16_t awun; ///< Atomic write unit normal
    uint16_t awupf; ///< Atomic write unit power fail
    uint8_t  nvscc; ///< NVM vendor specific command confiuration
    uint8_t  reserved3; ///< reserved
    uint16_t acwu; ///< Acomit compare and write unit
    uint16_t reserved4; ///< reserved
    uint32_t sgls; ///< SGL support
    uint8_t  reserved5[768 - 540]; ///< reserved
    uint8_t  subnqn[256]; ///< NVM subsystem qualified name
    uint8_t  reserved6[2048 - 1024]; ///< reserved
    uint8_t  psd[32][32]; ///< Power state descriptors
    uint8_t  vs[4096 - 3072]; ///< Vendor specific
}__attribute__((packed)) nvme_identify_t; ///< shorthand for struct

typedef struct nvme_lba_format_t {
    uint32_t ms       : 16; ///< Metadata size
    uint32_t lbads    : 8; ///< LBA data size
    uint32_t rp       : 2; ///< Relative performance
    uint32_t reserved : 6; ///< reserved
}__attribute__((packed)) nvme_lba_format_t;


/**
 * @struct nvme_ns_identify_t
 * @brief nvme namespace identify data fields
 */
typedef struct nvme_ns_identify_t {
    uint64_t          nsze; ///< Namespace size
    uint64_t          ncap; ///< Namespace capacity
    uint64_t          nuse; ///< Namespace utilization
    uint8_t           nsfeat; ///< Namespace features
    uint8_t           nlbaf; ///< Number of LBA formats
    uint8_t           flbas; ///< Formatted LBA size
    uint8_t           mc; ///< Metadata capabilities
    uint8_t           dpc; ///< End to end data protection capabilitities
    uint8_t           dps; ///< End to end protection type settings
    uint8_t           nmic; ///< Namespace multipath I/O and namespace sharing capabilities
    uint8_t           rescap; ///< Reservation capabilities
    uint8_t           fpi; ///< Format progress indicator
    uint8_t           reserved0; ///< reserved
    uint16_t          nawun; ///< Namespace atomic write unit normal
    uint16_t          nawupf; ///< namespace atomic write unit power fail
    uint16_t          nacwu; ///< namespace atomic compare and write unit
    uint16_t          nsabsn; ///< namespace atomic boundary size normal
    uint16_t          nsabo; ///< namespace atomic boundary offset
    uint16_t          nabspf; ///< namespace atomic boundary size power fail
    uint16_t          reserved1; ///< reserved
    uint64_t          nvmcap_lo; ///< NVM capacity low 64 bits
    uint64_t          nvmcap_hi; ///< NVM capacity high 64 bits
    uint8_t           reserved2[104 - 64];
    uint8_t           nguid[16]; ///< namespace globally unique identifier
    uint64_t          eui64; ///< ieee extended unique identifier
    nvme_lba_format_t lbaf[16]; ///< LBA formats
    uint8_t           reserved3[4096 - 192]; ///< reserved
} __attribute__((packed)) nvme_ns_identify_t; ///< shorthand for struct

_Static_assert(offsetof_field(nvme_ns_identify_t, lbaf) == 128, "nvme_ns_identify_t size mismatch");

/**
 * @enum nvme_admin_cmd_opcode_t
 * @brief nvme admin commands
 **/
typedef enum nvme_admin_cmd_opcode_t {
    NVME_ADMIN_CMD_DELETE_SQ = 0X00, ///< nvme admin command for deleting submission queue
    NVME_ADMIN_CMD_CREATE_SQ = 0X01, ///< nvme admin command for creating submission queue
    NVME_ADMIN_CMD_GET_LOG_PAGE = 0X02, ///< nvme admin command for getting log page entries
    NVME_ADMIN_CMD_DELETE_CQ = 0X04, ///< nvme admin command for deleting completion queue
    NVME_ADMIN_CMD_CREATE_CQ = 0X05, ///< nvme admin command for creating completion queue
    NVME_ADMIN_CMD_IDENTIFY = 0X06, ///< nvme admin command for identify controller and name space
    NVME_ADMIN_CMD_ABORT = 0X08, ///< nvme admin command for aborting another command
    NVME_ADMIN_CMD_SET_FEATURES = 0X09, ///< nvme admin command for setting nvme features not in registers
    NVME_ADMIN_CMD_GET_FEATURES = 0X0A, ///< nvme admin command for getting nvme features not in registers
    NVME_ADMIN_CMD_ASYNC_EVNT_REQ = 0X0C,
    NVME_ADMIN_CMD_FIRMWARE_COMMIT = 0X10, ///< nvme admin command for uploading firmare
    NVME_ADMIN_CMD_FIRMWARE_DOWNLOAD = 0X11, ///< nvme admin command for downloading firmware
    NVME_ADMIN_CMD_NS_ATTACH = 0X15, ///< nvme admin command for attaching namespace
    NVME_ADMIN_CMD_KEEP_ALIVE = 0X18, ///< nvme admin command for keeping alive
    NVME_ADMIN_CMD_FORMAT_NVM = 0X80, ///< nvme admin command for formatting nvme
} nvme_admin_cmd_opcode_t; ///< shorthand for enum

/**
 * @enum nvme_cmd_opcode_t
 * @brief nvme io commands
 **/
typedef enum nvme_cmd_opcode_t {
    NVME_CMD_FLUSH = 0X00, ///< nvme io command for flush
    NVME_CMD_WRITE = 0X01, ///< nvme io command for write
    NVME_CMD_READ = 0X02, ///< nvme io command for read
    NVME_CMD_WRITE_UNCORRECTABLE = 0X4,
    NVME_CMD_COMPARE = 0X05,
    NVME_CMD_WRITE_ZEROS = 0X08,
    NVME_CMD_DATASET_MGMT = 0X09,
    NVME_CMD_RESERVATION_REG = 0X0D,
    NVME_CMD_RESERVATION_REP = 0X0E,
    NVME_CMD_RESERVATION_ACQ = 0X11,
    NVME_CMD_RESERVATION_REL = 0X15,
} nvme_cmd_opcode_t; ///< shorthand for enum

/**
 * @enum nvme_feat_id_t
 * @brief nvme feature id's
 **/
typedef enum nvme_feat_id_t {
    NVME_FEAT_HOST_MEM_BUF = 0XD,
    NVME_FEAT_NUM_QUEUES = 0X07,
} nvme_feat_id_t; ///< shorthand for enum

/**
 * @enum nvme_cmd_status_t
 * @brief nvme command's status codes
 **/
typedef enum nvme_cmd_status_t {
    NVME_CMD_STATUS_SUCCESS = 0X0, ///< nvme command is succeed
    NVME_CMD_STATUS_INVALID_CMD = 0X1, ///< nvme command is invalid
    NVME_CMD_STATUS_INVALID_FIELD = 0X2,
    NVME_CMD_STATUS_CMD_ID_CONFLICT = 0X3,
    NVME_CMD_STATUS_DATA_XFER_ERROR = 0X4,
    NVME_CMD_STATUS_ABORTED_PWR_LOSS = 0X5,
    NVME_CMD_STATUS_INTERNAL_ERROR = 0X6,
    NVME_CMD_STATUS_CMD_ABORT_REQUESTED = 0X7,
    NVME_CMD_STATUS_CMD_ABORT_SQ_DELETED = 0X8,
    NVME_CMD_STATUS_CMD_ABORT_FUSED_CMD = 0X9,
    NVME_CMD_STATUS_CMD_ABORT_MISSING_FUSED_CMD = 0XA,
    NVME_CMD_STATUS_INVALID_NS_OR_FMT = 0XB,
    NVME_CMD_STATUS_CMD_SEQ_ERROR = 0XC,
    NVME_CMD_STATUS_INVALID_SGL_SEG_DESC = 0XD,
    NVME_CMD_STATUS_INVALID_SGL_COUNT = 0XE,
    NVME_CMD_STATUS_INVALID_DATA_SGL_LEN = 0XF,
    NVME_CMD_STATUS_INVALID_MD_GL_LEN = 0X10,
    NVME_CMD_STATUS_INVALID_SGL_DESC_TYPE = 0X11,
    NVME_CMD_STATUS_INVALID_USE_CTRL_MEM = 0X12,
    NVME_CMD_STATUS_INVALID_PRP_OFS = 0X13,
    NVME_CMD_STATUS_ATOMIC_WRITE_UNIT_EXCEEDED = 0X14,
    NVME_CMD_STATUS_INVALID_SGL_OFS = 0X16,
    NVME_CMD_STATUS_INVALID_SGL_SUBTYPE = 0X17,
    NVME_CMD_STATUS_INCONSISTENT_HOST_ID = 0X18,
    NVME_CMD_STATUS_KEEPALIVE_EXPIRED = 0X19,
    NVME_CMD_STATUS_KEEPALIVE_INVALID = 0X1A,
    NVME_CMD_STATUS_LBA_OUT_OF_RANGE = 0X80,
    NVME_CMD_STATUS_CAPACITY_EXCEEDED = 0X81,
    NVME_CMD_STATUS_NS_NOT_READY = 0X82, ///< namespace is not ready
    NVME_CMD_STATUS_RESERVATION_CONFLICT = 0X83,
    NVME_CMD_STATUS_FORMAT_IN_PROGRESS = 0X84, ///< nvme formatting in progress
} nvme_cmd_status_t; ///< shorthand for enum



typedef struct nvme_disk_t {
    memory_heap_t*                 heap; ///< heap to allocate memory from
    uint64_t                       disk_id; ///< disk id
    pci_generic_device_t*          pci_device; ///< pci device
    nvme_controller_registers_t*   nvme_registers; ///< nvme registers
    pci_capability_msix_t*         msix_capability; ///< msix capability
    uint64_t                       admin_queue_size; ///< admin queue size
    uint64_t                       admin_s_queue_tail; ///< admin submission queue tail
    uint64_t                       admin_c_queue_head; ///< admin completion queue head
    uint32_t*                      admin_submission_queue_tail_doorbell; ///< admin submission queue tail doorbell
    uint32_t*                      admin_completion_queue_head_doorbell; ///< admin completion queue head doorbell
    nvme_submission_queue_entry_t* admin_submission_queue; ///< admin submission queue
    nvme_completion_queue_entry_t* admin_completion_queue; ///< admin completion queue
    uint64_t                       io_queue_size; ///< io queue size
    uint64_t                       io_s_queue_tail; ///< io queue tail
    uint64_t                       io_c_queue_head; ///< io completion queue head
    uint32_t*                      io_submission_queue_tail_doorbell; ///< io submission queue tail doorbell
    uint32_t*                      io_completion_queue_head_doorbell; ///< io completion queue head doorbell
    nvme_submission_queue_entry_t* io_submission_queue; ///< io submission queue
    nvme_completion_queue_entry_t* io_completion_queue; ///< io completion queue
    nvme_identify_t*               identify; ///< identify
    nvme_ns_identify_t*            ns_identify; ///< namespace identify
    uint32_t*                      active_ns_list; ///< active namespace list
    uint64_t                       timeout; ///< timeout
    uint32_t                       ns_id; ///< namespace id
    uint64_t                       lba_count; ///< lba count
    uint32_t                       lba_size; ///< lba size
    uint16_t                       next_cid; ///< next command id
    boolean_t                      flush_supported; ///< flush supported
    uint16_t                       io_sq_count; ///< io submission queue count
    uint16_t                       io_cq_count; ///< io completion queue count
    uint64_t                       io_queue_isr; ///< io queue isr
    hashmap_t*                     command_lock_map; ///< command lock map
    uint64_t                       prp_frame_fa; ///< prp frame fa
    uint64_t                       prp_frame_va; ///< prp frame va
    uint64_t                       max_prp_entries; ///< max prp entries
} nvme_disk_t; ///< shorthand for struct


future_t           nvme_read(uint64_t disk_id, uint64_t lba, uint32_t size, uint8_t* buffer);
future_t           nvme_write(uint64_t disk_id, uint64_t lba, uint32_t size, uint8_t* buffer);
future_t           nvme_flush(uint64_t disk_id);
disk_t*            nvme_disk_impl_open(nvme_disk_t* nvme_disk);
const nvme_disk_t* nvme_get_disk_by_id(uint64_t disk_id);
#endif
