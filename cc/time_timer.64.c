#include <time/timer.h>
#include <video.h>
#include <ports.h>

#define TIME_TIMER_PIT_BASE_HZ       1193181
#define TIME_TIMER_PIT_COMMAND_PORT  0x43
#define TIME_TIMER_PIT_COMMAND_WRITE 0x34
#define TIME_TIMER_PIT_DATA_PORT     0x40

__volatile__ uint64_t time_timer_tick_count = 0;

void time_timer_reset_tick_count() {
	time_timer_tick_count = 0;
}

void time_timer_pit_isr(interrupt_frame_t* frame, uint16_t intnum){
	UNUSED(frame);
	UNUSED(intnum);

	time_timer_tick_count++;
}

void time_timer_pit_set_hz(uint16_t hz) {
	uint16_t divisor = TIME_TIMER_PIT_BASE_HZ / hz;         /* Calculate our divisor */
	outb(TIME_TIMER_PIT_COMMAND_PORT, TIME_TIMER_PIT_COMMAND_WRITE);               /* Set our command byte 0x36 */
	outb(TIME_TIMER_PIT_DATA_PORT, divisor & 0xFF);     /* Set low byte of divisor */
	outb(TIME_TIMER_PIT_DATA_PORT, divisor >> 8);       /* Set high byte of divisor */
}

void time_timer_pit_sleep(uint64_t usecs) {
	time_timer_reset_tick_count();
	while(time_timer_tick_count <= usecs);
}

void time_timer_apic_isr(interrupt_frame_t* frame, uint16_t intnum) {
	UNUSED(frame);
	UNUSED(intnum);

	time_timer_tick_count++;

	if((time_timer_tick_count % 1000) == 0) {
		printf("APICTIMER: timer hits!\n");
	}
}
