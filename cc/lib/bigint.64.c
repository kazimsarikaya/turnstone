/**
 * @file bigint.64.c
 * @brief Big integer arithmetic
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <bigint.h>
#include <memory.h>
#include <strings.h>
#include <buffer.h>
#include <logging.h>

MODULE("turnstone.lib");

typedef struct bigint_item_t bigint_item_t;

/*! each value is 48 bits */
#define BIGINT_ITEM_BITS       48
#define BIGINT_ITEM_HEX_DIGITS 12

#define BIGINT_ITEM_MAX   0x0000FFFFFFFFFFFFUL
#define BIGINT_ITEM_MSB   0x0000800000000000UL
#define BIGINT_ITEM_CARRY 0x0001000000000000UL

typedef struct bigint_item_t {
    uint64_t       value;
    bigint_item_t* next;
    bigint_item_t* prev;
} bigint_item_t;

struct bigint_t {
    bigint_item_t* lsb;
    bigint_item_t* msb;
    int32_t        sign;
};


bigint_t* bigint_create(void) {
    bigint_t* bigint = (bigint_t*)memory_malloc(sizeof(bigint_t));

    if (bigint) {
        bigint->lsb = NULL;
        bigint->msb = NULL;
        bigint->sign = 0;
    }

    return bigint;
}

static void bigint_destroy_items(bigint_t* bigint) {
    bigint_item_t* item = bigint->lsb;

    while (item) {
        bigint_item_t* next = item->next;
        memory_free(item);
        item = next;
    }

    bigint->lsb = NULL;
    bigint->msb = NULL;
    bigint->sign = 0;
}

void bigint_destroy(bigint_t* bigint) {
    bigint_destroy_items(bigint);

    memory_free(bigint);
}

int8_t bigint_set_int64(bigint_t* bigint, int64_t value) {
    bigint_item_t* item;
    uint64_t uvalue;
    boolean_t need_next = false;
    uint64_t next_value = 0;

    if (bigint->lsb) {
        bigint_destroy_items(bigint);
    }

    if (value < 0) {
        bigint->sign = -1;
        uvalue = -value;
    } else {
        bigint->sign = 1;
        uvalue = value;
    }

    if (uvalue > BIGINT_ITEM_MAX) {
        need_next = true;
        next_value = uvalue >> BIGINT_ITEM_BITS;
        uvalue &= BIGINT_ITEM_MAX;
    }

    item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

    if (!item) {
        return -1;
    }

    item->value = uvalue;
    item->next = NULL;

    if (need_next) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            memory_free(item);
            return -1;
        }

        next->value = next_value;
        next->prev = item;
        next->next = NULL;

        item->next = next;
    }

    bigint->lsb = item;

    if (need_next) {
        bigint->msb = item->next;
    } else {
        bigint->msb = item;
    }

    return 0;
}

int8_t bigint_set_uint64(bigint_t* bigint, uint64_t value) {
    bigint_item_t* item;
    boolean_t need_next = false;
    uint64_t next_value = 0;

    if (bigint->lsb) {
        bigint_destroy_items(bigint);
    }

    bigint->sign = 1;

    if (value > BIGINT_ITEM_MAX) {
        need_next = true;
        next_value = value >> BIGINT_ITEM_BITS;
        value &= BIGINT_ITEM_MAX;
    }

    item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

    if (!item) {
        return -1;
    }

    item->value = value;
    item->next = NULL;

    if (need_next) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            memory_free(item);
            return -1;
        }

        next->value = next_value;
        next->next = NULL;
        next->prev = item;

        item->next = next;
    }

    bigint->lsb = item;

    if (need_next) {
        bigint->msb = item->next;
    } else {
        bigint->msb = item;
    }

    return 0;
}

int8_t bigint_set_bigint(bigint_t* bigint, const bigint_t* src) {
    bigint_item_t* item = src->lsb;
    bigint_item_t* prev = NULL;
    bigint_item_t* first = NULL;
    bigint_item_t* next = NULL;

    if (bigint->lsb) {
        bigint_destroy_items(bigint);
    }

    bigint->sign = src->sign;

    while (item) {
        next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            return -1;
        }

        next->value = item->value;
        next->next = NULL;

        if (prev) {
            prev->next = next;
        } else {
            first = next;
        }

        next->prev = prev;

        prev = next;
        item = item->next;
    }

    bigint->lsb = first;
    bigint->msb = next;

    return 0;
}

