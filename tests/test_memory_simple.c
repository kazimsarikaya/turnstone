#include "setup.h"
#include <memory/paging.h>

int main(){
	setup_ram();
	FILE* fp;

	char* data1 = memory_malloc(sizeof(char) * 31);
	if(data1 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data1", data1, 5);
	// if(memory_free(data) != 0) {
	// 	print_error("Cannot free");
	// 	return -1;
	// }
	char* data2 = memory_malloc(sizeof(char) * 117);
	if(data2 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data2", data2, 5);
	char* data3 = memory_malloc(sizeof(char) * 87);
	if(data3 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data3", data3, 5);

	memory_free(data2);

	char* data4 = memory_malloc(sizeof(char) * 29);
	if(data4 == NULL) {
		print_error("Cannot malloc");
		return -1;
	}
	memory_memcopy("data4", data4, 5);

	fp = fopen( "tmp/mem1.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);

	memory_free(data1);
	memory_free(data3);
	memory_free(data4);

	print_success("OK");

	uint64_t fa;
	memory_page_table_t* p4 = memory_paging_malloc_page();
	if(memory_paging_add_page(p4, 0x0, ((uint64_t)0x1234) << 12, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
		print_error("can not add 4k huge page");
	} else {
		print_success("page adding succeed");
	}

	fa = 0;
	if(memory_paging_delete_page_ext(p4, 0x0, &fa) != 0) {
		print_error("delete page failed");
	} else {
		fa >>= 12;
		printf("fa: 0x%08x\n", fa);
		if(fa != 0x1234) {
			print_error("delete page failed");
		} else {
			if(memory_paging_get_frame_address_ext(p4, 0x0, &fa) != -1) {
				print_error("delete page failed");
			} else {
				print_success("delete page succeed");
			}
		}
	}


	if(memory_paging_add_page(p4, 0x0, ((uint64_t)0x1234) << 12, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
		print_error("can not add 4k huge page");
	} else {
		print_success("page adding succeed");
	}

	fa = 0;
	if(memory_paging_delete_page_ext(p4, 0x0, &fa) != 0) {
		print_error("delete page failed");
	} else {
		fa >>= 12;
		printf("fa: 0x%08x\n", fa);
		if(fa != 0x1234) {
			print_error("delete page failed");
		} else {
			if(memory_paging_get_frame_address_ext(p4, 0x0, &fa) != -1) {
				print_error("delete page failed");
			} else {
				print_success("delete page succeed");
			}
		}
	}

	if(memory_paging_add_page(p4, ((uint64_t)0x3) << 21, ((uint64_t)0x1234) << 21, MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
		print_error("can not add 2m huge page");
	} else {
		print_success("page adding succeed");
	}
	fa = 0;
	if(memory_paging_delete_page_ext(p4, ((uint64_t)0x3) << 21, &fa) != 0) {
		print_error("delete page failed");
	} else {
		fa >>= 21;
		printf("fa: 0x%08x\n", fa);
		if(fa != 0x1234) {
			print_error("delete page failed");
		} else {
			print_success("delete page succeed");
		}
	}

	if(memory_paging_add_page(p4, ((uint64_t)0x3) << 30, ((uint64_t)0x12) << 30, MEMORY_PAGING_PAGE_TYPE_1G) != 0) {
		print_error("can not add 1g huge page");
	} else {
		print_success("page adding succeed");
	}
	fa = 0;
	if(memory_paging_delete_page_ext(p4, ((uint64_t)0x3) << 30, &fa) != 0) {
		print_error("delete page failed");
	} else {
		fa >>= 30;
		printf("fa: 0x%08x\n", fa);
		if(fa != 0x12) {
			print_error("delete page failed");
		} else {
			print_success("delete page succeed");
		}
	}

	memory_paging_add_page(p4, 0x0, ((uint64_t)0x1234) << 12, MEMORY_PAGING_PAGE_TYPE_4K);
	memory_paging_add_page(p4, ((uint64_t)0x3) << 21, ((uint64_t)0x1234) << 21, MEMORY_PAGING_PAGE_TYPE_2M);
	memory_paging_add_page(p4, ((uint64_t)0x3) << 30, ((uint64_t)0x12) << 30, MEMORY_PAGING_PAGE_TYPE_1G);
	printf("fill p4 for clone test\n");
	memory_page_table_t* p4_cloned = memory_paging_clone_pagetable_ext(NULL, p4);
	printf("p4 cloned: %p\n", p4_cloned);

	fa = 0;
	if(memory_paging_get_frame_address_ext(p4_cloned, 0x0, &fa) != 0) {
		printf("fa: %04x\n", fa);
		print_error("clone p4 failed, can not get fa");
	} else {
		fa >>= 12;
		printf("fa: %04x\n", fa);
		if(fa != 0x1234) {
			print_error("clone p4 failed, fa not found for 4k");
		} else {
			print_success("4k fa found");
		}
	}

	fa = 0;
	if(memory_paging_get_frame_address_ext(p4_cloned, ((uint32_t)0x3) << 21, &fa) != 0) {
		printf("fa: %04x\n", fa);
		print_error("clone p4 failed, can not get fa");
	} else {
		fa >>= 21;
		printf("fa: %04x\n", fa);
		if(fa != 0x1234) {
			print_error("clone p4 failed, fa not found for 4k");
		} else {
			print_success("2m fa found");
		}
	}

	fa = 0;
	if(memory_paging_get_frame_address_ext(p4_cloned, ((uint64_t)0x3) << 30, &fa) != 0) {
		printf("fa: %04x\n", fa);
		print_error("clone p4 failed, can not get fa");
	} else {
		fa >>= 30;
		printf("fa: %04x\n", fa);
		if(fa != 0x12) {
			print_error("clone p4 failed, fa not found for 4k");
		} else {
			print_success("1g fa found");
		}
	}

	fp = fopen( "tmp/mem2.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );
	fclose(fp);
	return 0;
}
