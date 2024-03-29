/**
 * @file utils.xx.c
 * @brief several utility functions implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <utils.h>
#include <memory.h>
#include <strings.h>
#include <random.h>

MODULE("turnstone.lib.utils");

number_t power(number_t base, number_t p) {
    if (p == 0 ) {
        return 1;
    }
    number_t ret = 1;
    while(p) {
        if(p & 0x1) {
            ret *= base;
        }
        p /= 2;
        base *= base;

    }
    return ret;
}

int8_t ito_base_with_buffer(char_t* buffer, number_t number, number_t base) {
    if(buffer == NULL) {
        return -1;
    }

    if(base < 2 || base > 36) {
        return -1;
    }

    if(number == 0) {
        buffer[0] = '0';
        buffer[1] = NULL;
        return 0;
    }

    int8_t sign = 0;

    if(number < 0) {
        buffer[0] = '-';
        sign = 1;
        number *= -1;
    }

    size_t len = 0;
    number_t temp = number;

    while(temp) {
        temp /= base;
        len++;
    }

    size_t i = 1;
    number_t r;

    while(number) {
        r = number % base;
        number /= base;

        if(r < 10) {
            buffer[len - i + sign] = 48 + r;
        } else {
            buffer[len - i + sign] = 55 + r;
        }

        i++;
    }

    buffer[len + sign] = NULL;

    return 0;
}

int8_t uto_base_with_buffer(char_t* buffer, unumber_t number, number_t base) {
    if(buffer == NULL) {
        return -1;
    }

    if(base < 2 || base > 36) {
        return -1;
    }

    if(number == 0) {
        buffer[0] = '0';
        buffer[1] = NULL;
        return 0;
    }

    size_t len = 0;
    unumber_t temp = number;

    while(temp) {
        temp /= base;
        len++;
    }

    size_t i = 1;
    unumber_t r;

    while(number) {
        r = number % base;
        number /= base;

        buffer[len - i] = DIGIT_TO_HEX(r);

        i++;
    }

    buffer[len] = NULL;

    return 0;
}

int8_t fto_base_with_buffer(char_t* buffer, float64_t number, number_t prec, number_t base) {
    if(buffer == NULL) {
        return -1;
    }

    if(base < 2 || base > 36) {
        return -1;
    }

    if(number == 0) {
        buffer[0] = '0';
        buffer[1] = '.';
        buffer[2] = '0';
        buffer[3] = NULL;

        return 0;
    }

    int sign = 0;

    if(number < 0)  {
        sign = 1;
    }

    number_t ival = (number_t)number;

    if(ival == 0) {
        if(sign) {
            buffer[0] = '-';
            buffer[1] = '0';
            buffer[2] = NULL;
        } else {
            buffer[0] = '0';
            buffer[1] = NULL;
        }

    } else {
        if(ito_base_with_buffer(buffer, ival, base) != 0) {
            return -1;
        }
    }

    buffer += strlen(buffer);
    buffer[0] = '.';
    buffer++;

    number -= ival;

    if(sign) {
        number *= -1;
    }

    number_t p = power(base, prec);

    number *= p;

    ival = (number_t)number;

    if(ito_base_with_buffer(buffer, ival, base) != 0) {
        return -1;
    }

    return 0;
}

uint8_t byte_count(const uint64_t num) {
    uint8_t res = sizeof(uint64_t);
    uint64_t tmp_num = BYTE_SWAP64(num);
    uint8_t* blist = (uint8_t*)&tmp_num;

    while(*blist == 0) {
        blist++;
        res--;
    }

    return res;
}

uint64_t __attribute__((noinline, optimize("O0"))) read_memio(uint64_t va, uint8_t size) {
    uint64_t res = 0;

    if(size == 8) {
        res = *(volatile uint8_t*)va;
    } else if(size == 16) {
        res = *(volatile uint16_t*)va;
    } else if(size == 32) {
        res = *(volatile uint32_t*)va;
    } else if(size == 64) {
        res = *(volatile uint64_t*)va;
    }

    return res;
}

void __attribute__((noinline, optimize("O0"))) write_memio(uint64_t va, uint64_t val, uint8_t size) {
    if(size == 8) {
        *(volatile uint8_t*)va = val;
    } else if(size == 16) {
        *(volatile uint16_t*)va = val;
    } else if(size == 32) {
        *(volatile uint32_t*)va = val;
    } else if(size == 64) {
        *(volatile uint64_t*)va = val;
    }
}

boolean_t isalpha(char_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

boolean_t isdigit(char_t c) {
    return c >= '0' && c <= '9';
}

boolean_t isalnum(char_t c) {
    return isalpha(c) || isdigit(c);
}

boolean_t isxdigit(char_t c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

boolean_t islower(char_t c) {
    return c >= 'a' && c <= 'z';
}

boolean_t isupper(char_t c) {
    return c >= 'A' && c <= 'Z';
}

boolean_t isspace(char_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

boolean_t isprint(char_t c) {
    return c >= 0x20 && c <= 0x7E;
}

boolean_t isgraph(char_t c) {
    return c >= 0x21 && c <= 0x7E;
}

boolean_t iscntrl(char_t c) {
    return c <= 0x1F || c == 0x7F;
}

boolean_t ispunct(char_t c) {
    return (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7E);
}

boolean_t isblank(char_t c) {
    return c == ' ' || c == '\t';
}

boolean_t isascii(char_t c) {
    return c >= 0 && ((uint8_t)c) <= 0x7F;
}

boolean_t isalnumw(char_t c) {
    return isalnum(c) || c == '_';
}

const char_t* randstr(uint32_t len) {
    static const char_t charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    if(len > 255) {
        return NULL;
    }

    if(len == 0) {
        return strdup("");
    }

    char_t* str = memory_malloc(len + 1);

    if(str == NULL) {
        return NULL;
    }

    for(uint32_t i = 0; i < len; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }

    str[len] = '\0';

    return str;
}
