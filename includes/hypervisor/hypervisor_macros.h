/**
 * @file hypervisor_macros.h
 * @brief defines hypervisor related macros
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_MACROS_H
#define ___HYPERVISOR_MACROS_H 0

#include <types.h>

#define HYPERVISOR_ECX_HYPERVISOR_BIT 5

#define FEATURE_CONTROL_VMXON_OUTSIDE_SMX (1 << 2)
#define FEATURE_CONTROL_LOCKED        (1 << 0)
#define CPU_MSR_IA32_FEATURE_CONTROL        0x0000003a


#define VMX_DATA_ACCESS_RIGHT (0x3 | 1 << 4 | 1 << 7)
#define VMX_CODE_ACCESS_RIGHT (0x3 | 1 << 4 | 1 << 7 | 1 << 13)
#define VMX_LDTR_ACCESS_RIGHT (0x2 | 1 << 7)
#define VMX_TR_ACCESS_RIGHT (0x3 | 1 << 7)
#define VMX_RFLAG_RESERVED (1 << 1)


#define VMX_RFLAG_RESERVED (1 << 1)



#define CPU_MSR_IA32_VMX_BASIC                  0x0480
#define CPU_MSR_IA32_VMX_PINBASED_CTLS          0x0481
#define CPU_MSR_IA32_VMX_PRI_PROCBASED_CTLS     0x0482
#define CPU_MSR_IA32_VMX_VM_EXIT_CTLS           0x0483
#define CPU_MSR_IA32_VMX_VM_ENTRY_CTLS          0x0484
#define CPU_MSR_IA32_VMX_CR0_FIXED0             0x0486
#define CPU_MSR_IA32_VMX_CR0_FIXED1             0x0487
#define CPU_MSR_IA32_VMX_CR4_FIXED0             0x0488
#define CPU_MSR_IA32_VMX_CR4_FIXED1             0x0489
#define CPU_MSR_IA32_VMX_SEC_PROCBASED_CTLS     0x048b
#define CPU_MSR_IA32_VMX_EPT_VPID_CAP           0x048c


#define VMX_CTLS_VPID 0x0000


#define VMX_HOST_ES_SELECTOR            0x0c00
#define VMX_HOST_CS_SELECTOR            0x0c02
#define VMX_HOST_SS_SELECTOR            0x0c04
#define VMX_HOST_DS_SELECTOR            0x0c06
#define VMX_HOST_FS_SELECTOR            0x0c08
#define VMX_HOST_GS_SELECTOR            0x0c0a
#define VMX_HOST_TR_SELECTOR            0x0c0c

#define VMX_HOST_EFER                   0x2c02

#define VMX_HOST_IA32_SYSENTER_CS       0x4c00

#define VMX_HOST_CR0                    0x6c00
#define VMX_HOST_CR3                    0x6c02
#define VMX_HOST_CR4                    0x6c04
#define VMX_HOST_FS_BASE                0x6c06
#define VMX_HOST_GS_BASE                0x6c08
#define VMX_HOST_TR_BASE                0x6c0a
#define VMX_HOST_GDTR_BASE              0x6c0c
#define VMX_HOST_IDTR_BASE              0x6c0e
#define VMX_HOST_IA32_SYSENTER_ESP      0x6c10
#define VMX_HOST_IA32_SYSENTER_EIP      0x6c12
#define VMX_HOST_RSP                    0x6c14
#define VMX_HOST_RIP                    0x6c16

#define VMX_GUEST_ES_SELECTOR           0x0800
#define VMX_GUEST_CS_SELECTOR           0x0802
#define VMX_GUEST_SS_SELECTOR           0x0804
#define VMX_GUEST_DS_SELECTOR           0x0806
#define VMX_GUEST_FS_SELECTOR           0x0808
#define VMX_GUEST_GS_SELECTOR           0x080a
#define VMX_GUEST_LDTR_SELECTOR         0x080e
#define VMX_GUEST_TR_SELECTOR           0x080c

#define VMX_GUEST_CR0                       0x6800
#define VMX_GUEST_CR3                       0x6802
#define VMX_GUEST_CR4                       0x6804
#define VMX_GUEST_ES_BASE                   0x6806
#define VMX_GUEST_CS_BASE                   0x6808
#define VMX_GUEST_SS_BASE                   0x680a
#define VMX_GUEST_DS_BASE                   0x680c
#define VMX_GUEST_FS_BASE                   0x680e
#define VMX_GUEST_GS_BASE                   0x6810
#define VMX_GUEST_LDTR_BASE                 0x6812
#define VMX_GUEST_TR_BASE                   0x6814
#define VMX_GUEST_IDTR_BASE                 0x6816
#define VMX_GUEST_GDTR_BASE                 0x6818
#define VMX_GUEST_DR7                       0x681a
#define VMX_GUEST_RSP                       0x681c
#define VMX_GUEST_RIP                       0x681e
#define VMX_GUEST_RFLAGS                    0x6820
#define VMX_GUEST_PENDING_DEBUG_EXCEPTIONS  0x6822
#define VMX_GUEST_IA32_SYSENTER_ESP         0x6824
#define VMX_GUEST_IA32_SYSENTER_EIP         0x6826

#define VMX_CTLS_IO_BITMAP_A                0x2000
#define VMX_CTLS_IO_BITMAP_B                0x2002
#define VMX_CTLS_VM_EXIT_MSR_STORE          0x2006
#define VMX_CTLS_VM_EXIT_MSR_LOAD           0x2008
#define VMX_CTLS_VM_ENTRY_MSR_LOAD          0x200a
#define VMX_CTLS_EPTP                       0x201a

#define VMX_GUEST_VMCS_LINK_POINTER_LOW     0x2800
#define VMX_GUEST_VMCS_LINK_POINTER_HIGH    0x2801
#define VMX_GUEST_IA32_EFER                 0x2806

#define VMX_CTLS_PIN_BASED_VM_EXECUTION                0x4000
#define VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION           0x4002
#define VMX_CTLS_EXCEPTION_BITMAP                      0x4004
#define VMX_CTLS_CR3_TARGET_COUNT                      0x400a
#define VMX_CTLS_VM_EXIT                               0x400c
#define VMX_CTLS_VM_EXIT_MSR_STORE_COUNT               0x400e
#define VMX_CTLS_VM_EXIT_MSR_LOAD_COUNT                0x4010
#define VMX_CTLS_VM_ENTRY                              0x4012
#define VMX_CTLS_VM_ENTRY_MSR_LOAD_COUNT               0x4014
#define VMX_CTLS_VM_ENTRY_INTERRUPT_INFORMATION_FIELD  0x4016
#define VMX_CTLS_SEC_PROC_BASED_VM_EXECUTION           0x401e

#define VMX_GUEST_ES_LIMIT                  0x4800
#define VMX_GUEST_CS_LIMIT                  0x4802
#define VMX_GUEST_SS_LIMIT                  0x4804
#define VMX_GUEST_DS_LIMIT                  0x4806
#define VMX_GUEST_FS_LIMIT                  0x4808
#define VMX_GUEST_GS_LIMIT                  0x480a
#define VMX_GUEST_LDTR_LIMIT                0x480c
#define VMX_GUEST_TR_LIMIT                  0x480e
#define VMX_GUEST_GDTR_LIMIT                0x4810
#define VMX_GUEST_IDTR_LIMIT                0x4812
#define VMX_GUEST_ES_ACCESS_RIGHT           0x4814
#define VMX_GUEST_CS_ACCESS_RIGHT           0x4816
#define VMX_GUEST_SS_ACCESS_RIGHT           0x4818
#define VMX_GUEST_DS_ACCESS_RIGHT           0x481a
#define VMX_GUEST_FS_ACCESS_RIGHT           0x481c
#define VMX_GUEST_GS_ACCESS_RIGHT           0x481e
#define VMX_GUEST_LDTR_ACCESS_RIGHT         0x4820
#define VMX_GUEST_TR_ACCESS_RIGHT           0x4822
#define VMX_GUEST_INTERRUPTIBILITY_STATE    0x4824
#define VMX_GUEST_ACTIVITY_STATE            0x4826
#define VMX_GUEST_SMBASE                    0x4828
#define VMX_GUEST_IA32_SYSENTER_CS          0x482a
#define VMX_GUEST_IA32_PREMPTION_TIMER      0x482e

#define VMX_VM_INSTRUCTION_ERROR            0x4400



















#endif
