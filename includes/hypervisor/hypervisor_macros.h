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


#define VMX_DATA_ACCESS_RIGHT  0x0093
#define VMX_CODE_ACCESS_RIGHT  0x209b
#define VMX_LDTR_ACCESS_RIGHT  0x0082
#define VMX_TR_ACCESS_RIGHT    0x008b
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
#define VMX_GUEST_LDTR_SELECTOR         0x080c
#define VMX_GUEST_TR_SELECTOR           0x080e

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
#define VMX_GUEST_GDTR_BASE                 0x6816
#define VMX_GUEST_IDTR_BASE                 0x6818
#define VMX_GUEST_DR7                       0x681a
#define VMX_GUEST_RSP                       0x681c
#define VMX_GUEST_RIP                       0x681e
#define VMX_GUEST_RFLAGS                    0x6820
#define VMX_GUEST_PENDING_DEBUG_EXCEPTIONS  0x6822
#define VMX_GUEST_IA32_SYSENTER_ESP         0x6824
#define VMX_GUEST_IA32_SYSENTER_EIP         0x6826

#define VMX_CTLS_IO_BITMAP_A                0x2000
#define VMX_CTLS_IO_BITMAP_B                0x2002
#define VMX_CTLS_MSR_BITMAP                 0x2004
#define VMX_CTLS_VM_EXIT_MSR_STORE          0x2006
#define VMX_CTLS_VM_EXIT_MSR_LOAD           0x2008
#define VMX_CTLS_VM_ENTRY_MSR_LOAD          0x200a
#define VMX_CTLS_TSC_OFFSET                 0x2010
#define VMX_CTLS_VIRTUAL_APIC_PAGE_ADDR     0x2012
#define VMX_CTLS_APIC_ACCESS_ADDR           0x2014
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
#define VMX_CTLS_VM_ENTRY_EXCEPTION_ERROR_CODE         0x4018
#define VMX_CTLS_VM_ENTRY_INSTRUCTION_LENGTH           0x401a
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

#define VMX_GUEST_PHYSICAL_ADDR             0x2400
#define VMX_VMEXIT_REASON                   0x4402
#define VMX_VMEXIT_INTERRUPT_INFO           0x4404
#define VMX_VMEXIT_INTERRUPT_ERROR_CODE     0x4406
#define VMX_VMEXIT_INSTRUCTION_LENGTH       0x440c
#define VMX_VMEXIT_INSTRUCTION_INFO         0x440e
#define VMX_EXIT_QUALIFICATION              0x6400
#define VMX_GUEST_LINEAR_ADDR               0x640a

#define VMX_VMEXIT_REASON_EXCEPTION_OR_NMI            0
#define VMX_VMEXIT_REASON_EXTERNAL_INTERRUPT          1
#define VMX_VMEXIT_REASON_TRIPLE_FAULT                2
#define VMX_VMEXIT_REASON_INIT                        3
#define VMX_VMEXIT_REASON_SIPI                        4
#define VMX_VMEXIT_REASON_IO_SMI                      5
#define VMX_VMEXIT_REASON_OTHER_SMI                   6
#define VMX_VMEXIT_REASON_INTERRUPT_WINDOW            7
#define VMX_VMEXIT_REASON_NMI_WINDOW                  8
#define VMX_VMEXIT_REASON_TASK_SWITCH                 9
#define VMX_VMEXIT_REASON_CPUID                      10
#define VMX_VMEXIT_REASON_GETSEC                     11
#define VMX_VMEXIT_REASON_HLT                        12
#define VMX_VMEXIT_REASON_INVD                       13
#define VMX_VMEXIT_REASON_INVLPG                     14
#define VMX_VMEXIT_REASON_RDPMC                      15
#define VMX_VMEXIT_REASON_RDTSC                      16
#define VMX_VMEXIT_REASON_RSM                        17
#define VMX_VMEXIT_REASON_VMCALL                     18
#define VMX_VMEXIT_REASON_VMCLEAR                    19
#define VMX_VMEXIT_REASON_VMLAUNCH                   20
#define VMX_VMEXIT_REASON_VMPTRLD                    21
#define VMX_VMEXIT_REASON_VMPTRST                    22
#define VMX_VMEXIT_REASON_VMREAD                     23
#define VMX_VMEXIT_REASON_VMRESUME                   24
#define VMX_VMEXIT_REASON_VMWRITE                    25
#define VMX_VMEXIT_REASON_VMXOFF                     26
#define VMX_VMEXIT_REASON_VMXON                      27
#define VMX_VMEXIT_CONTROL_REGISTER_ACCESS           28
#define VMX_VMEXIT_REASON_MOV_CR                     29
#define VMX_VMEXIT_REASON_IO_INSTRUCTION             30
#define VMX_VMEXIT_REASON_RDMSR                      31
#define VMX_VMEXIT_REASON_WRMSR                      32
#define VMX_VMEXIT_REASON_INVALID_GUEST_STATE        33
#define VMX_VMEXIT_REASON_MSR_LOADING                34
#define VMX_VMEXIT_REASON_MWAIT                      36
#define VMX_VMEXIT_REASON_MONITOR_TRAP_FLAG          37
#define VMX_VMEXIT_REASON_MONITOR                    39
#define VMX_VMEXIT_REASON_PAUSE                      40
#define VMX_VMEXIT_REASON_MACHINE_CHECK              41
#define VMX_VMEXIT_REASON_TPR_BELOW_THRESHOLD        43
#define VMX_VMEXIT_REASON_APIC_ACCESS                44
#define VMX_VMEXIT_REASON_VIRTUALIZED_EOI            45
#define VMX_VMEXIT_REASON_GDTR_IDTR_ACCESS           46
#define VMX_VMEXIT_REASON_LDTR_TR_ACCESS             47
#define VMX_VMEXIT_REASON_EPT_VIOLATION              48
#define VMX_VMEXIT_REASON_EPT_MISCONFIG              49
#define VMX_VMEXIT_REASON_INVEPT                     50
#define VMX_VMEXIT_REASON_RDTSCP                     51
#define VMX_VMEXIT_REASON_VMX_TIMER_EXPIRED          52
#define VMX_VMEXIT_REASON_INVVPID                    53
#define VMX_VMEXIT_REASON_WBINVD                     54
#define VMX_VMEXIT_REASON_XSETBV                     55
#define VMX_VMEXIT_REASON_APIC_WRITE                 56
#define VMX_VMEXIT_REASON_RDRAND                     57
#define VMX_VMEXIT_REASON_INVPCID                    58
#define VMX_VMEXIT_REASON_VMFUNC                     59
#define VMX_VMEXIT_REASON_ENCLS                      60
#define VMX_VMEXIT_REASON_RDSEED                     61
#define VMX_VMEXIT_REASON_PML_FULL                   62
#define VMX_VMEXIT_REASON_XSAVES                     63
#define VMX_VMEXIT_REASON_XRSTORS                    64
#define VMX_VMEXIT_REASON_SPP                        66
#define VMX_VMEXIT_REASON_UMWAIT                     67
#define VMX_VMEXIT_REASON_TPAUSE                     68
#define VMX_VMEXIT_REASON_LOADIWKEY                  69
#define VMX_VMEXIT_REASON_COUNT                      70















#endif
