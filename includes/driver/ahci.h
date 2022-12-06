/**
 * @file ahci.h
 * @brief ahci interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___DRIVER_AHCI_H
/*! prevent duplicate header error macro */
#define ___DRIVER_AHCI_H 0

#include <types.h>
#include <memory.h>
#include <linkedlist.h>
#include <cpu/sync.h>
#include <future.h>
#include <disk.h>
#include <pci.h>

#define AHCI_SATA_SIG_ATA    0x00000101  // SATA drive
#define AHCI_SATA_SIG_ATAPI  0xEB140101  // SATAPI drive
#define AHCI_SATA_SIG_SEMB   0xC33C0101  // Enclosure management bridge
#define AHCI_SATA_SIG_PM     0x96690101  // Port multiplier

#define AHCI_HBA_PORT_IPM_ACTIVE   1
#define AHCI_HBA_PORT_DET_PRESENT  3

#define AHCI_HBA_PxCMD_ST    0x0001
#define AHCI_HBA_PxCMD_FRE   0x0010
#define AHCI_HBA_PxCMD_FR    0x4000
#define AHCI_HBA_PxCMD_CR    0x8000
#define AHCI_HBA_PxIS_TFES   (1 << 30)

#define AHCI_ATA_CMD_READ_DMA_EXT           0x25
#define AHCI_ATA_CMD_READ_LOG_EXT           0x2F
#define AHCI_ATA_CMD_WRITE_DMA_EXT          0x35
#define AHCI_ATA_CMD_READ_LOG_DMA_EXT       0x47
#define AHCI_ATA_CMD_READ_FPDMA_QUEUED      0x60
#define AHCI_ATA_CMD_WRITE_FPDMA_QUEUED     0x61
#define AHCI_ATA_CMD_FLUSH_EXT              0xEA
#define AHCI_ATA_CMD_IDENTIFY               0xEC

#define AHCI_ATA_DEV_BUSY 0x80
#define AHCI_ATA_DEV_DRQ  0x08

#define PCI_DEVICE_CAPABILITY_SATA    0x12

typedef struct {
	pci_capability_t cap;
	uint16_t revision_minor : 4;
	uint16_t revision_major : 4;
	uint16_t reserved0 : 8;
	uint32_t bar_location : 4;
	uint32_t bar_offset : 20;
	uint32_t reserved1 : 8;
}__attribute__((packed)) ahci_pci_capability_sata_t;

typedef struct {
	uint16_t config;              ///< General Configuration.
	uint16_t cylinders;           ///< Number of Cylinders.
	uint16_t reserved_2;
	uint16_t heads;               ///< Number of logical heads.
	uint16_t vendor_data1;
	uint16_t vendor_data2;
	uint16_t sectors_per_track;
	uint16_t vendor_specific_7_9[3];
	char_t serial_no[20];         ///< ASCII
	uint16_t vendor_specific_20_21[2];
	uint16_t ecc_bytes_available;
	char_t firmware_version[8];       ///< ASCII
	char_t model_name[40];        ///< ASCII
	uint16_t multi_sector_cmd_max_sct_cnt;
	uint16_t reserved_48;
	uint16_t capabilities;
	uint16_t reserved_50;
	uint16_t pio_cycle_timing;
	uint16_t reserved_52;
	uint16_t field_validity;
	uint16_t current_cylinders;
	uint16_t current_heads;
	uint16_t current_sectors;
	uint32_t current_capacity;
	uint16_t reserved_59;
	uint32_t user_addressable_sectors;
	uint16_t reserved_62;
	uint16_t multi_word_dma_mode;
	uint16_t advanced_pio_modes;
	uint16_t min_multi_word_dma_cycle_time;
	uint16_t rec_multi_word_dma_cycle_time;
	uint16_t min_pio_cycle_time_without_flow_control;
	uint16_t min_pio_cycle_time_with_flow_control;
	uint16_t additional_supported;
	uint16_t reserved_70;
	uint16_t reserved_71_74[4];
	uint16_t queue_depth;
	uint16_t serial_ata_capabilities;
	uint16_t reserved_77;                                ///< Reserved for Serial ATA
	uint16_t serial_ata_features_supported;
	uint16_t serial_ata_features_enabled;
	uint16_t major_version_no;
	uint16_t minor_version_no;
	uint16_t command_set_supported_82;     ///< word 82
	uint16_t command_set_supported_83;     ///< word 83
	uint16_t command_set_supported_84;     ///< word 84
	uint16_t command_set_feature_enabled_85;   ///< word 85
	uint16_t command_set_feature_enabled_86;  ///< word 86
	uint16_t command_set_feature_enabled_87;  ///< word 87
	uint16_t ultra_dma_mode;               ///< word 88
	uint16_t reserved_89_99[11];
	uint64_t user_addressable_sectors_ext; ///< word 100
	uint16_t reserved_104_118[15];
	uint16_t command_set_supported_119;   ///< word 119
	uint16_t command_set_feature_enabled_120;  ///< word 120
	uint16_t reserved_121_127[7];
	uint16_t security_status;
	uint16_t vendor_data_129_159[31];
	uint16_t reserved_160_255[96];
}__attribute__((packed)) ahci_ata_identify_data_t;

