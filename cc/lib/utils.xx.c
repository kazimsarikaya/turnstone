/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <utils.h>
#include <memory.h>
#include <strings.h>

number_t power(number_t base, number_t p) {
    if (p == 0 ) {
        return 1;
    }
    number_t ret = 1;
    while(p) {
        if(p & 0x1) {
            p--;
            ret *= base;
        } else {
            p /= 2;
            base *= base;
        }
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
