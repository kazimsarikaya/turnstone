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

int8_t hypervisor_iommu_init(void);


#ifdef __cplusplus
}
#endif

#endif