typedef struct {
	uint8_t ncq_tag : 4;
	uint8_t reserved0 : 1;
	uint8_t unload : 1;
	uint8_t nq : 1;
	uint8_t reserved1;
	uint8_t status;
	uint8_t error;
	uint64_t lba0 : 24;
	uint8_t device;
	uint64_t lba1 : 24;
	uint8_t reserved2;
	uint16_t count;
	uint8_t sense_key;
	uint8_t additional_sense_code;
	uint8_t additional_sense_qualifier;
	uint8_t reserved3[239];
	uint8_t vendor_specific[255];
	uint8_t checksum;
}__attribute__((packed)) ahci_ata_ncq_error_log_t;

typedef enum {
	AHCI_FIS_TYPE_REG_H2D    = 0x27, ///< Register FIS - host to device
	AHCI_FIS_TYPE_REG_D2H    = 0x34, ///< Register FIS - device to host
	AHCI_FIS_TYPE_DMA_ACT    = 0x39, ///< DMA activate FIS - device to host
	AHCI_FIS_TYPE_DMA_SETUP  = 0x41, ///< DMA setup FIS - bidirectional
	AHCI_FIS_TYPE_DATA       = 0x46, ///< Data FIS - bidirectional
	AHCI_FIS_TYPE_BIST       = 0x58, ///< BIST activate FIS - bidirectional
	AHCI_FIS_TYPE_PIO_SETUP  = 0x5F, ///< PIO setup FIS - device to host
	AHCI_FIS_TYPE_DEV_BITS   = 0xA1, ///< Set device bits FIS - device to host
} ahci_fis_type_t;

typedef enum {
	AHCI_DEVICE_NULL,
	AHCI_DEVICE_SATA,
	AHCI_DEVICE_SEMB,
	AHCI_DEVICE_PM,
	AHCI_DEVICE_SATAPI,
} ahci_device_type_t;

typedef struct {
	ahci_fis_type_t fis_type : 8;   ///< fis type
	uint8_t port_multiplier : 4;    ///< Port multiplier
	uint8_t reserved0 : 3;          ///< Reserved
	uint8_t control_or_command : 1; ///< 1: Command, 0: Control
	uint8_t command;                ///< Command register
	uint8_t featurel;               ///< Feature register, 0:7
	uint64_t lba0 : 24;             ///< LBA low register, 0:23
	uint8_t device;                 ///< Device register
	uint64_t lba1 : 24;             ///< LBA register, 24:48
	uint8_t featureh;               ///< Feature register, 15:8
	uint16_t count;                 ///< Count register
	uint8_t icc;                    ///< Isochronous command completion
	uint8_t control;                ///< Control register
	uint8_t reserved1[4];           ///< Reserved
} __attribute((packed)) ahci_fis_reg_h2d_t;

