/**
 * @file hypervisor_iommu.h
 * @brief defines hypervisor iommu related macros
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_IOMMU_H
#define ___HYPERVISOR_IOMMU_H 0

#include <types.h>
#include <pci.h>
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef union ivrs_ivinfo_t {
    struct {
        uint32_t efr_support      :1;
        uint32_t dma_remap_support:1;
        uint32_t reserved0        :3;
        uint32_t gva_size         :3;
        uint32_t pa_size          :7;
        uint32_t va_size          :7;
        uint32_t ht_ats_reserved  :1;
        uint32_t reserved1        :9;
    } __attribute__((packed)) fields;
    uint32_t bits;
}__attribute__((packed)) ivrs_ivinfo_t;

typedef union ivrs_bdf_t {
    struct {
        uint16_t function:3;
        uint16_t device  :5;
        uint16_t bus     :8;
    } __attribute__((packed)) fields;
    uint16_t bits;
} __attribute__((packed)) ivrs_bdf_t;

typedef struct ivrs_ivhd_type_10_t {
    uint64_t type :8;
    union {
        struct {
            uint8_t httunen  :1;
            uint8_t passpw   :1;
            uint8_t respasspw:1;
            uint8_t isoc     :1;
            uint8_t iotlbsup :1;
            uint8_t coherent :1;
            uint8_t prefsup  :1;
            uint8_t pprsup   :1;
        }__attribute__((packed)) fields;
        uint8_t bits;
    }__attribute__((packed)) flags;
    uint64_t   length :16;
    ivrs_bdf_t device_id;
    uint64_t   capability_offset :16;
    uint64_t   iommu_base_addr   :64;
    uint64_t   pci_segment_group :16;
    union {
        struct {
            uint8_t msinum    :5;
            uint8_t reserved0 :3;
            uint8_t unit_id   :5;
            uint8_t reserved1 :3;
        }__attribute__((packed)) fields;
        uint8_t bits;
    }__attribute__((packed)) iommu_info;
    union {
        struct {
            uint32_t xtsup     :1;
            uint32_t nxsup     :1;
            uint32_t gtsup     :1;
            uint32_t glxsup    :2;
            uint32_t iasup     :1;
            uint32_t gasup     :1;
            uint32_t hesup     :1;
            uint32_t pasmax    :5;
            uint32_t pncounters:4;
            uint32_t pnbanks   :6;
            uint32_t msinumppr :5;
            uint32_t gats      :2;
            uint32_t hats      :2;
        }__attribute__((packed)) fields;
        uint32_t bits;
    }__attribute__((packed)) iommu_feature_info;
    uint8_t devices_infos[];
} __attribute__((packed)) ivrs_ivhd_type_10_t;

_Static_assert(sizeof(ivrs_ivhd_type_10_t) == 24, "ivrs_ivhd_type_type_10 size is not correct");

typedef struct ivrs_ivhd_type_11_t {
    uint64_t type :8;
    union {
        struct {
            uint8_t httunen   :1;
            uint8_t passpw    :1;
            uint8_t respasspw :1;
            uint8_t isoc      :1;
            uint8_t iotlbsup  :1;
            uint8_t coherent  :1;
            uint8_t reserved0 :2;
        }__attribute__((packed)) fields;
        uint8_t bits;
    }__attribute__((packed)) flags;
    uint64_t   length :16;
    ivrs_bdf_t device_id;
    uint64_t   capability_offset :16;
    uint64_t   iommu_base_addr   :64;
    uint64_t   pci_segment_group :16;
    union {
        struct {
            uint8_t msinum    :5;
            uint8_t reserved0 :3;
            uint8_t unit_id   :5;
            uint8_t reserved1 :3;
        }__attribute__((packed)) fields;
        uint8_t bits;
    }__attribute__((packed)) iommu_info;
    union {
        struct {
            uint32_t hatdis     :1;
            uint32_t reserved0  :12;
            uint32_t pncounters :4;
            uint32_t pnbanks    :6;
            uint32_t msinumppr  :5;
            uint32_t reserved1  :4;
        }__attribute__((packed)) fields;
        uint32_t bits;
    }__attribute__((packed)) iommu_feature_info;
    uint64_t iommu_efr_image  :64;
    uint64_t iommu_efr_image2 :64;
    uint8_t  devices_infos[];
} __attribute__((packed)) ivrs_ivhd_type_11_t;

_Static_assert(sizeof(ivrs_ivhd_type_11_t) == 40, "ivrs_ivhd_type_type_11 size is not correct");

typedef union ivrs_ivhd_t {
    struct {
        uint64_t type      :8;
        uint64_t reserved0 :8;
        uint64_t length    :16;
        uint64_t reserved1 :32;
    } __attribute__((packed)) type_length;

    ivrs_ivhd_type_10_t type_10;
    ivrs_ivhd_type_11_t type_11;

    struct {
        uint64_t type              :8;
        uint64_t flags             :8;
        uint64_t length            :16;
        uint64_t device_id         :16;
        uint64_t capability_offset :16;
        uint64_t iommu_base_addr   :64;
        uint64_t pci_segment_group :16;
        uint64_t iommu_info        :16;
        uint64_t iommu_feature_info:32;
        uint64_t iommu_efr_image   :64;
        uint64_t reserved0         :64;
        uint8_t  devices_infos[];
    } __attribute__((packed)) type_40;


} ivrs_ivhd_t;

typedef struct ivrs_4byte_device_info_t {
    uint8_t    type;
    ivrs_bdf_t bdf;
    uint8_t    dte;
} __attribute__((packed)) ivrs_4byte_device_info_t;

_Static_assert(sizeof(ivrs_4byte_device_info_t) == 4, "ivrs_4byte_device_info_t size is not correct");

typedef struct ivrs_8byte_device_info_72_t {
    uint8_t    type;
    uint16_t   reserved0;
    uint8_t    dte;
    uint8_t    handle;
    ivrs_bdf_t bdf;
    uint8_t    variety;
} __attribute__((packed)) ivrs_8byte_device_info_72_t;

_Static_assert(sizeof(ivrs_8byte_device_info_72_t) == 8, "ivrs_8byte_device_info_72_t size is not correct");

typedef struct ivrs_vbyte_device_info_f0_t {
    uint8_t    type;
    ivrs_bdf_t bdf;
    uint8_t    dte;
    uint64_t   hid;
    uint64_t   cid;
    uint8_t    uid_format;
    uint8_t    uid_length;
    uint8_t    uid[];
} __attribute__((packed)) ivrs_vbyte_device_info_f0_t;

_Static_assert(sizeof(ivrs_vbyte_device_info_f0_t) == 22, "ivrs_vbyte_device_info_f0_t size is not correct");

typedef struct amdvi_pci_capability_t {
    union {
        struct {
            pci_capability_t cap;
            uint32_t         cap_type    :3;
            uint32_t         revision    :5;
            uint32_t         iotlbsup    :1;
            uint32_t         httunnel    :1;
            uint32_t         npcache     :1;
            uint32_t         efr_support :1;
            uint32_t         capext      :1;
            uint32_t         reserved0   :3;
        } __attribute__((packed)) fields;
        uint32_t bits;
    }__attribute__((packed)) header;
    union {
        struct {
            uint32_t enable       :1;
            uint32_t reserved1    :13;
            uint32_t base_address :18;
        }__attribute__((packed)) fields;
        uint32_t bits;
    }__attribute__((packed)) base_address_low;
    uint32_t base_address_high  :32;
    uint32_t unit_id            :5;
    uint32_t reserved2          :2;
    uint32_t rng_valid          :1;
    uint32_t bus_number         :8;
    uint32_t first_device_number:8;
    uint32_t last_device_number :8;
    uint32_t msinum             :5;
    uint32_t gva_size           :3;
    uint32_t pa_size            :7;
    uint32_t va_size            :7;
    uint32_t ht_ats_reserved    :1;
    uint32_t reserved3          :4;
    uint32_t msinum_ppr         :5;
    uint32_t msinum_ga          :5;
    uint32_t reserved4          :27;
} __attribute__((packed)) amdvi_pci_capability_t;

_Static_assert(sizeof(amdvi_pci_capability_t) == sizeof(uint32_t) * 6, "amdvi_pci_capability_t size is not correct");


#define AMDVI_REG_DEVICE_TABLE_BASE_BASE_ADDRESS          0x0000
#define AMDVI_REG_COMMAND_BUFFER_BASE_ADDRESS             0x0008
#define AMDVI_REG_EVENT_LOG_BASE_ADDRESS                  0x0010
#define AMDVI_REG_CONTROL                                 0x0018
#define AMDVI_REG_EXCLUSION_BASE_ADDRESS                  0x0020
#define AMDVI_REG_EXCLUSION_RANGE_LIMIT_ADDRESS           0x0028
#define AMDVI_REG_EXTENDED_FEATURES                       0x0030
#define AMDVI_REG_PPR_LOG_BASE_ADDRESS                    0x0038
#define AMDVI_REG_HARDWARE_EVENT_UPPER                    0x0040
#define AMDVI_REG_HARDWARE_EVENT_LOWER                    0x0048
#define AMDVI_REG_HARDWARE_EVENT_STATUS                   0x0050

#define AMDVI_REG_SMI_FILTER_00                           0x0060
#define AMDVI_REG_SMI_FILTER_01                           0x0068
#define AMDVI_REG_SMI_FILTER_02                           0x0070
#define AMDVI_REG_SMI_FILTER_03                           0x0078
#define AMDVI_REG_SMI_FILTER_04                           0x0080
#define AMDVI_REG_SMI_FILTER_05                           0x0088
#define AMDVI_REG_SMI_FILTER_06                           0x0090
#define AMDVI_REG_SMI_FILTER_07                           0x0098
#define AMDVI_REG_SMI_FILTER_08                           0x00A0
#define AMDVI_REG_SMI_FILTER_09                           0x00A8
#define AMDVI_REG_SMI_FILTER_10                           0x00B0
#define AMDVI_REG_SMI_FILTER_11                           0x00B8
#define AMDVI_REG_SMI_FILTER_12                           0x00C0
#define AMDVI_REG_SMI_FILTER_13                           0x00C8
#define AMDVI_REG_SMI_FILTER_14                           0x00D0
#define AMDVI_REG_SMI_FILTER_15                           0x00D8

#define AMDVI_REG_GUEST_VIRTUAL_APIC_LOG_BASE_ADDRESS     0x00E0
#define AMDVI_REG_GUEST_VIRTUAL_APIC_LOG_TAIL_ADDRESS     0x00E8

#define AMDVI_REG_COMMAND_BUFFER_HEAD                     0x2000
#define AMDVI_REG_COMMAND_BUFFER_TAIL                     0x2008
#define AMDVI_REG_EVENT_LOG_HEAD                          0x2010
#define AMDVI_REG_EVENT_LOG_TAIL                          0x2018
#define AMDVI_REG_STATUS                                  0x2020
#define AMDVI_REG_PPR_LOG_HEAD                            0x2030
#define AMDVI_REG_PPR_LOG_TAIL                            0x2038
#define AMDVI_REG_GA_LOG_HEAD                             0x2040
#define AMDVI_REG_GA_LOG_TAIL                             0x2048

#define AMDVI_REG_TOTAL_SIZE                              0x4000


typedef union amdvi_device_table_base_t {
    struct {
        uint64_t size        :9;
        uint64_t reserved0   :3;
        uint64_t base_address:40;
        uint64_t reserved1   :12;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) amdvi_device_table_base_t;

_Static_assert(sizeof(amdvi_device_table_base_t) == sizeof(uint64_t), "amdvi_device_table_base_t size is not correct");

typedef union amdvi_command_buffer_base_t {
    struct {
        uint64_t reserved0   :12;
        uint64_t base_address:40;
        uint64_t reserved1   :4;
        uint64_t size        :4;
        uint64_t reserved2   :4;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) amdvi_command_buffer_base_t;

_Static_assert(sizeof(amdvi_command_buffer_base_t) == sizeof(uint64_t), "amdvi_command_buffer_base_t size is not correct");

typedef amdvi_command_buffer_base_t amdvi_event_log_base_t;
typedef amdvi_command_buffer_base_t amdvi_ppr_log_base_t;
typedef amdvi_command_buffer_base_t amdvi_ga_log_base_t;

typedef union amdvi_control_t {
    struct {
        uint64_t iommu_en             :1; ///< 0
        uint64_t ht_tun_en            :1; ///< 1
        uint64_t event_log_en         :1; ///< 2
        uint64_t event_int_en         :1; ///< 3
        uint64_t com_wait_int_en      :1; ///< 4
        uint64_t inv_time_out         :3; ///< 5-7
        uint64_t pass_pw              :1; ///< 8
        uint64_t res_pass_pw          :1; ///< 9
        uint64_t coherent             :1; ///< 10
        uint64_t isoc                 :1; ///< 11
        uint64_t cmd_buf_en           :1; ///< 12
        uint64_t ppr_log_en           :1; ///< 13
        uint64_t ppr_int_en           :1; ///< 14
        uint64_t ppr_en               :1; ///< 15
        uint64_t gt_en                :1; ///< 16
        uint64_t ga_en                :1; ///< 17
        uint64_t crw                  :4; ///< 18-21
        uint64_t smi_f_en             :1; ///< 22
        uint64_t self_wb_dis          :1; ///< 23
        uint64_t smi_f_log_en         :1; ///< 24
        uint64_t ga_mode_en           :3; ///< 25-27
        uint64_t ga_log_en            :1; ///< 28
        uint64_t ga_int_en            :1; ///< 29
        uint64_t dual_ppr_log_en      :2; ///< 30-31
        uint64_t dual_event_log_en    :2; ///< 32-33
        uint64_t dev_table_seg_en     :3; ///< 34-36
        uint64_t priv_abrt_en         :2; ///< 37-38
        uint64_t ppr_auto_rsp_en      :1; ///< 39
        uint64_t marc_en              :1; ///< 40
        uint64_t blk_stop_mark_en     :1; ///< 41
        uint64_t ppr_auto_rsp_a_on    :1; ///< 42
        uint64_t num_int_remap_mode   :2; ///< 43-44
        uint64_t eph_en               :1; ///< 45
        uint64_t had_update           :2; ///< 46-47
        uint64_t gd_update_dis        :1; ///< 48
        uint64_t reserved0            :1; ///< 49
        uint64_t xt_en                :1; ///< 50
        uint64_t int_cap_xt_en        :1; ///< 51
        uint64_t v_cmd_en             :1; ///< 52
        uint64_t v_iommu_en           :1; ///< 53
        uint64_t ga_update_dis        :1; ///< 54
        uint64_t ga_ppi_en            :1; ///< 55
        uint64_t tmpm_en              :1; ///< 56
        uint64_t reserved1            :1; ///< 57
        uint64_t gcr3_trp_mode        :1; ///< 58
        uint64_t irt_cache_dis        :1; ///< 59
        uint64_t guest_buffer_trp_mode:1; ///< 60
        uint64_t snp_avic_en          :3; ///< 61-63
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) amdvi_control_t;

_Static_assert(sizeof(amdvi_control_t) == sizeof(uint64_t), "amdvi_control_t size is not correct");

typedef union amdvi_extended_feature_t {
    struct {
        uint64_t pref_sup              :1; ///< 0
        uint64_t ppr_sup               :1; ///< 1
        uint64_t xt_sup                :1; ///< 2
        uint64_t nx_sup                :1; ///< 3
        uint64_t gt_sup                :1; ///< 4
        uint64_t gappi_sup             :1; ///< 5
        uint64_t ia_sup                :1; ///< 6
        uint64_t ga_sup                :1; ///< 7
        uint64_t he_sup                :1; ///< 8
        uint64_t pc_sup                :1; ///< 9
        uint64_t hats                  :2; ///< 10-11
        uint64_t gats                  :2; ///< 12-13
        uint64_t glx_sup               :2; ///< 14-15
        uint64_t smi_f_sup             :2; ///< 16-17
        uint64_t smi_frc               :3; ///< 18-20
        uint64_t gam_sup               :3; ///< 21-23
        uint64_t dual_ppr_log_sup      :2; ///< 24-25
        uint64_t reserved0             :2; ///< 26-27
        uint64_t dual_event_log_sup    :2; ///< 28-29
        uint64_t reserved1             :1; ///< 30
        uint64_t sats_sup              :1; ///< 31
        uint64_t pas_max               :5; ///< 32-36
        uint64_t us_suo                :1; ///< 37
        uint64_t dev_table_seg_sup     :2; ///< 38-39
        uint64_t ppr_overflow_early_sup:1; ///< 40
        uint64_t ppr_auto_rsp_sup      :1; ///< 41
        uint64_t marc_sup              :2; ///< 42-43
        uint64_t blk_stop_mark_sup     :1; ///< 44
        uint64_t perf_opt_sup          :1; ///< 45
        uint64_t msi_cap_mmio_sup      :1; ///< 46
        uint64_t reserved2             :1; ///< 47
        uint64_t gio_sup               :1; ///< 48
        uint64_t ha_sup                :1; ///< 49
        uint64_t eph_sup               :1; ///< 50
        uint64_t attr_forward_sup      :1; ///< 51
        uint64_t hd_sup                :1; ///< 52
        uint64_t reserved3             :1; ///< 53
        uint64_t inv_iotbl_type_sup    :1; ///< 54
        uint64_t v_iommu_sup           :1; ///< 55
        uint64_t reserved4             :5; ///< 56-60
        uint64_t ga_update_dis_sup     :1; ///< 61
        uint64_t force_phy_dest_sup    :1; ///< 62
        uint64_t snp_sup               :1; ///< 63
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) amdvi_extended_feature_t;

_Static_assert(sizeof(amdvi_extended_feature_t) == sizeof(uint64_t), "amdvi_extended_feature_t size is not correct");

int8_t hypervisor_iommu_init(void);


#ifdef __cplusplus
}
#endif

#endif
