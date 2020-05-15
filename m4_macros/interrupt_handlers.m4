define(`forloop',
     `pushdef(`$1', `$2')_forloop(`$1', `$2', `$3', `$4')popdef(`$1')')dnl
define(`_forloop',
     `$4`'ifelse($1, `$3', ,
          `define(`$1', incr($1))_forloop(`$1', `$2', `$3', `$4')')')dnl



define(`CONCAT', `$1'`$2'`$3')dnl

forloop(`i',32,255, `CONCAT(`void __attribute__ ((interrupt)) interrupt_dummy_noerrcode_int',i,`(interrupt_frame_t* frame){
  interrupt_dummy_noerrcode(frame, i);
}
')'
)

typedef void (*interrupt_dummy_noerrcode_int_ptr)(interrupt_frame_t*);

interrupt_dummy_noerrcode_int_ptr interrupt_dummy_noerrcode_list[224] = {
forloop(`i',32,254, `CONCAT(`&interrupt_dummy_noerrcode_int',i,`,
')')
&interrupt_dummy_noerrcode_int255
};

void interrupt_register_dummy_handlers(descriptor_idt_t* idt) {
  uint64_t fa;
  for(uint32_t i=32;i<256;i++){
    fa=(uint64_t)interrupt_dummy_noerrcode_list[i-32];
    DESCRIPTOR_BUILD_IDT_SEG(idt[i], fa, KERNEL_CODE_SEG, 0, DPL_KERNEL)
  }
}