typedef struct {
	ahci_fis_type_t fis_type : 8; ///< fis type
	uint8_t port_multiplier : 4;  ///< Port multiplier
	uint8_t reserved0 : 2;        ///< Reserved
	uint8_t interrupt : 1;        ///< Interrupt Bit
	uint8_t reserved1 : 1;        ///< Reserved
	uint8_t status;               ///< Status
	uint8_t error;                ///< Error
	uint64_t lba0 : 24;           ///< LBA low register, 0:23
	uint8_t device;               ///< Device register
	uint64_t lba1 : 24;           ///< LBA register, 24:48
	uint8_t reserved2;            ///< Reserved
	uint16_t count;               ///< Count register
	uint8_t reserved3[6];         ///< Reserved
} __attribute((packed)) ahci_fis_reg_d2h_t;

typedef struct {
	ahci_fis_type_t fis_type : 8; ///< fis type
	uint8_t port_multiplier : 4;  ///< Port multiplier
	uint8_t reserved0 : 4;        ///< Reserved
	uint8_t reserved1[2];         ///< Reserved
	uint32_t data[1];             ///< data 1~N
} __attribute((packed)) ahci_fis_reg_data_t;

typedef struct {
	ahci_fis_type_t fis_type : 8; ///< fis type
	uint8_t port_multiplier : 4;  ///< Port multiplier
	uint8_t reserved0 : 2;        ///< Reserved
	uint8_t interrupt : 1;        ///< Interrupt Bit
	uint8_t notification : 1;     ///< Notification Bit
	uint8_t status;               ///< Status
	uint8_t error;                ///< Error
	uint8_t reserved1[4];         ///< Reserved
} __attribute((packed)) ahci_fis_dev_bits_t;

typedef struct {
	ahci_fis_type_t fis_type : 8; ///< fis type
	uint8_t port_multiplier : 4;  ///< Port multiplier
	uint8_t reserved0 : 1;        ///< Reserved
	uint8_t direction : 1;        ///< 0: host2device 1: device2host
	uint8_t interrupt : 1;        ///< Interrupt Bit
	uint8_t reserved1 : 1;        ///< Reserved
	uint8_t status;               ///< Status
	uint8_t error;                ///< Error
	uint64_t lba0 : 24;           ///< LBA low register, 0:23
	uint8_t device;               ///< Device register
	uint64_t lba1 : 24;           ///< LBA register, 24:48
	uint8_t reserved2;            ///< Reserved
	uint16_t count;               ///< Count register
	uint8_t reserved3;            ///< Reserved
	uint8_t new_status;           ///< New Status
	uint16_t transfer_count;      ///< Transfer Count
	uint8_t reserved4[2];         ///< Reserved
} __attribute((packed)) ahci_fis_pio_setup_t;

typedef struct {
	ahci_fis_type_t fis_type : 8; ///< fis type
	uint8_t port_multiplier : 4;  ///< Port multiplier
	uint8_t reserved0 : 1;        ///< Reserved
	uint8_t direction : 1;        ///< 0: host2device 1: device2host
	uint8_t interrupt : 1;        ///< Interrupt Bit
	uint8_t auto_active : 1;      ///< Auto-activate. Specifies if DMA Activate FIS is needed
	uint8_t reserved1[2];         ///< Reserved
	uint64_t dma_buffer_id;       ///< DMA Buffer Identifier. Used to Identify DMA buffer in host memory. SATA Spec says host specific and not in Spec. Trying AHCI spec might work.
	uint32_t reserved2;           ///< Reserved
	uint32_t dma_buffer_offset;   ///< byte offset into buffer first 2 bits must be 0
	uint32_t transfer_count;      ///< Transfer Count bit 0 must 0
	uint32_t reserved3;           ///< Reserved
} __attribute((packed)) ahci_fis_dma_setup_t;

