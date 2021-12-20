#include <types.h>
#include <efi.h>

int64_t efi_main(efi_handle_t efi_handle, efi_system_table_t* system_table) {
	char_t* s_msg = "Hello world!\n\r\0";
	wchar_t msg[128];

	uint8_t i = 0;

	while(s_msg[i] != NULL) {
		msg[i] = s_msg[i];
		i++;
	}

	system_table->console_output->output_string(system_table->console_output, msg);

	while(1);

	return 0;
}
