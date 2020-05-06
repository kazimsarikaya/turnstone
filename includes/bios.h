/**
 * @file bios.h
 * @brief data structures for bios
 */
#ifndef ___BIOS_H
/*! prevent duplicate header error macro */
#define ___BIOS_H 0

/*! BDA memory location*/
#define BIOS_BDA_POINTER 0x400

/**
 * @struct bios_data_area_t
 * @brief bios data area struct
 *
 * always at @ref BIOS_BDA_POINTER
 */
typedef struct {
	uint16_t com1; ///< com1 port at 0x400
	uint16_t com2; ///< com2 port at 0x402
	uint16_t com3; ///< com3 port at 0x404
	uint16_t com4; ///< com4 port at 0x406
	uint16_t lpt1; ///< lpt1 port at 0x408
	uint16_t lpt2; ///< lpt2 port at 0x40A
	uint16_t lpt3; ///< lpt3 port at 0x40C
	uint16_t ebda_base_address; ///< ebda base address at 0x40E
	uint16_t detected_hw_packet_bit_flags; ///< packet bit flags for detected hardware at 0x410
	uint8_t unused_1[5]; ///< unused data 0x412-0x416
	uint16_t keybord_state_flags; ///< keyboard state flags at 0x417
	uint8_t unused_2[8]; ///< unused data 0x418-0x41D
	uint8_t keyboard_buffer[32]; ///< keyboard buffer at 0x41E
	uint8_t unused_3[11]; ///< unused data 0x43E-0x448
	uint8_t display_mode; ///< display mode at 0x449
	uint16_t text_mode_column_count; ///< text mode column count at 0x44A
	uint8_t unused_4[24]; ///< unused data 0x44B-0x462
	uint16_t video_base_port; ///< video controller base port at 0x463
	uint8_t unused_5[7]; ///< unused data 0x465-0x46B
	uint16_t irq0_timer_ticks; ///< irq0 timer ticks since boot at 0x46C
	uint8_t unused_6[8]; ///< unused data 0x46D-0x474
	uint8_t number_of_detected_hds; ///< number of detected hard drives at 0x475
	uint8_t unused_7[4]; ///< unused data 0x476-0x479
	uint16_t keyboard_buffer_start; ///< keyboard buffer start at 0x480
	uint16_t keyboard_buffer_end; ///< keyboard buffer end at 0x482
	uint8_t unused_8[19]; ///< unused data 0x484-0x496
	uint8_t last_keyboard_state; ///< last keyboard led shift key state at 0x497
	uint8_t unused_9[104]; ///< unused data 0x498-0x4FF
}__attribute__((packed)) bios_data_area_t; ///< short hand for struct

#endif
