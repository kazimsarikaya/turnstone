#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.


cat <<EOF
/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu.h>
#include <cpu/descriptor.h>
#include <cpu/interrupt.h>

#ifndef ___DEPEND_ANALYSIS

void interrupt_dummy_noerrcode(interrupt_frame_t*, uint8_t);

EOF

for i in $(seq 32 255); do

cat <<EOF
void __attribute__ ((interrupt)) interrupt_dummy_noerrcode_int${i}(interrupt_frame_t* frame){
	cpu_cli();
  interrupt_dummy_noerrcode(frame, ${i});
  cpu_sti();
}
EOF

done

cat <<EOF
typedef void (*interrupt_dummy_noerrcode_int_ptr)(interrupt_frame_t*);

interrupt_dummy_noerrcode_int_ptr interrupt_dummy_noerrcode_list[224] = {
EOF

for i in $(seq 32 255); do
cat <<EOF
&interrupt_dummy_noerrcode_int${i},
EOF
done

cat <<EOF
};

void interrupt_register_dummy_handlers(descriptor_idt_t* idt) {
  uint64_t fa;
  for(uint32_t i=32;i<256;i++){
    fa=(uint64_t)interrupt_dummy_noerrcode_list[i-32];
    DESCRIPTOR_BUILD_IDT_SEG(idt[i], fa, KERNEL_CODE_SEG, 0, DPL_KERNEL)
  }
}

#endif
EOF
