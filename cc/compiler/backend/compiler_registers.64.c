/**
 * @file compiler_registers.64.c
 * @brief compiler registers functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler");

const char_t*const compiler_regs[] = {
    "rax",
    "rbx",
    "rcx",
    "rdx",
    "rsi",
    "rdi",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
};

_Static_assert(sizeof(compiler_regs) / sizeof(compiler_regs[0]) == COMPILER_VM_REG_COUNT, "invalid register count");

int8_t compiler_find_free_reg(compiler_t* compiler) {
    for(int32_t i = COMPILER_VM_REG_COUNT - 1; i >= 0; i--) {
        if(compiler->busy_regs[i] == false) {
            compiler->busy_regs[i] = true;
            return i;
        }
    }

    return -1;
}

char_t compiler_get_reg_suffix(uint8_t size) {
    if(size == 1 || size == 8) {
        return 'b';
    } else if(size == 16) {
        return 'w';
    } else if(size == 32) {
        return 'l';
    } else if(size == 64) {
        return 'q';
    }

    return 'X'; // force error
}

const char_t* compiler_cast_reg_to_size(const char_t* reg, uint8_t size) {
    if(strcmp(reg, "rax") == 0) {
        if(size == 1 || size == 64) {
            return "al";
        } else if(size == 16) {
            return "ax";
        } else if(size == 32) {
            return "eax";
        } else if(size == 64) {
            return "rax";
        }
    } else if(strcmp(reg, "rbx") == 0) {
        if(size == 1 || size == 8) {
            return "bl";
        } else if(size == 16) {
            return "bx";
        } else if(size == 32) {
            return "ebx";
        } else if(size == 64) {
            return "rbx";
        }
    } else if(strcmp(reg, "rcx") == 0) {
        if(size == 1 || size == 8) {
            return "cl";
        } else if(size == 16) {
            return "cx";
        } else if(size == 32) {
            return "ecx";
        } else if(size == 64) {
            return "rcx";
        }
    } else if(strcmp(reg, "rdx") == 0) {
        if(size == 1 || size == 8) {
            return "dl";
        } else if(size == 16) {
            return "dx";
        } else if(size == 32) {
            return "edx";
        } else if(size == 64) {
            return "rdx";
        }
    } else if(strcmp(reg, "rsi") == 0) {
        if(size == 1 || size == 8) {
            return "sil";
        } else if(size == 16) {
            return "si";
        } else if(size == 32) {
            return "esi";
        } else if(size == 64) {
            return "rsi";
        }
    } else if(strcmp(reg, "rdi") == 0) {
        if(size == 1 || size == 8) {
            return "dil";
        } else if(size == 16) {
            return "di";
        } else if(size == 32) {
            return "edi";
        } else if(size == 64) {
            return "rdi";
        }
    } else if(strcmp(reg, "r8") == 0) {
        if(size == 1 || size == 8) {
            return "r8b";
        } else if(size == 16) {
            return "r8w";
        } else if(size == 32) {
            return "r8d";
        } else if(size == 64) {
            return "r8";
        }
    } else if(strcmp(reg, "r9") == 0) {
        if(size == 1 || size == 8) {
            return "r9b";
        } else if(size == 16) {
            return "r9w";
        } else if(size == 32) {
            return "r9d";
        } else if(size == 64) {
            return "r9";
        }
    } else if(strcmp(reg, "r10") == 0) {
        if(size == 1 || size == 8) {
            return "r10b";
        } else if(size == 16) {
            return "r10w";
        } else if(size == 32) {
            return "r10d";
        } else if(size == 64) {
            return "r10";
        }
    } else if(strcmp(reg, "r11") == 0) {
        if(size == 1 || size == 8) {
            return "r11b";
        } else if(size == 16) {
            return "r11w";
        } else if(size == 32) {
            return "r11d";
        } else if(size == 64) {
            return "r11";
        }
    } else if(strcmp(reg, "r12") == 0) {
        if(size == 1 || size == 8) {
            return "r12b";
        } else if(size == 16) {
            return "r12w";
        } else if(size == 32) {
            return "r12d";
        } else if(size == 64) {
            return "r12";
        }
    } else if(strcmp(reg, "r13") == 0) {
        if(size == 1 || size == 8) {
            return "r13b";
        } else if(size == 16) {
            return "r13w";
        } else if(size == 32) {
            return "r13d";
        } else if(size == 64) {
            return "r13";
        }
    } else if(strcmp(reg, "r14") == 0) {
        if(size == 1 || size == 8) {
            return "r14b";
        } else if(size == 16) {
            return "r14w";
        } else if(size == 32) {
            return "r14d";
        } else if(size == 64) {
            return "r14";
        }
    } else if(strcmp(reg, "r15") == 0) {
        if(size == 1 || size == 8) {
            return "r15b";
        } else if(size == 16) {
            return "r15w";
        } else if(size == 32) {
            return "r15d";
        } else if(size == 64) {
            return "r15";
        }
    }

    return NULL;
}


