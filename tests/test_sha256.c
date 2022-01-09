#include "setup.h"
#include <sha256.h>

uint32_t main(uint32_t argc, char_t** argv) {
	setup_ram();

	if(argc != 2) {
		print_error("incorrect args");

		return -1;
	}

	argv++;

	sha256_ctx_t ctx = sha256_init();

	if(!ctx) {
		print_error("cannot create sha256 ctx");

		return -1;
	}

	FILE* fp_in = fopen(*argv, "r");

	if(!fp_in) {
		print_error("cannot open file");

		return -1;
	}

	uint8_t chunk[4096];

	while(1) {
		int64_t r = fread(chunk, 1, 4096, fp_in);

		if(!r) {
			break;
		}

		sha256_update(ctx, chunk, r);
	}

	fclose(fp_in);

	uint8_t* sum = sha256_final(ctx);

	for(int64_t i = 0; i < SHA256_OUTPUT_SIZE; i++) {
		printf("%02x", sum[i]);
	}
	printf(" %s\n", *argv);

	memory_free(sum);

	return 0;
}