int8_t bigint_set_str(bigint_t* bigint, const char_t* str) {
    if (!str) {
        return -1;
    }

    int32_t sign = 1;

    if (*str == '-') {
        sign = -1;
        str++;
    }

    while (*str && *str == '0') {
        str++;
    }

    uint32_t len = strlen(str);

    if (len == 0) {
        bigint->sign = 0;

        return 0;
    }


    bigint_destroy_items(bigint);

    bigint->sign = sign;

    int32_t parts = len / BIGINT_ITEM_HEX_DIGITS;

    if (len % BIGINT_ITEM_HEX_DIGITS) {
        parts++;
    }

    int32_t first_len = len % BIGINT_ITEM_HEX_DIGITS;

    if (first_len == 0) {
        first_len = BIGINT_ITEM_HEX_DIGITS;
    }

    bigint_item_t* next = NULL;

    for (int32_t i = 0; i < parts; i++) {
        int32_t part_len = (i == 0) ? first_len : BIGINT_ITEM_HEX_DIGITS;

        uint64_t value = 0;

        for (int32_t j = 0; j < part_len; j++) {
            value <<= 4;

            if (str[j] >= '0' && str[+j] <= '9') {
                value += str[j] - '0';
            } else if (str[j] >= 'A' && str[j] <= 'F') {
                value += str[j] - 'A' + 10;
            } else if (str[j] >= 'a' && str[j] <= 'f') {
                value += str[j] - 'a' + 10;
            } else {
                return -1;
            }
        }

        str += part_len;

        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = value;
        item->next = next;

        if (next) {
            next->prev = item;
        }

        next = item;

        if (i == 0) {
            bigint->msb = item;
        }
    }

    bigint->lsb = next;

    return 0;
}

const char_t* bigint_to_str(const bigint_t* bigint) {
    if (!bigint->msb) {
        return strdup("0");
    }

    if(bigint->sign == 0) {
        return strdup("0");
    }


    buffer_t* buffer = buffer_new_with_capacity(NULL, 64);
    bigint_item_t* item = bigint->msb; // msb is at msb

    if (bigint->sign < 0) {
        buffer_append_byte(buffer, '-');
    }

    buffer_printf(buffer, "%llx", item->value); // first item max 48 bits so 12 hex digits and no leading zeros at the beginning

    item = item->prev;

    while (item) {
        buffer_printf(buffer, "%012llx", item->value); // each item max 48 bits so 12 hex digits
        item = item->prev;
    }

    buffer_append_byte(buffer, '\0');

    uint8_t* str = buffer_get_all_bytes_and_destroy(buffer, NULL);

    return (const char_t*)str;
}

boolean_t bigint_is_zero(const bigint_t* a) {
    return a->sign == 0;
}

boolean_t bigint_is_negative(const bigint_t* a) {
    return a->sign < 0;
}

boolean_t bigint_is_odd(const bigint_t* a) {
    if (a->sign == 0) {
        return false;
    }

    return a->lsb->value & 1;
}

int8_t bigint_and(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    bigint_destroy_items(result);

    if (a->sign == 0 || b->sign == 0) {
        result->sign = 0;
        return 0;
    }

    if (a->sign < 0 && b->sign < 0) {
        result->sign = -1;
    } else {
        result->sign = 1;
    }

    bigint_item_t* item_a = a->lsb;
    bigint_item_t* item_b = b->lsb;
    bigint_item_t* prev = NULL;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = item_a->value & item_b->value;
        item->prev = prev;

        if (prev) {
            prev->next = item;
        }

        if(!result->lsb) {
            result->lsb = item;
        }

        prev = item;

        item_a = item_a->next;
        item_b = item_b->next;
    }

    if (item_a) {
        result->msb = item_a;
    } else {
        result->msb = item_b;
    }

    return 0;
}

int8_t bigint_or(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    bigint_destroy_items(result);

    if (a->sign == 0) {
        return bigint_set_bigint(result, b);
    }

    if (b->sign == 0) {
        return bigint_set_bigint(result, a);
    }

    if (a->sign < 0 && b->sign < 0) {
        result->sign = -1;
    } else {
        result->sign = 1;
    }

    bigint_item_t* item_a = a->lsb;
    bigint_item_t* item_b = b->lsb;
    bigint_item_t* prev = NULL;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = item_a->value | item_b->value;
        item->prev = prev;

        if (prev) {
            prev->next = item;
        }

        if(!result->lsb) {
            result->lsb = item;
        }

        prev = item;

        item_a = item_a->next;
        item_b = item_b->next;
    }

    if (item_a) {
        result->msb = item_a;
    } else {
        result->msb = item_b;
    }

    return 0;
}

