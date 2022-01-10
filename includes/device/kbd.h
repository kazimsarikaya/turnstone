/**
 * @file kbd.h
 * @brief keyboard device interface
 */
#ifndef ___DEVICE_KBD_H
/*! prevent duplicate header error macro */
#define ___DEVICE_KBD_H 0


#include <types.h>
#include <cpu/interrupt.h>

int8_t dev_kbd_isr(interrupt_frame_t* frame, uint8_t intnum);

#endif
