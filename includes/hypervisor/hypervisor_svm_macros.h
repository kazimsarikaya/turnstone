/**
 * @file hypervisor_svm_macros.h
 * @brief defines hypervisor svm (amd) related macros
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_SVM_MACROS_H
#define ___HYPERVISOR_SVM_MACROS_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYPERVISOR_AMD_ECX_HYPERVISOR_BIT   2

#define SVM_MSR_VM_HSAVE_PA                 0xc0010117
#define CPU_MSR_IA32_PAT                    0x00000277

#define SVM_DATA_ACCESS_RIGHTS 0x0093
#define SVM_CODE_ACCESS_RIGHTS 0x029b
#define SVM_LDTR_ACCESS_RIGHTS 0x0082
#define SVM_TR_ACCESS_RIGHTS   0x008b
#define SVM_RFLAG_RESERVED (1 << 1)

#define SVM_GUEST_IDTR_BASE_VALUE  0x1000
#define SVM_GUEST_IFEXT_BASE_VALUE 0x2000
#define SVM_GUEST_GDTR_BASE_VALUE  0x3000
#define SVM_GUEST_TR_BASE_VALUE    0x4000
#define SVM_GUEST_CR3_BASE_VALUE   0x5000

#define SVM_GUEST_GOT_BASE_VALUE  (8ULL << 40)
#define SVM_GUEST_HEAP_BASE_VALUE (4ULL << 40)
#define SVM_GUEST_STACK_TOP_VALUE  SVM_GUEST_HEAP_BASE_VALUE


#define SVM_VMEXIT_REASON_CR0_READ            0x0000
#define SVM_VMEXIT_REASON_CR1_READ            0x0001
#define SVM_VMEXIT_REASON_CR2_READ            0x0002
#define SVM_VMEXIT_REASON_CR3_READ            0x0003
#define SVM_VMEXIT_REASON_CR4_READ            0x0004
#define SVM_VMEXIT_REASON_CR5_READ            0x0005
#define SVM_VMEXIT_REASON_CR6_READ            0x0006
#define SVM_VMEXIT_REASON_CR7_READ            0x0007
#define SVM_VMEXIT_REASON_CR8_READ            0x0008
#define SVM_VMEXIT_REASON_CR9_READ            0x0009
#define SVM_VMEXIT_REASON_CR10_READ           0x000a
#define SVM_VMEXIT_REASON_CR11_READ           0x000b
#define SVM_VMEXIT_REASON_CR12_READ           0x000c
#define SVM_VMEXIT_REASON_CR13_READ           0x000d
#define SVM_VMEXIT_REASON_CR14_READ           0x000e
#define SVM_VMEXIT_REASON_CR15_READ           0x000f
#define SVM_VMEXIT_REASON_CR0_WRITE           0x0010
#define SVM_VMEXIT_REASON_CR1_WRITE           0x0011
#define SVM_VMEXIT_REASON_CR2_WRITE           0x0012
#define SVM_VMEXIT_REASON_CR3_WRITE           0x0013
#define SVM_VMEXIT_REASON_CR4_WRITE           0x0014
#define SVM_VMEXIT_REASON_CR5_WRITE           0x0015
#define SVM_VMEXIT_REASON_CR6_WRITE           0x0016
#define SVM_VMEXIT_REASON_CR7_WRITE           0x0017
#define SVM_VMEXIT_REASON_CR8_WRITE           0x0018
#define SVM_VMEXIT_REASON_CR9_WRITE           0x0019
#define SVM_VMEXIT_REASON_CR10_WRITE          0x001a
#define SVM_VMEXIT_REASON_CR11_WRITE          0x001b
#define SVM_VMEXIT_REASON_CR12_WRITE          0x001c
#define SVM_VMEXIT_REASON_CR13_WRITE          0x001d
#define SVM_VMEXIT_REASON_CR14_WRITE          0x001e
#define SVM_VMEXIT_REASON_CR15_WRITE          0x001f
#define SVM_VMEXIT_REASON_DR0_READ            0x0020
#define SVM_VMEXIT_REASON_DR1_READ            0x0021
#define SVM_VMEXIT_REASON_DR2_READ            0x0022
#define SVM_VMEXIT_REASON_DR3_READ            0x0023
#define SVM_VMEXIT_REASON_DR4_READ            0x0024
#define SVM_VMEXIT_REASON_DR5_READ            0x0025
#define SVM_VMEXIT_REASON_DR6_READ            0x0026
#define SVM_VMEXIT_REASON_DR7_READ            0x0027
#define SVM_VMEXIT_REASON_DR8_READ            0x0028
#define SVM_VMEXIT_REASON_DR9_READ            0x0029
#define SVM_VMEXIT_REASON_DR10_READ           0x002a
#define SVM_VMEXIT_REASON_DR11_READ           0x002b
#define SVM_VMEXIT_REASON_DR12_READ           0x002c
#define SVM_VMEXIT_REASON_DR13_READ           0x002d
#define SVM_VMEXIT_REASON_DR14_READ           0x002e
#define SVM_VMEXIT_REASON_DR15_READ           0x002f
#define SVM_VMEXIT_REASON_DR0_WRITE           0x0030
#define SVM_VMEXIT_REASON_DR1_WRITE           0x0031
#define SVM_VMEXIT_REASON_DR2_WRITE           0x0032
#define SVM_VMEXIT_REASON_DR3_WRITE           0x0033
#define SVM_VMEXIT_REASON_DR4_WRITE           0x0034
#define SVM_VMEXIT_REASON_DR5_WRITE           0x0035
#define SVM_VMEXIT_REASON_DR6_WRITE           0x0036
#define SVM_VMEXIT_REASON_DR7_WRITE           0x0037
#define SVM_VMEXIT_REASON_DR8_WRITE           0x0038
#define SVM_VMEXIT_REASON_DR9_WRITE           0x0039
#define SVM_VMEXIT_REASON_DR10_WRITE          0x003a
#define SVM_VMEXIT_REASON_DR11_WRITE          0x003b
#define SVM_VMEXIT_REASON_DR12_WRITE          0x003c
#define SVM_VMEXIT_REASON_DR13_WRITE          0x003d
#define SVM_VMEXIT_REASON_DR14_WRITE          0x003e
#define SVM_VMEXIT_REASON_DR15_WRITE          0x003f
#define SVM_VMEXIT_REASON_EXCP0               0x0040
#define SVM_VMEXIT_REASON_EXCP1               0x0041
#define SVM_VMEXIT_REASON_EXCP2               0x0042
#define SVM_VMEXIT_REASON_EXCP3               0x0043
#define SVM_VMEXIT_REASON_EXCP4               0x0044
#define SVM_VMEXIT_REASON_EXCP5               0x0045
#define SVM_VMEXIT_REASON_EXCP6               0x0046
#define SVM_VMEXIT_REASON_EXCP7               0x0047
#define SVM_VMEXIT_REASON_EXCP8               0x0048
#define SVM_VMEXIT_REASON_EXCP9               0x0049
#define SVM_VMEXIT_REASON_EXCP10              0x004a
#define SVM_VMEXIT_REASON_EXCP11              0x004b
#define SVM_VMEXIT_REASON_EXCP12              0x004c
#define SVM_VMEXIT_REASON_EXCP13              0x004d
#define SVM_VMEXIT_REASON_EXCP14              0x004e
#define SVM_VMEXIT_REASON_EXCP15              0x004f
#define SVM_VMEXIT_REASON_EXCP16              0x0050
#define SVM_VMEXIT_REASON_EXCP17              0x0051
#define SVM_VMEXIT_REASON_EXCP18              0x0052
#define SVM_VMEXIT_REASON_EXCP19              0x0053
#define SVM_VMEXIT_REASON_EXCP20              0x0054
#define SVM_VMEXIT_REASON_EXCP21              0x0055
#define SVM_VMEXIT_REASON_EXCP22              0x0056
#define SVM_VMEXIT_REASON_EXCP23              0x0057
#define SVM_VMEXIT_REASON_EXCP24              0x0058
#define SVM_VMEXIT_REASON_EXCP25              0x0059
#define SVM_VMEXIT_REASON_EXCP26              0x005a
#define SVM_VMEXIT_REASON_EXCP27              0x005b
#define SVM_VMEXIT_REASON_EXCP28              0x005c
#define SVM_VMEXIT_REASON_EXCP29              0x005d
#define SVM_VMEXIT_REASON_EXCP30              0x005e
#define SVM_VMEXIT_REASON_EXCP31              0x005f
#define SVM_VMEXIT_REASON_INTR                0x0060
#define SVM_VMEXIT_REASON_NMI                 0x0061
#define SVM_VMEXIT_REASON_SMI                 0x0062
#define SVM_VMEXIT_REASON_INIT                0x0063
#define SVM_VMEXIT_REASON_VINTR               0x0064
#define SVM_VMEXIT_REASON_CR0_SEL_WRITE       0x0065
#define SVM_VMEXIT_REASON_IDTR_READ           0x0066
#define SVM_VMEXIT_REASON_GDTR_READ           0x0067
#define SVM_VMEXIT_REASON_LDTR_READ           0x0068
#define SVM_VMEXIT_REASON_TR_READ             0x0069
#define SVM_VMEXIT_REASON_IDTR_WRITE          0x006a
#define SVM_VMEXIT_REASON_GDTR_WRITE          0x006b
#define SVM_VMEXIT_REASON_LDTR_WRITE          0x006c
#define SVM_VMEXIT_REASON_TR_WRITE            0x006d
#define SVM_VMEXIT_REASON_RDTSC               0x006e
#define SVM_VMEXIT_REASON_RDPMC               0x006f
#define SVM_VMEXIT_REASON_PUSHF               0x0070
#define SVM_VMEXIT_REASON_POPF                0x0071
#define SVM_VMEXIT_REASON_CPUID               0x0072
#define SVM_VMEXIT_REASON_RSM                 0x0073
#define SVM_VMEXIT_REASON_IRET                0x0074
#define SVM_VMEXIT_REASON_INTn                0x0075
#define SVM_VMEXIT_REASON_INVD                0x0076
#define SVM_VMEXIT_REASON_PAUSE               0x0077
#define SVM_VMEXIT_REASON_HLT                 0x0078
#define SVM_VMEXIT_REASON_INVLPG              0x0079
#define SVM_VMEXIT_REASON_INVLPGA             0x007a
#define SVM_VMEXIT_REASON_IOIO                0x007b
#define SVM_VMEXIT_REASON_MSR                 0x007c
#define SVM_VMEXIT_REASON_TASK_SWITCH         0x007d
#define SVM_VMEXIT_REASON_FERR_FREEZE         0x007e
#define SVM_VMEXIT_REASON_SHUTDOWN            0x007f
#define SVM_VMEXIT_REASON_VMRUN               0x0080
#define SVM_VMEXIT_REASON_VMMCALL             0x0081
#define SVM_VMEXIT_REASON_VMLOAD              0x0082
#define SVM_VMEXIT_REASON_VMSAVE              0x0083
#define SVM_VMEXIT_REASON_STGI                0x0084
#define SVM_VMEXIT_REASON_CLGI                0x0085
#define SVM_VMEXIT_REASON_SKINIT              0x0086
#define SVM_VMEXIT_REASON_RDTSCP              0x0087
#define SVM_VMEXIT_REASON_ICEBP               0x0088
#define SVM_VMEXIT_REASON_WBINVD              0x0089
#define SVM_VMEXIT_REASON_MONITOR             0x008a
#define SVM_VMEXIT_REASON_MWAIT               0x008b
#define SVM_VMEXIT_REASON_MWAIT_CONDITIONAL   0x008c
#define SVM_VMEXIT_REASON_RDPRU               0x008d
#define SVM_VMEXIT_REASON_XSETBV              0x008e
#define SVM_VMEXIT_REASON_EFER_WRITE_TRAP     0x008f
#define SVM_VMEXIT_REASON_CR0_WRITE_TRAP      0x0090
#define SVM_VMEXIT_REASON_CR1_WRITE_TRAP      0x0091
#define SVM_VMEXIT_REASON_CR2_WRITE_TRAP      0x0092
#define SVM_VMEXIT_REASON_CR3_WRITE_TRAP      0x0093
#define SVM_VMEXIT_REASON_CR4_WRITE_TRAP      0x0094
#define SVM_VMEXIT_REASON_CR5_WRITE_TRAP      0x0095
#define SVM_VMEXIT_REASON_CR6_WRITE_TRAP      0x0096
#define SVM_VMEXIT_REASON_CR7_WRITE_TRAP      0x0097
#define SVM_VMEXIT_REASON_CR8_WRITE_TRAP      0x0098
#define SVM_VMEXIT_REASON_CR9_WRITE_TRAP      0x0099
#define SVM_VMEXIT_REASON_CR10_WRITE_TRAP     0x009a
#define SVM_VMEXIT_REASON_CR11_WRITE_TRAP     0x009b
#define SVM_VMEXIT_REASON_CR12_WRITE_TRAP     0x009c
#define SVM_VMEXIT_REASON_CR13_WRITE_TRAP     0x009d
#define SVM_VMEXIT_REASON_CR14_WRITE_TRAP     0x009e
#define SVM_VMEXIT_REASON_CR15_WRITE_TRAP     0x009f
#define SVM_VMEXIT_REASON_INVLPGB             0x00a0
#define SVM_VMEXIT_REASON_INVLPGB_ILLEGAL     0x00a1
#define SVM_VMEXIT_REASON_INVPCID             0x00a2
#define SVM_VMEXIT_REASON_MCOMMIT             0x00a3
#define SVM_VMEXIT_REASON_TLBSYNC             0x00a4
#define SVM_VMEXIT_REASON_BUSLOCK             0x00a5
#define SVM_VMEXIT_REASON_IDLE_HLT            0x00a6

#define SVM_VMEXIT_REASON_NPF                 0x0400
#define SVM_VMEXIT_REASON_AVIC_INCOMPLETE_IPI 0x0401
#define SVM_VMEXIT_REASON_AVIC_NOACCEL        0x0402
#define SVM_VMEXIT_REASON_VMGEXIT             0x0403

#define SVM_VMEXIT_REASON_INVALID             -1
#define SVM_VMEXIT_REASON_BUSY                -2
#define SVM_VMEXIT_REASON_IDLE_REQUIRED       -3
#define SVM_VMEXIT_REASON_INVALID_PMC         -4

#define SVM_VMEXIT_REASON_UNUSED              0xF000_0000 // is it correct? docs say 0xF000_000 one zero is missing

#define SVM_VMEXIT_REASON_COUNT               (0x00a7 + 4 + 4 + 1) // a7 for first group, 4 for second group, 4 for third group, 1 for last group


#ifdef __cplusplus
}
#endif

#endif