int8_t bigint_xor(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    bigint_destroy_items(result);

    if (a->sign == 0) {
        return bigint_set_bigint(result, b);
    }

    if (b->sign == 0) {
        return bigint_set_bigint(result, a);
    }

    if (a->sign < 0 && b->sign < 0) {
        result->sign = -1;
    } else {
        result->sign = 1;
    }

    bigint_item_t* item_a = a->lsb;
    bigint_item_t* item_b = b->lsb;
    bigint_item_t* prev = NULL;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = item_a->value ^ item_b->value;
        item->prev = prev;

        if (prev) {
            prev->next = item;
        }

        if(!result->lsb) {
            result->lsb = item;
        }

        prev = item;

        item_a = item_a->next;
        item_b = item_b->next;
    }

    if (item_a) {
        result->msb = item_a;
    } else {
        result->msb = item_b;
    }

    return 0;
}

int8_t bigint_not(bigint_t* result, const bigint_t* a) {
    bigint_destroy_items(result);

    if (a->sign == 0) {
        result->sign = 0;
        return 0;
    }

    result->sign = -a->sign;

    bigint_item_t* item = a->lsb;
    bigint_item_t* prev = NULL;

    while (item) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            return -1;
        }

        next->value = ~item->value;

        next->value &= BIGINT_ITEM_MAX;

        next->prev = prev;

        if (prev) {
            prev->next = next;
        } else {
            result->lsb = next;
        }

        prev = next;

        item = item->next;
    }

    result->msb = prev;

    return 0;
}

int8_t bigint_shl(bigint_t* result, const bigint_t* a, int64_t shift) {
    bigint_destroy_items(result);

    if (a->sign == 0) {
        result->sign = 0;
        return 0;
    }

    if (shift < 0) {
        return bigint_shr(result, a, -shift);
    }

    result->sign = a->sign;

    int64_t item_shift = shift / BIGINT_ITEM_BITS;
    int64_t bit_shift = shift % BIGINT_ITEM_BITS;

    uint64_t carry = 0;

    bigint_item_t* item = a->lsb;
    bigint_item_t* prev = NULL;

    while (item) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            return -1;
        }

        next->value = (item->value << bit_shift) | carry;
        next->value &= BIGINT_ITEM_MAX;

        carry = item->value >> (BIGINT_ITEM_BITS - bit_shift);

        if (prev) {
            prev->next = next;
        } else {
            result->lsb = next;
        }

        next->prev = prev;

        prev = next;

        item = item->next;
    }

    if (carry) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            return -1;
        }

        next->value = carry;

        if (prev) {
            prev->next = next;
        } else {
            result->lsb = next;
        }

        next->prev = prev;

        prev = next;
    }

    result->msb = prev;

    if (item_shift) {
        prev = result->lsb;

        for (int64_t i = 0; i < item_shift; i++) {
            bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!next) {
                return -1;
            }

            next->value = 0;

            prev->prev = next;
            next->next = prev;

            prev = next;
        }

        result->lsb = prev;
    }

    return 0;
}

int8_t bigint_shr(bigint_t* result, const bigint_t* a, int64_t shift) {
    bigint_destroy_items(result);

    if (a->sign == 0) {
        result->sign = 0;
        return 0;
    }

    if (shift < 0) {
        return bigint_shl(result, a, -shift);
    }

    result->sign = a->sign;

    int64_t item_shift = shift / BIGINT_ITEM_BITS;
    int64_t bit_shift = shift % BIGINT_ITEM_BITS;

    bigint_item_t* item = a->msb;
    bigint_item_t* prev = NULL;
    uint64_t carry = 0;

    while (item) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            return -1;
        }

        next->value = carry | item->value >> bit_shift;

        carry = item->value << (BIGINT_ITEM_BITS - bit_shift);
        carry &= BIGINT_ITEM_MAX;

        if (prev) {
            prev->prev = next;
        } else {
            result->msb = next;
        }

        next->next = prev;

        prev = next;

        item = item->prev;
    }

    result->lsb = prev;

    if (item_shift) {
        // remove the first item_shift items
        prev = result->lsb;

        for (int64_t i = 0; i < item_shift; i++) {
            if (!prev) {
                break;
            }

            bigint_item_t* next = prev->next;

            memory_free(prev);

            prev = next;
        }

        result->lsb = prev;

        if (prev) {
            prev->prev = NULL;
        }
    }

    if (result->lsb == NULL) {
        result->msb = NULL;
    }

    // start from msb and remove leading zeros

    item = result->msb;

    while (item && item->value == 0) {
        prev = item->prev;

        if (prev) {
            prev->next = NULL;
        }

        memory_free(item);

        item = prev;
    }

    result->msb = item;

    if (!item) {
        result->sign = 0;
        result->lsb = NULL;
    }

    return 0;
}