typedef volatile struct {
	uint64_t command_list_base_address;   ///< 0x00, command list base address, 1K-byte aligned
	uint64_t fis_base_address;            ///< 0x08, FIS base address, 256-byte aligned
	uint32_t interrupt_status;            ///< 0x10, interrupt status
	uint32_t interrupt_enable;            ///< 0x14, interrupt enable
	uint32_t command_and_status;          ///< 0x18, command and status
	uint32_t reserved0;                   ///< 0x1C, Reserved
	uint32_t task_file_data;              ///< 0x20, task file data
	uint32_t signature;                   ///< 0x24, signature
	uint32_t sata_status;                 ///< 0x28, SATA status (SCR0:SStatus)
	uint32_t sata_control;                ///< 0x2C, SATA control (SCR2:SControl)
	uint32_t sata_error;                  ///< 0x30, SATA error (SCR1:SError)
	uint32_t sata_active;                 ///< 0x34, SATA active (SCR3:SActive)
	uint32_t command_issue;               ///< 0x38, command issue
	uint32_t sata_notification;           ///< 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fis_based_switch_control;    ///< 0x40, FIS-based switch control
	uint32_t device_sleep;                ///< 0x44, device sleep
	uint32_t reserved1[10];               ///< 0x48 ~ 0x6F, Reserved
	uint32_t vendor[4];                   ///< 0x70 ~ 0x7F, vendor specific
} __attribute((packed)) ahci_hba_port_t;

typedef volatile struct {
	uint32_t host_capability;                       ///< 0x00, Host capability
	uint32_t global_host_control;                   ///< 0x04, Global host control
	uint32_t interrupt_status;                      ///< 0x08, Interrupt status
	uint32_t port_implemented;                      ///< 0x0C, Port implemented
	uint32_t version;                               ///< 0x10, Version
	uint32_t command_completion_coalescing_control; ///< 0x14, Command completion coalescing control
	uint32_t command_completion_coalescing_ports;   ///< 0x18, Command completion coalescing ports
	uint32_t enclosure_management_location;         ///< 0x1C, Enclosure management location
	uint32_t enclosure_management_control;          ///< 0x20, Enclosure management control
	uint32_t host_capability_extended;              ///< 0x24, Host capabilities extended
	uint32_t bios_os_handoff_control_and_status;    ///< 0x28, BIOS/OS handoff control and status
	uint8_t reserved[0xA0 - 0x2C];                  ///< reserved
	uint8_t vendor[0x100 - 0xA0];                   ///< vendor defined
	ahci_hba_port_t ports[0];                       ///< 1 ~ 32
} __attribute((packed)) ahci_hba_mem_t;

typedef volatile struct {
	ahci_fis_dma_setup_t dma_setup_fis;         ///< DMA Setup FIS
	uint8_t padding0[4];                        ///< padding
	ahci_fis_pio_setup_t pio_setup_fis;         ///< PIO Setup FIS
	uint8_t padding1[12];                       ///< padding
	ahci_fis_reg_d2h_t d2h_fis;                 ///< Register – Device to Host FIS
	uint8_t padding2[4];                        ///< padding
	ahci_fis_dev_bits_t sdbs_fis;               ///< Set Device Bit FIS
	uint8_t ufis[64];                           ///< unknown fis
	uint8_t reserved0[0x100 - 0xA0];            ///< reserved
}  __attribute((packed)) ahci_hba_fis_t;

