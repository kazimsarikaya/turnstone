/**
 * @file backtrace.64.c
 * @brief backtrace (stack trace) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <backtrace.h>
#include <video.h>
#include <logging.h>

stackframe_t* backtrace_get_stackframe() {
	stackframe_t* frame = NULL;

	asm ("mov %%rbp, %0\n" : "=r" (frame));

	return frame;
}

void backtrace_print(stackframe_t* frame) {
	PRINTLOG(KERNEL, LOG_ERROR, "Trace:", 0);

	while(frame) {
		PRINTLOG(KERNEL, LOG_ERROR, "\tRIP: 0x%lx RBP: 0x%lp", frame->rip, frame);

		frame = frame->previous;
	}

}

void backtrace(){
	backtrace_print(backtrace_get_stackframe());
}