int8_t bigint_set_bit(bigint_t* bigint, uint64_t bit, boolean_t value) {
    if (bigint->sign == 0) {
        if (value) {
            bigint->sign = 1;
            bigint->lsb = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!bigint->lsb) {
                return -1;
            }

            bigint->lsb->value = 0;
            bigint->lsb->next = NULL;
            bigint->lsb->prev = NULL;
            bigint->msb = bigint->lsb;
        } else {
            return 0;
        }
    }

    bigint_item_t* item = bigint->lsb;
    uint64_t item_bit = bit % BIGINT_ITEM_BITS;

    for (uint64_t i = 0; i < bit / BIGINT_ITEM_BITS; i++) {
        if (!item) {
            item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item) {
                return -1;
            }

            item->value = 0;
            item->next = NULL;
            item->prev = bigint->msb;

            bigint->msb->next = item;
            bigint->msb = item;
        }

        item = item->next;
    }

    if (!item) {
        item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = 0;
        item->next = NULL;
        item->prev = bigint->msb;

        bigint->msb->next = item;
        bigint->msb = item;
    }

    if (value) {
        item->value |= 1ULL << item_bit;
    } else {
        item->value &= ~(1ULL << item_bit);
    }

    return 0;
}

int8_t bigint_get_bit(const bigint_t* bigint, uint64_t bit, boolean_t* value) {
    if (bigint->sign == 0) {
        *value = false;
        return 0;
    }

    bigint_item_t* item = bigint->lsb;
    uint64_t item_bit = bit % BIGINT_ITEM_BITS;

    for (uint64_t i = 0; i < bit / BIGINT_ITEM_BITS; i++) {
        if (!item) {
            *value = false;
            return 0;
        }

        item = item->next;
    }

    if (!item) {
        *value = false;
        return 0;
    }

    *value = (item->value >> item_bit) & 1;

    return 0;
}

int8_t bigint_flip_bit(bigint_t* bigint, uint64_t bit) {
    if (bigint->sign == 0) {
        bigint->sign = 1;
        bigint->lsb = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!bigint->lsb) {
            return -1;
        }

        bigint->lsb->value = 0;
        bigint->lsb->next = NULL;
        bigint->lsb->prev = NULL;
        bigint->msb = bigint->lsb;
    }

    bigint_item_t* item = bigint->lsb;
    uint64_t item_bit = bit % BIGINT_ITEM_BITS;

    for (uint64_t i = 0; i < bit / BIGINT_ITEM_BITS; i++) {
        if (!item) {
            item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item) {
                return -1;
            }

            item->value = 0;
            item->next = NULL;
            item->prev = bigint->msb;

            bigint->msb->next = item;
            bigint->msb = item;
        }

        item = item->next;
    }

    if (!item) {
        item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = 0;
        item->next = NULL;
        item->prev = bigint->msb;

        bigint->msb->next = item;
        bigint->msb = item;
    }

    item->value ^= 1ULL << item_bit;

    return 0;
}

int8_t bigint_clear_bit(bigint_t* bigint, uint64_t bit) {
    if (bigint->sign == 0) {
        return 0;
    }

    bigint_item_t* item = bigint->lsb;
    uint64_t item_bit = bit % BIGINT_ITEM_BITS;

    for (uint64_t i = 0; i < bit / BIGINT_ITEM_BITS; i++) {
        if (!item) {
            return 0;
        }

        item = item->next;
    }

    if (!item) {
        return 0;
    }

    item->value &= ~(1ULL << item_bit);

    return 0;
}