typedef struct {
	uint8_t command_fis_length : 5;         ///<  Command FIS length in DWORDS, 2 ~ 16
	uint8_t atapi : 1;                      ///<  ATAPI
	uint8_t write_direction : 1;            ///<  Write, 1: H2D, 0: D2H
	uint8_t prefetchable : 1;               ///<  Prefetchable
	uint8_t reset : 1;                      ///<  Reset
	uint8_t bist : 1;                       ///<  BIST
	uint8_t clear_busy : 1;                 ///<  Clear busy upon R_OK
	uint8_t reserved0 : 1;                  ///<  Reserved
	uint8_t port_multiplier : 4;            ///<  Port multiplier port
	uint16_t prdt_length;                   ///<  Physical region descriptor table length in entries
	volatile uint32_t prd_transfer_count;   ///<  Physical region descriptor byte count transferred
	uint64_t prdt_base_address;             ///<  Command table descriptor base address
	uint32_t reserved1[4];                  ///< Reserved
}  __attribute((packed)) ahci_hba_cmd_header_t;

typedef struct {
	uint64_t data_base_address;        ///< Data base address
	uint32_t reserved0;                ///< Reserved
	uint32_t data_byte_count : 22;     ///< Byte count, 4M max
	uint32_t reserved1 : 9;            ///< Reserved
	uint32_t interrupt : 1;            ///< Interrupt on completion
}  __attribute((packed)) ahci_hba_prdt_entry_t;

typedef struct {
	uint8_t command_fis[64];               ///< Command FIS
	uint8_t acmd[16];                      ///< ATAPI command, 12 or 16 bytes
	uint8_t reserved[48];                  ///< Reserved
	ahci_hba_prdt_entry_t prdt_entry[1];   ///< Physical region descriptor table entries, 0 ~ 65535
} __attribute((packed)) ahci_hba_prdt_t;

typedef union {
	struct {
		uint8_t gpl_supported : 1;
		uint8_t gpl_enabled : 1;
		uint8_t dma_ext_supported : 1;
		uint8_t dma_ext_enabled : 1;
		uint8_t dma_ext_is_log_ext : 1;
		uint8_t reserved : 3;
	} __attribute((packed)) fields;
	uint8_t bits;
} ahci_ata_logging_t;

typedef union {
	struct {
		uint8_t supported : 1;
		uint8_t enabled : 1;
		uint8_t errlog_supported : 1;
		uint8_t errlog_enabled : 1;
		uint8_t selftest_supported : 1;
		uint8_t selftest_enabled : 1;
		uint8_t reserved : 2;
	} __attribute((packed)) fields;
	uint8_t bits;
} ahci_ata_smart_t;

typedef struct {
	uint64_t disk_id;
	ahci_device_type_t type;
	uint64_t port_address;
	uint16_t cylinders;
	uint16_t heads;
	uint16_t sectors;
	uint64_t lba_count;
	char_t serial[21];
	char_t model[41];
	uint8_t sncq; ///< native command queue support
	uint8_t volatile_write_cache;
	uint16_t queue_depth;
	ahci_ata_logging_t logging;
	ahci_ata_smart_t smart_status;
	uint8_t command_count;
	lock_t disk_lock;
	uint32_t acquired_slots;
	uint32_t current_commands;
	lock_t future_locks[32];
	boolean_t inserted;
}ahci_sata_disk_t;

typedef struct {
	uint64_t hba_addr;
	uint8_t revision_major;
	uint8_t revision_minor;
	uint8_t sata_bar;
	uint8_t sata_bar_offset;
	uint8_t intnum_base;
	uint8_t intnum_count;
	uint8_t disk_base;
	uint8_t disk_count;
} ahci_hba_t;


int8_t ahci_init(memory_heap_t* heap, linkedlist_t sata_pci_devices);
int8_t ahci_identify(uint64_t disk_id);
future_t ahci_read(uint64_t disk_id, uint64_t lba, uint16_t size, uint8_t* buffer);
future_t ahci_write(uint64_t disk_id, uint64_t lba, uint16_t size, uint8_t* buffer);
future_t ahci_flush(uint64_t disk_id);

ahci_sata_disk_t* ahci_get_disk_by_id(uint64_t disk_id);
disk_t* ahci_disk_impl_open(ahci_sata_disk_t* sata_disk);

#endif
