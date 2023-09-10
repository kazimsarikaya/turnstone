/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x800000
#include "setup.h"
#include <linker.h>
#include <strings.h>

int32_t main(int32_t argc, char_t** argv, char_t** envp);
int8_t  linker_relink(program_header_t* program);

typedef int32_t (*_tos_start)(void);

int8_t linker_relink(program_header_t* program) {
    uint64_t program_addr = (uint64_t)program;

    uint64_t reloc_start = program->reloc_start;
    reloc_start += program_addr;
    uint64_t reloc_count = program->reloc_count;

    linker_direct_relocation_t* relocs = (linker_direct_relocation_t*)reloc_start;

    uint8_t* dst_bytes = (uint8_t*)program;

    for(uint64_t i = 0; i < reloc_count; i++) {
        relocs[i].addend += program_addr;

        if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32) {
            uint32_t* target = (uint32_t*)&dst_bytes[relocs[i].offset];

            *target = (uint32_t)relocs[i].addend;
        } else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32S) {
            int32_t* target = (int32_t*)&dst_bytes[relocs[i].offset];

            *target = (int32_t)relocs[i].addend;
        }  else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_64) {
            uint64_t* target = (uint64_t*)&dst_bytes[relocs[i].offset];

            *target = relocs[i].addend;
        } else {
            PRINTLOG(LINKER, LOG_FATAL, "unknown relocation 0x%lli type %i", i, relocs[i].relocation_type);

            return -1;
        }
    }

    linker_global_offset_table_entry_t* got_table = (linker_global_offset_table_entry_t*)(program->section_locations[LINKER_SECTION_TYPE_GOT].section_start + program_addr);

    for(uint64_t i = 0; i < program->got_entry_count; i++) {
        if(got_table[i].entry_value != 0) {
            got_table[i].entry_value += program_addr;
        }
    }

    linker_section_locations_t bss_sl = program->section_locations[LINKER_SECTION_TYPE_BSS];
    uint8_t* dst_bss = (uint8_t*)(bss_sl.section_start + program_addr);

    memory_memclean(dst_bss, bss_sl.section_size);

    return 0;
}

#define CLONE_VM 0x00000100
#define CLONE_FS 0x00000200
#define CLONE_FILES 0x00000400
#define CLONE_PARENT 0x00008000


int64_t clone(uint64_t flags, void* stack,
              int32_t* parent_tid, uint64_t tls,
              int32_t* child_tid);


int64_t clone(uint64_t flags, void* stack,
              int32_t* parent_tid, uint64_t tls,
              int32_t* child_tid) {
    int64_t ret = 0;
    register uint64_t r10 asm ("r10") = tls;
    register int32_t* r8 asm ("r8") = child_tid;

    asm volatile (
        "movq $56, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (flags), "S" (stack), "d" (parent_tid), "r" (r10), "r" (r8)
        );

    return ret;
}

int32_t vfork(void);


