/**
 * @file hypervisor_svm_vmcb_ops.h
 * @brief defines hypervisor svm (amd) related vmcb operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_SVM_VMCB_OPS_H
#define ___HYPERVISOR_SVM_VMCB_OPS_H 0

#include <types.h>
#include <utils.h>
#include <hypervisor/hypervisor_vm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union svm_vmcb_intercept_crx_t {
    struct {
        uint32_t read_cr0  :1;
        uint32_t read_cr1  :1;
        uint32_t read_cr2  :1;
        uint32_t read_cr3  :1;
        uint32_t read_cr4  :1;
        uint32_t read_cr5  :1;
        uint32_t read_cr6  :1;
        uint32_t read_cr7  :1;
        uint32_t read_cr8  :1;
        uint32_t read_cr9  :1;
        uint32_t read_cr10 :1;
        uint32_t read_cr11 :1;
        uint32_t read_cr12 :1;
        uint32_t read_cr13 :1;
        uint32_t read_cr14 :1;
        uint32_t read_cr15 :1;
        uint32_t write_cr0 :1;
        uint32_t write_cr1 :1;
        uint32_t write_cr2 :1;
        uint32_t write_cr3 :1;
        uint32_t write_cr4 :1;
        uint32_t write_cr5 :1;
        uint32_t write_cr6 :1;
        uint32_t write_cr7 :1;
        uint32_t write_cr8 :1;
        uint32_t write_cr9 :1;
        uint32_t write_cr10:1;
        uint32_t write_cr11:1;
        uint32_t write_cr12:1;
        uint32_t write_cr13:1;
        uint32_t write_cr14:1;
        uint32_t write_cr15:1;
    } __attribute__((packed)) fields;
    uint32_t bits;
} __attribute__((packed)) svm_vmcb_intercept_crx_t;

_Static_assert(sizeof(svm_vmcb_intercept_crx_t) == sizeof(uint32_t), "svm_vmcb_intercept_crx_t size is not 4 bytes");

typedef union svm_vmcb_intercept_drx_t {
    struct {
        uint32_t read_dr0  :1;
        uint32_t read_dr1  :1;
        uint32_t read_dr2  :1;
        uint32_t read_dr3  :1;
        uint32_t read_dr4  :1;
        uint32_t read_dr5  :1;
        uint32_t read_dr6  :1;
        uint32_t read_dr7  :1;
        uint32_t read_dr8  :1;
        uint32_t read_dr9  :1;
        uint32_t read_dr10 :1;
        uint32_t read_dr11 :1;
        uint32_t read_dr12 :1;
        uint32_t read_dr13 :1;
        uint32_t read_dr14 :1;
        uint32_t read_dr15 :1;
        uint32_t write_dr0 :1;
        uint32_t write_dr1 :1;
        uint32_t write_dr2 :1;
        uint32_t write_dr3 :1;
        uint32_t write_dr4 :1;
        uint32_t write_dr5 :1;
        uint32_t write_dr6 :1;
        uint32_t write_dr7 :1;
        uint32_t write_dr8 :1;
        uint32_t write_dr9 :1;
        uint32_t write_dr10:1;
        uint32_t write_dr11:1;
        uint32_t write_dr12:1;
        uint32_t write_dr13:1;
        uint32_t write_dr14:1;
        uint32_t write_dr15:1;
    } __attribute__((packed)) fields;
    uint32_t bits;
} __attribute__((packed)) svm_vmcb_intercept_drx_t;

_Static_assert(sizeof(svm_vmcb_intercept_drx_t) == sizeof(uint32_t), "svm_vmcb_intercept_drx_t size is not 4 bytes");

typedef union svm_vmcb_intercept_control_1_t {
    struct {
        uint32_t intr       :1;
        uint32_t nmi        :1;
        uint32_t smi        :1;
        uint32_t init       :1;
        uint32_t vinit      :1;
        uint32_t cr0_write  :1;
        uint32_t read_idtr  :1;
        uint32_t read_gdtr  :1;
        uint32_t read_ldtr  :1;
        uint32_t read_tr    :1;
        uint32_t write_idtr :1;
        uint32_t write_gdtr :1;
        uint32_t write_ldtr :1;
        uint32_t write_tr   :1;
        uint32_t rdtsc      :1;
        uint32_t rdpmc      :1;
        uint32_t pushf      :1;
        uint32_t popf       :1;
        uint32_t cpuid      :1;
        uint32_t rsm        :1;
        uint32_t iret       :1;
        uint32_t intn       :1;
        uint32_t invd       :1;
        uint32_t pause      :1;
        uint32_t hlt        :1;
        uint32_t invlpg     :1;
        uint32_t invlpga    :1;
        uint32_t io         :1;
        uint32_t msr        :1;
        uint32_t task_switch:1;
        uint32_t ferr_freeze:1;
        uint32_t shutdown   :1;
    } __attribute__((packed)) fields;
    uint32_t bits;
} __attribute__((packed)) svm_vmcb_intercept_control_1_t;

_Static_assert(sizeof(svm_vmcb_intercept_control_1_t) == sizeof(uint32_t), "svm_vmcb_intercept_control_1_t size is not 4 bytes");

typedef union svm_vmcb_intercept_control_2_t {
    struct {
        uint32_t vmrun              :1;
        uint32_t vmmcall            :1;
        uint32_t vmload             :1;
        uint32_t vmsave             :1;
        uint32_t stgi               :1;
        uint32_t clgi               :1;
        uint32_t skinit             :1;
        uint32_t rdtscp             :1;
        uint32_t icebp              :1;
        uint32_t wbinvd             :1;
        uint32_t monitor            :1;
        uint32_t mwait_unconditional:1;
        uint32_t mwait_hw_armed     :1;
        uint32_t xsetbv             :1;
        uint32_t rdpru              :1;
        uint32_t efer_write_after   :1;
        uint32_t cr0_write_after    :1;
        uint32_t cr1_write_after    :1;
        uint32_t cr2_write_after    :1;
        uint32_t cr3_write_after    :1;
        uint32_t cr4_write_after    :1;
        uint32_t cr5_write_after    :1;
        uint32_t cr6_write_after    :1;
        uint32_t cr7_write_after    :1;
        uint32_t cr8_write_after    :1;
        uint32_t cr9_write_after    :1;
        uint32_t cr10_write_after   :1;
        uint32_t cr11_write_after   :1;
        uint32_t cr12_write_after   :1;
        uint32_t cr13_write_after   :1;
        uint32_t cr14_write_after   :1;
        uint32_t cr15_write_after   :1;
    } __attribute__((packed)) fields;
    uint32_t bits;
} __attribute__((packed)) svm_vmcb_intercept_control_2_t;

_Static_assert(sizeof(svm_vmcb_intercept_control_2_t) == sizeof(uint32_t), "svm_vmcb_intercept_control_2_t size is not 4 bytes");

typedef union svm_vmcb_intercept_control_3_t {
    struct {
        uint32_t invlpgb        :1;
        uint32_t invlpgb_illegal:1;
        uint32_t invpcid        :1;
        uint32_t mcommit        :1;
        uint32_t tlb_sync       :1;
        uint32_t bus_lock       :1;
        uint32_t idle_hlt       :1;
        uint32_t reserved       :25;
    } __attribute__((packed)) fields;
    uint32_t bits;
} __attribute__((packed)) svm_vmcb_intercept_control_3_t;

_Static_assert(sizeof(svm_vmcb_intercept_control_3_t) == sizeof(uint32_t), "svm_vmcb_intercept_control_3_t size is not 4 bytes");

typedef struct svm_vmcb_pause_control_t {
    uint32_t threshold :16;
    uint32_t count     :16;
} __attribute__((packed)) svm_vmcb_pause_control_t;

_Static_assert(sizeof(svm_vmcb_pause_control_t) == sizeof(uint32_t), "svm_vmcb_pause_control_t size is not 4 bytes");

typedef union svm_vmcb_guest_asid_t {
    struct {
        uint64_t asid       :32;
        uint64_t tlb_control:8;
        uint64_t reserved   :24;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_guest_asid_t;

_Static_assert(sizeof(svm_vmcb_guest_asid_t) == sizeof(uint64_t), "svm_vmcb_guest_asid_t size is not 8 bytes");

typedef union svm_vmcb_vint_control_t {
    struct {
        uint64_t v_tpr        :8;
        uint64_t v_irq        :1;
        uint64_t v_gif        :1;
        uint64_t v_nmi        :1;
        uint64_t v_nmi_mask   :1;
        uint64_t reserved_sbz0:3;
        uint64_t v_intr_prio  :4;
        uint64_t v_ign_tpr    :1;
        uint64_t reserved_sbz1:3;
        uint64_t v_intr_mask  :1;
        uint64_t v_gif_enabled:1;
        uint64_t v_nmi_enabled:1;
        uint64_t reserved_sbz2:3;
        uint64_t vapic_x2apic :1;
        uint64_t vapic_apic   :1;
        uint64_t v_intr_vector:8;
        uint64_t reserved_sbz3:24;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_vint_control_t;

_Static_assert(sizeof(svm_vmcb_vint_control_t) == sizeof(uint64_t), "svm_vmcb_vint_control_t size is not 8 bytes");

typedef union svm_vmcb_interrupt_shadow_t {
    struct {
        uint64_t int_shadow     :1;
        uint64_t guest_int_mask :1;
        uint64_t reserved       :62;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_interrupt_shadow_t;

_Static_assert(sizeof(svm_vmcb_interrupt_shadow_t) == sizeof(uint64_t), "svm_vmcb_interrupt_shadow_t size is not 8 bytes");

typedef union svm_vmcb_nested_page_control_t {
    struct {
        uint64_t np_enable  :1;
        uint64_t reserved1  :11;
        uint64_t np_base_pa :40;
        uint64_t reserved2  :12;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_nested_page_control_t;

_Static_assert(sizeof(svm_vmcb_nested_page_control_t) == sizeof(uint64_t), "svm_vmcb_nested_page_control_t size is not 8 bytes");

typedef union svm_vmcb_lbr_virtualization_t {
    struct {
        uint64_t enabled             :1;
        uint64_t vmsave_vmload_enable:1;
        uint64_t sampling_enable     :1;
        uint64_t reserved1           :61;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_lbr_virtualization_t;

_Static_assert(sizeof(svm_vmcb_lbr_virtualization_t) == sizeof(uint64_t), "svm_vmcb_lbr_virtualization_t size is not 8 bytes");

typedef union svm_vmcb_clean_bits_t {
    struct {
        uint64_t i       :1;
        uint64_t iopm    :1;
        uint64_t asid    :1;
        uint64_t tpr     :1;
        uint64_t np      :1;
        uint64_t crx     :1;
        uint64_t drx     :1;
        uint64_t dt      :1;
        uint64_t seg     :1;
        uint64_t cr2     :1;
        uint64_t lbr     :1;
        uint64_t avic    :1;
        uint64_t cet     :1;
        uint64_t reserved:51;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_clean_bits_t;

_Static_assert(sizeof(svm_vmcb_clean_bits_t) == sizeof(uint64_t), "svm_vmcb_clean_bits_t size is not 8 bytes");

typedef struct svm_vmcb_guest_fetched_instructions_t {
    uint8_t length;
    uint8_t bytes[15];
} __attribute__((packed)) svm_vmcb_guest_fetched_instructions_t;

_Static_assert(sizeof(svm_vmcb_guest_fetched_instructions_t) == 16, "svm_vmcb_guest_fetched_instructions_t size is not 16 bytes");

typedef union svm_vmcb_avic_physical_table_t {
    struct {
        uint64_t max_index       :8;
        uint64_t reserved1       :4;
        uint64_t physical_address:40;
        uint64_t reserved2       :12;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_avic_physical_table_t;

_Static_assert(sizeof(svm_vmcb_avic_physical_table_t) == sizeof(uint64_t), "svm_vmcb_avic_physical_table_t size is not 8 bytes");


typedef union svm_vmcb_bus_lock_threshold_t {
    struct {
        uint32_t threshold:16;
        uint32_t reserved1:16;
        uint32_t reserved2;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_vmcb_bus_lock_threshold_t;

_Static_assert(sizeof(svm_vmcb_bus_lock_threshold_t) == sizeof(uint64_t), "svm_vmcb_bus_lock_threshold_t size is not 8 bytes");

typedef enum svm_exit_int_info_type_t {
    SVM_EXIT_INFO_TYPE_INTR,
    SVM_EXIT_INFO_TYPE_NMI,
    SVM_EXIT_INFO_TYPE_FAULT_OR_TRAP,
    SVM_EXIT_INFO_TYPE_INTn,
} svm_exit_int_info_type_t;

typedef union svm_exit_int_info_t {
    struct {
        uint64_t vector           :8;
        uint64_t type             :3;
        uint64_t error_code_valid :1;
        uint64_t reserved         :19;
        uint64_t valid            :1;
        uint64_t error_code       :32;
    } __attribute__((packed)) fields;
    uint64_t bits;
} __attribute__((packed)) svm_exit_int_info_t;

typedef svm_exit_int_info_t svm_event_injection_t;

_Static_assert(sizeof(svm_exit_int_info_t) == sizeof(uint64_t), "svm_exit_int_info_t size is not 8 bytes");

typedef struct svm_vmcb_control_area_t {
    svm_vmcb_intercept_crx_t              intercept_crx;
    svm_vmcb_intercept_drx_t              intercept_drx;
    svm_vmcb_intercept_control_1_t        intercept_control_1;
    svm_vmcb_intercept_control_2_t        intercept_control_2;
    svm_vmcb_intercept_control_3_t        intercept_control_3;
    uint8_t                               reserved_sbz0[40];
    svm_vmcb_pause_control_t              pause_control;
    uint64_t                              iopm_base_pa;
    uint64_t                              msrpm_base_pa;
    uint64_t                              tsc_offset;
    svm_vmcb_guest_asid_t                 guest_asid;
    svm_vmcb_vint_control_t               vint_control;
    svm_vmcb_interrupt_shadow_t           interrupt_shadow;
    uint64_t                              exit_code;
    uint64_t                              exit_info_1;
    uint64_t                              exit_info_2;
    svm_exit_int_info_t                   exit_int_info;
    svm_vmcb_nested_page_control_t        nested_page_control;
    uint64_t                              avic_apic_bar;
    uint64_t                              ghcb_physical_address;
    svm_event_injection_t                 event_injection;
    uint64_t                              n_cr3;
    svm_vmcb_lbr_virtualization_t         lbr_virtualization;
    svm_vmcb_clean_bits_t                 clean_bits;
    uint64_t                              n_rip;
    svm_vmcb_guest_fetched_instructions_t guest_fetched_instructions;
    uint64_t                              avic_apic_backing_page_pointer;
    uint8_t                               reserved_sbz1[8];
    uint64_t                              avic_logical_table_pointer;
    svm_vmcb_avic_physical_table_t        avic_physical_table;
    uint8_t                               reserved_sbz2[8];
    uint64_t                              vmsa_pointer;
    uint64_t                              vmexit_rax;
    uint64_t                              vmexit_cpl;
    svm_vmcb_bus_lock_threshold_t         bus_lock_threshold;
    uint8_t                               reserved_sbz3[12];
    uint32_t                              update_irr;
    uint64_t                              guest_sev_mask;
    uint64_t                              guest_sev_features;
    uint8_t                               reserved_sbz4[8];
    uint64_t                              requested_irr[4];
    uint8_t                               reserved_sbz5[624];
    uint8_t                               reserved_for_host[32];
} __attribute__((packed)) svm_vmcb_control_area_t;


_Static_assert(offsetof_field(svm_vmcb_control_area_t, pause_control) == 0x3c, "svm_vmcb_control_area_t pause_control offset is not at 0x3c bytes");
_Static_assert(offsetof_field(svm_vmcb_control_area_t, n_cr3) == 0xb0, "svm_vmcb_control_area_t n_cr3 offset is not at 0xb0 bytes");
_Static_assert(offsetof_field(svm_vmcb_control_area_t, avic_logical_table_pointer) == 0xf0, "svm_vmcb_control_area_t avic_apic_backing_page_pointer offset is not at 0xe0 bytes");
_Static_assert(offsetof_field(svm_vmcb_control_area_t, vmsa_pointer) == 0x108, "svm_vmcb_control_area_t vmsa_pointer offset is not at 0x108 bytes");
_Static_assert(offsetof_field(svm_vmcb_control_area_t, requested_irr) == 0x150, "svm_vmcb_control_area_t reserved_sbz offset is not at 0x150 bytes");
_Static_assert(sizeof(svm_vmcb_control_area_t) == 0x400, "svm_vmcb_control_area_t size is not 0x400 bytes");

typedef struct svm_vmcb_segdesc_t {
    uint16_t selector;
    uint16_t attrib;
    uint32_t limit;
    uint64_t base;
} __attribute__((packed)) svm_vmcb_segdesc_t;

_Static_assert(sizeof(svm_vmcb_segdesc_t) == 16, "svm_vmcb_segdesc_t size is not 16 bytes");


typedef struct svm_vmcb_save_state_area_t {
    svm_vmcb_segdesc_t es;
    svm_vmcb_segdesc_t cs;
    svm_vmcb_segdesc_t ss;
    svm_vmcb_segdesc_t ds;
    svm_vmcb_segdesc_t fs;
    svm_vmcb_segdesc_t gs;
    svm_vmcb_segdesc_t gdtr;
    svm_vmcb_segdesc_t ldtr;
    svm_vmcb_segdesc_t idtr;
    svm_vmcb_segdesc_t tr;
    uint8_t            reserved_sbz0[43];
    uint8_t            cpl;
    uint8_t            reserved_sbz1[4];
    uint64_t           efer;
    uint8_t            reserved_sbz2[8];
    uint64_t           perf_ctl0;
    uint64_t           perf_ctr0;
    uint64_t           perf_ctl1;
    uint64_t           perf_ctr1;
    uint64_t           perf_ctl2;
    uint64_t           perf_ctr2;
    uint64_t           perf_ctl3;
    uint64_t           perf_ctr3;
    uint64_t           perf_ctl4;
    uint64_t           perf_ctr4;
    uint64_t           perf_ctl5;
    uint64_t           perf_ctr5;
    uint8_t            reserved_sbz3[8];
    uint64_t           cr4;
    uint64_t           cr3;
    uint64_t           cr0;
    uint64_t           dr7;
    uint64_t           dr6;
    uint64_t           rflags;
    uint64_t           rip;
    uint8_t            reserved_sbz4[64];
    uint64_t           instr_retired_ctr;
    uint64_t           perf_ctr_global_sts;
    uint64_t           perf_ctr_global_ctrl;
    uint64_t           rsp;
    uint64_t           s_cet;
    uint64_t           ssp;
    uint64_t           isst_addr;
    uint64_t           rax;
    uint64_t           star;
    uint64_t           lstar;
    uint64_t           cstar;
    uint64_t           sfmask;
    uint64_t           kernel_gs_base;
    uint64_t           sysenter_cs;
    uint64_t           sysenter_esp;
    uint64_t           sysenter_eip;
    uint64_t           cr2;
    uint8_t            reserved_sbz5[32];
    uint64_t           g_pat;
    uint64_t           dbgctl;
    uint64_t           br_from;
    uint64_t           br_to;
    uint64_t           last_excp_from;
    uint64_t           last_excp_to;
    uint64_t           debug_extn_ctl;
    uint8_t            reserved_sbz6[64];
    uint64_t           spec_ctrl;
    uint8_t            reserved_sbz7[904];
    uint8_t            lbr_stack_from_to[256];
    uint64_t           lbr_select;
    uint64_t           ibs_fetch_ctl;
    uint64_t           ibs_fetch_lin_ad;
    uint64_t           ibs_op_ctl;
    uint64_t           ibs_op_rip;
    uint64_t           ibs_op_data1;
    uint64_t           ibs_op_data2;
    uint64_t           ibs_op_data3;
    uint64_t           ibs_dc_lin_ad;
    uint64_t           bp_ibstgt_rip;
    uint64_t           ic_ibs_extd_ctl;
} __attribute__((packed)) svm_vmcb_save_state_area_t;


_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, cpl) == 0xcb, "svm_vmcb_save_state_area_t cpl offset is not at 0xcb bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, efer) == 0xd0, "svm_vmcb_save_state_area_t efer offset is not at 0x80 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, perf_ctl0) == 0xe0, "svm_vmcb_save_state_area_t perf_ctl0 offset is not at 0xe0 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, cr0) == 0x158, "svm_vmcb_save_state_area_t cr0 offset is not at 0x158 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, instr_retired_ctr) == 0x1c0, "svm_vmcb_save_state_area_t instr_retired_ctr offset is not at 0x1c0 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, rsp) == 0x1d8, "svm_vmcb_save_state_area_t rsp offset is not at 0x1d8 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, g_pat) == 0x268, "svm_vmcb_save_state_area_t g_pat offset is not at 0x268 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, spec_ctrl) == 0x2e0, "svm_vmcb_save_state_area_t spec_ctrl offset is not at 0x2e0 bytes");
_Static_assert(offsetof_field(svm_vmcb_save_state_area_t, lbr_stack_from_to) == 0x670, "svm_vmcb_save_state_area_t lbr_stack_from_to offset is not at 0x670 bytes");
_Static_assert(sizeof(svm_vmcb_save_state_area_t) == 0x7c8, "svm_vmcb_save_state_area_t size is not 0x7c8 bytes");

typedef struct svm_vmcb_t {
    svm_vmcb_control_area_t    control_area;
    svm_vmcb_save_state_area_t save_state_area;
    uint8_t                    reserved_sbz[0x1000 - sizeof(svm_vmcb_control_area_t) - sizeof(svm_vmcb_save_state_area_t)];
} __attribute__((packed)) svm_vmcb_t;

_Static_assert(offsetof_field(svm_vmcb_t, save_state_area) == 0x400, "svm_vmcb_t save_state_area offset is not at 0x400 bytes");
_Static_assert(sizeof(svm_vmcb_t) == 0x1000, "svm_vmcb_t size is not 0x1000 bytes");

int8_t hypervisor_svm_vmcb_prepare(hypervisor_vm_t** vm_out);


#ifdef __cplusplus
}
#endif

#endif