int8_t bigint_sub(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    // convert to -b
    bigint_t* neg_b = bigint_create();

    if (bigint_set_bigint(neg_b, b) == -1) {
        bigint_destroy(neg_b);
        return -1;
    }

    neg_b->sign = -neg_b->sign;

    // add a and -b
    int8_t ret = bigint_add(result, a, neg_b);

    bigint_destroy(neg_b);

    return ret;
}

int8_t bigint_add(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    bigint_destroy_items(result);

    if (a->sign == 0) {
        return bigint_set_bigint(result, b);
    }

    if (b->sign == 0) {
        return bigint_set_bigint(result, a);
    }

    if (a->sign < 0 && b->sign < 0) {
        result->sign = -1;
    } else {
        result->sign = 1;
    }

    bigint_item_t* item_a = a->lsb;
    bigint_item_t* item_b = b->lsb;
    bigint_item_t* prev = NULL;
    uint64_t carry = 0;

    int32_t res_sign = 0;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        if(b->sign < 0) {
            item->value = item_a->value - item_b->value - carry;

            if(item_a->value < item_b->value + carry) {
                carry = 1;
                res_sign = -1;
            } else {
                carry = 0;
                res_sign = 1;
            }

            item->value &= BIGINT_ITEM_MAX;
        } else {
            item->value = item_a->value + item_b->value + carry;
            carry = item->value >> BIGINT_ITEM_BITS;
            item->value &= BIGINT_ITEM_MAX;
        }

        item->prev = prev;

        if (prev) {
            prev->next = item;
        }

        if(!result->lsb) {
            result->lsb = item;
        }

        prev = item;

        item_a = item_a->next;
        item_b = item_b->next;
    }

    if (item_a) {
        while (item_a) {
            bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item) {
                return -1;
            }

            if(b->sign < 0) {
                item->value = item_a->value - carry;

                if(item_a->value < carry) {
                    carry = 1;
                    res_sign = -1;
                } else {
                    carry = 0;
                    res_sign = 1;
                }

                item->value &= BIGINT_ITEM_MAX;
            } else {
                item->value = item_a->value + carry;
                carry = item->value >> BIGINT_ITEM_BITS;
                item->value &= BIGINT_ITEM_MAX;
            }

            item->prev = prev;

            if (prev) {
                prev->next = item;
            }

            if(!result->lsb) {
                result->lsb = item;
            }

            prev = item;

            item_a = item_a->next;
        }
    } else if (item_b) {
        while (item_b) {
            bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item) {
                return -1;
            }

            if(b->sign < 0) {
                item->value = item_b->value - carry;

                if(item_b->value < carry) {
                    carry = 1;
                    res_sign = -1;
                } else {
                    carry = 0;
                    res_sign = 1;
                }

                item->value &= BIGINT_ITEM_MAX;
            } else {
                item->value = item_b->value + carry;
                carry = item->value >> BIGINT_ITEM_BITS;
                item->value &= BIGINT_ITEM_MAX;
            }

            item->prev = prev;

            if (prev) {
                prev->next = item;
            }

            if(!result->lsb) {
                result->lsb = item;
            }

            prev = item;

            item_b = item_b->next;



        }
    }

    result->msb = prev;

    if (carry) {
        if(res_sign < 0) {
            // carry is 1 so result is negative

            bigint_t* complement = bigint_create();

            if(bigint_not(complement, result) == -1) {
                bigint_destroy(complement);
                return -1;
            }

            bigint_t* one = bigint_create();

            if(bigint_set_int64(one, 1) == -1) {
                bigint_destroy(complement);
                bigint_destroy(one);
                return -1;
            }

            if(bigint_add(result, complement, one) == -1) {
                bigint_destroy(complement);
                bigint_destroy(one);
                return -1;
            }

            bigint_destroy(complement);
            bigint_destroy(one);

            result->sign = -1;
        } else {
            bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item) {
                return -1;
            }

            item->value = carry;

            if (prev) {
                prev->next = item;
            } else {
                result->lsb = item;
            }

            item->prev = prev;

            prev = item;

            result->msb = prev;
        }
    }


    // start from msb and remove leading zeros

    bigint_item_t* item = result->msb;

    while (item && item->value == 0) {
        prev = item->prev;

        if(prev) {
            prev->next = NULL;
        }

        memory_free(item);

        item = prev;
    }

    result->msb = item;

    if (!item) {
        result->sign = 0;
        result->lsb = NULL;
    }

    return 0;
}






































