int32_t main(int32_t argc, char_t** argv, char_t** envp) {
    if (argc < 2) {
        PRINTLOG(LINKER, LOG_FATAL, "no tosbin file");

        return -1;
    }

    char_t* tosbin_name = argv[1];

    PRINTLOG(LINKER, LOG_INFO, "loading tos binary %s", tosbin_name);

    FILE* tosbin = fopen(tosbin_name, "rb");

    if (tosbin == NULL) {
        PRINTLOG(LINKER, LOG_FATAL, "tosbin file %s not found", tosbin_name);
        return -1;
    }

    fseek(tosbin, 0, SEEK_END);
    uint32_t tosbin_size = ftell(tosbin);

    PRINTLOG(LINKER, LOG_INFO, "tos binary %s size 0x%x", tosbin_name, tosbin_size);

    fseek(tosbin, 0, SEEK_SET);

    program_header_t tmp_program_header;

    fread(&tmp_program_header, 1, sizeof(program_header_t), tosbin);

    uint64_t program_size = tmp_program_header.program_size;

    program_size = (program_size + 4095) & ~4095;

    PRINTLOG(LINKER, LOG_INFO, "program size 0x%x", program_size);

    program_size += tmp_program_header.section_locations[LINKER_SECTION_TYPE_BSS].section_size;

    PRINTLOG(LINKER, LOG_INFO, "program bss size 0x%x", tmp_program_header.section_locations[LINKER_SECTION_TYPE_BSS].section_size);

    program_size = (program_size + 4095) & ~4095;

    PRINTLOG(LINKER, LOG_INFO, "fixed program size 0x%x", program_size);

    fseek(tosbin, 0, SEEK_SET);

    uint64_t program_addr = 1 << 30;

    uint8_t* tosbin_data =  mmap((void*)program_addr, program_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (tosbin_data != (void*)program_addr) {
        PRINTLOG(LINKER, LOG_FATAL, "failed create mmap for tosbin file %s to 0x%llX", tosbin_name, program_addr);
        return -1;
    }

    PRINTLOG(LINKER, LOG_INFO, "loading tos binary %s to address %p", tosbin_name, tosbin_data);

    uint8_t buffer[4096];

    uint32_t r = 0;

    while(r < tosbin_size) {
        uint32_t t_r = fread(buffer, 1, 4096, tosbin);

        if (t_r == 0) {
            break;
        }

        memory_memcopy(buffer, tosbin_data + r, t_r);

        r += t_r;
    }

    PRINTLOG(LINKER, LOG_INFO, "loaded tos binary %s to address %p read size %i", tosbin_name, tosbin_data, r);

    fclose(tosbin);

    uint8_t* stack = mmap(0, 4 << 20, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if(stack == MAP_FAILED) {
        PRINTLOG(LINKER, LOG_FATAL, "failed create mmap for stack 0x%llx", (uint64_t)stack);
        return -1;
    }

    PRINTLOG(LINKER, LOG_INFO, "created stack at address %p size %x", stack, 4 << 20);

    program_header_t* program = (program_header_t*)tosbin_data;

    program->section_locations[LINKER_SECTION_TYPE_STACK].section_start = (uint64_t)stack;
    program->section_locations[LINKER_SECTION_TYPE_STACK].section_size = 4 << 20;

    PRINTLOG(LINKER, LOG_INFO, "relinking tos binary %s to address %p", tosbin_name, program);

    linker_relink(program);

    PRINTLOG(LINKER, LOG_INFO, "entry point 0x%x", program->entry_point_address_pc_relative);

    _tos_start tos_start = (_tos_start)program;

    uint64_t* stack_top = (uint64_t*)(stack + (4 << 20));

    stack_top--;

    char_t** envp_tmp = envp;

    uint64_t envp_size = 0;
    uint64_t envp_count = 0;

    uint8_t* stack_data = (uint8_t*)stack_top;

    while(*envp_tmp != NULL) {
        envp_size += strlen(*envp_tmp) + 1;
        envp_count++;
        envp_tmp++;
    }

    stack_data -= envp_size;
    stack_data -= ((uint64_t)stack_data) & 0x7;

    char_t* envp_data = (char_t*)stack_data;

    stack_top = (uint64_t*)stack_data;

    stack_top--;

    uint64_t argv_size = 0;

    for (int i = 1; i < argc; i++) { // skip argv[0] it is loader
        argv_size += strlen(argv[i]) + 1;
    }

    stack_data -= argv_size;
    stack_data -= ((uint64_t)stack_data) & 0x7;

    char_t* argv_data = (char_t*)stack_data;

    stack_top = (uint64_t*)stack_data;

    stack_top--;

    uint64_t* envp_data_ptr = stack_top;
    envp_data_ptr -= envp_count;

    stack_top = envp_data_ptr;
    stack_top--;

    uint64_t* argv_data_ptr = stack_top;
    argv_data_ptr -= argc - 1;

    stack_top = argv_data_ptr;
    stack_top--;

    stack_top[0] = argc - 1;

    uint64_t envp_index = 0;

    envp_tmp = envp;

    while(envp_index < envp_count) {
        envp_data_ptr[envp_index] = (uint64_t)envp_data;

        memory_memcopy(*envp_tmp, envp_data, strlen(*envp_tmp) + 1);

        envp_data += strlen(*envp_tmp) + 1;

        envp_tmp++;

        envp_index++;
    }

    for (int i = 1; i < argc; i++) { // skip argv[0] it is loader
        argv_data_ptr[i - 1] = (uint64_t)argv_data;

        memory_memcopy(argv[i], argv_data, strlen(argv[i]) + 1);

        argv_data += strlen(argv[i]) + 1;
    }

    PRINTLOG(LINKER, LOG_INFO, "stack top %p", stack_top);

#if 0
    //int32_t ret = clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_PARENT, stack_top, NULL, 0, NULL);
    int32_t ret = vfork();

    if (ret == 0) {
        while(1);
        tos_start();
    } else {
        PRINTLOG(LINKER, LOG_INFO, "tos binary %s started with pid %i", tosbin_name, ret);
        while(1);
    }
#else
    asm volatile (
        "mov %%rdi, %%rsp\n"
        "jmp *%%rax\n"
        :
        : "a" (tos_start), "D" (stack_top)
        );
#endif

    UNUSED(envp);

    return 0;
}
