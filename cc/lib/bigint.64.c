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
#include <utils.h>
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
    if(!bigint) {
        return;
    }

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

static uint64_t bigint_item_count(const bigint_t* bigint) {
    uint64_t count = 0;
    bigint_item_t* item = bigint->lsb;

    while (item) {
        count++;
        item = item->next;
    }

    return count;
}

int8_t bigint_set_int64(bigint_t* bigint, int64_t value) {
    if(!bigint) {
        return -1;
    }

    bigint_item_t* item;
    uint64_t uvalue;
    boolean_t need_next = false;
    uint64_t next_value = 0;

    bigint_destroy_items(bigint);

    if(value == 0) {
        return 0;
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

    bigint_destroy_items(bigint);

    if (value == 0) {
        return 0;
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

bigint_t* bigint_one(void) {
    bigint_t* bigint = bigint_create();

    if (bigint) {
        bigint_set_int64(bigint, 1);
    }

    return bigint;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t bigint_set_bigint(bigint_t* bigint, const bigint_t* src) {
    if(!bigint || !src) {
        return -1;
    }

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
#pragma GCC diagnostic pop

int8_t bigint_set_str(bigint_t* bigint, const char_t* str) {
    if (!str || !bigint) {
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
    if (!bigint) {
        return NULL;
    }

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
    if (!a) {
        return true;
    }

    return a->sign == 0;
}

boolean_t bigint_is_negative(const bigint_t* a) {
    if (!a) {
        return false;
    }

    return a->sign < 0;
}

boolean_t bigint_is_odd(const bigint_t* a) {
    if (!a) {
        return false;
    }

    if (a->sign == 0) {
        return false;
    }

    return a->lsb->value & 1;
}

static int8_t bigint_sign_extend(bigint_t* bigint, uint64_t item_count) {
    if (bigint->sign == 0) {
        return 0;
    }

    uint64_t count = bigint_item_count(bigint);

    if (count >= item_count) {
        return 0;
    }

    item_count -= count; // how many items to add

    for (uint64_t i = 0; i < item_count; i++) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = (bigint->sign < 0) ? BIGINT_ITEM_MAX : 0;
        item->next = NULL;
        item->prev = bigint->msb;

        bigint->msb->next = item;
        bigint->msb = item;
    }

    return 0;
}

int8_t bigint_neg(bigint_t* result, const bigint_t* a) {
    if(!result || !a) {
        return -1;
    }

    if (result == a) {
        result->sign = -result->sign; // just change the sign

        return 0;
    }

    if (a->sign == 0) {
        result->sign = 0;

        return 0;
    }

    if(result == a) {
        result->sign = -result->sign;
        return 0;
    }

    bigint_destroy_items(result);

    if (bigint_set_bigint(result, a) == -1) {
        return -1;
    }

    result->sign = -result->sign;

    return 0;
}

static int8_t bigint_inc_with_overflow(bigint_t* bigint) {
    if (bigint->sign == 0) {
        bigint->sign = 1;
        bigint->lsb = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!bigint->lsb) {
            return -1;
        }

        bigint->lsb->value = 1;
        bigint->lsb->next = NULL;
        bigint->lsb->prev = NULL;
        bigint->msb = bigint->lsb;

        return 0;
    }

    bigint_item_t* item = bigint->lsb;
    uint64_t carry = 1;

    while (item) {
        item->value += carry;

        carry = item->value >> BIGINT_ITEM_BITS;

        item->value &= BIGINT_ITEM_MAX;

        if (carry == 0) {
            break;
        }

        item = item->next;
    }


    return 0;
}

static int8_t bigint_neg_with_sign(bigint_t* result, const bigint_t* a) {
    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        result->sign = 0;

        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, result);
            bigint_destroy(result);
        }

        return 0;
    }

    if (bigint_set_bigint(result, a) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if(bigint_not(result, result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if(bigint_inc_with_overflow(result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

static int8_t bigint_remove_leading_zeros(bigint_t* bigint) {
    bigint_item_t* item = bigint->msb;

    while (item && item->value == 0) {
        bigint_item_t* prev = item->prev;

        if (prev) {
            prev->next = NULL;
        }

        memory_free(item);

        item = prev;
    }

    bigint->msb = item;

    if (!item) {
        bigint->sign = 0;
        bigint->lsb = NULL;
    }

    return 0;
}

int8_t bigint_and(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0 || b->sign == 0) {
        result->sign = 0;

        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, result);
            bigint_destroy(result);
        }

        return 0;
    }

    if (a->sign < 0 && b->sign < 0) {
        result->sign = -1;
    } else {
        result->sign = 1;
    }

    bigint_t* tmp_a = bigint_create();

    if(!tmp_a) {
        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_a, a);

    bigint_t* tmp_b = bigint_create();

    if(!tmp_b) {
        bigint_destroy(tmp_a);

        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_b, b);

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b));

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
    bigint_item_t* prev = NULL;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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

    result->msb = prev;

    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);

    if(result->sign < 0) {
        if(bigint_neg_with_sign(result, result) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }

        result->sign = -1;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_or(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return bigint_set_bigint(orig_result, b);
    }

    if (b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);

            return 0; // result is already a
        }

        return bigint_set_bigint(orig_result, a);
    }

    if (a->sign > 0 && b->sign > 0) {
        result->sign = 1;
    } else {
        result->sign = -1;
    }

    bigint_t* tmp_a = bigint_create();

    if(!tmp_a) {
        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_a, a);

    bigint_t* tmp_b = bigint_create();

    if(!tmp_b) {
        bigint_destroy(tmp_a);

        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_b, b);

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b));

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
    bigint_item_t* prev = NULL;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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

    result->msb = prev;

    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);

    if(result->sign < 0) {
        if(bigint_neg_with_sign(result, result) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }

        result->sign = -1;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_xor(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return bigint_set_bigint(orig_result, b);
    }

    if (b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);

            return 0; // result is already a
        }

        return bigint_set_bigint(orig_result, a);
    }

    if (a->sign < 0 && b->sign < 0) {
        result->sign = 1;
    } else if (a->sign > 0 && b->sign > 0) {
        result->sign = 1;
    } else {
        result->sign = -1;
    }

    bigint_t* tmp_a = bigint_create();

    if(!tmp_a) {
        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_a, a);

    bigint_t* tmp_b = bigint_create();

    if(!tmp_b) {
        bigint_destroy(tmp_a);

        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_b, b);

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b));

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
    bigint_item_t* prev = NULL;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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

    result->msb = prev;

    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);

    if(result->sign < 0) {
        if(bigint_neg_with_sign(result, result) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }

        result->sign = -1;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_not(bigint_t* result, const bigint_t* a) {
    if(!result || !a) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    result->sign = a->sign;

    bigint_item_t* item = a->lsb;
    bigint_item_t* prev = NULL;

    while (item) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_shl(bigint_t* result, const bigint_t* a, int64_t shift) {
    if(!result || !a) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

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
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                return -1;
            }

            next->value = 0;

            prev->prev = next;
            next->next = prev;

            prev = next;
        }

        result->lsb = prev;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_shr(bigint_t* result, const bigint_t* a, int64_t shift) {
    if(!result || !a) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

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
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

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

    if (bigint_remove_leading_zeros(result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_set_bit(bigint_t* bigint, uint64_t bit, boolean_t value) {
    if(!bigint) {
        return -1;
    }

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
    if(!bigint || !value) {
        return -1;
    }

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
    if(!bigint) {
        return -1;
    }

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
    if(!bigint) {
        return -1;
    }

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
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return bigint_neg(result, b);
    }

    if (b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);

            return 0;
        }

        return bigint_set_bigint(result, a);
    }

    bigint_t* neg_b = bigint_create();

    if (!neg_b) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (bigint_neg(neg_b, b) == -1) {
        bigint_destroy(neg_b);

        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (bigint_add(result, a, neg_b) == -1) {
        bigint_destroy(neg_b);

        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_destroy(neg_b);

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_add(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return bigint_set_bigint(orig_result, b);
    }

    if (b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);

            return 0;
        }

        return bigint_set_bigint(orig_result, a);
    }

    bigint_t* tmp_a = bigint_create();

    if(!tmp_a) {
        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_a, a);

    bigint_t* tmp_b = bigint_create();

    if(!tmp_b) {
        bigint_destroy(tmp_a);

        if(orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_set_bigint(tmp_b, b);

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b)) + 1;

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);

        return -1;
    }

    int32_t sign = 1;

    if (a->sign < 0 && b->sign < 0) {
        sign = -1;
    } else if(a->sign < 0 || b->sign < 0) { // one of them is negative
        sign = -2; // -2 means we need to check the carry
    }

    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
    bigint_item_t* prev = NULL;
    uint64_t carry = 0;

    while (item_a && item_b) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }

        item->value = item_a->value + item_b->value + carry;
        carry = item->value >> BIGINT_ITEM_BITS;
        item->value &= BIGINT_ITEM_MAX;

        item->prev = prev;

        if (prev) {
            prev->next = item;
        } else {
            result->lsb = item;
        }

        prev = item;

        item_a = item_a->next;
        item_b = item_b->next;
    }

    result->msb = prev;

    if (carry) {
        if(sign == -2) {
            result->sign = 1;
        } else {
            if(sign == 1) {
                bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

                if (!item) {
                    if (orig_result_is_a) {
                        bigint_destroy(result);
                    }

                    bigint_destroy(tmp_a);
                    bigint_destroy(tmp_b);

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

            if(sign == -1) {
                result->sign = -1;

                if(bigint_neg_with_sign(result, result) == -1) {
                    if (orig_result_is_a) {
                        bigint_destroy(result);
                    }

                    bigint_destroy(tmp_a);
                    bigint_destroy(tmp_b);

                    return -1;
                }
            }

            result->sign = sign;
        }
    } else if(sign != -2) {
        result->sign = sign;
    } else {
        result->sign = -1;

        if(bigint_neg_with_sign(result, result) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);

            return -1;
        }

        result->sign = -1;
    }

    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);


    if (bigint_remove_leading_zeros(result) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

uint64_t bigint_bit_length(const bigint_t* bigint) {
    if(!bigint) {
        return 0;
    }

    if (bigint->sign == 0) {
        return 0;
    }

    if(bigint->lsb == NULL) {
        return 0;
    }

    uint64_t length = 0;

    bigint_item_t* item = bigint->lsb;

    while (item->next) {
        length += BIGINT_ITEM_BITS;

        item = item->next;
    }

    length += bit_most_significant(item->value);

    return length;

}

int8_t bigint_cmp(const bigint_t* a, const bigint_t* b) {
    if(!a || !b) {
        return -1;
    }

    if (a->sign < b->sign) {
        return -1;
    }

    if (a->sign > b->sign) {
        return 1;
    }

    if (a->sign == 0) {
        return 0;
    }

    uint64_t a_length = bigint_bit_length(a);
    uint64_t b_length = bigint_bit_length(b);

    if (a_length < b_length) {
        return -a->sign;
    }

    if (a_length > b_length) {
        return a->sign;
    }

    bigint_item_t* item_a = a->msb;
    bigint_item_t* item_b = b->msb;

    while (item_a && item_b) {
        if (item_a->value < item_b->value) {
            return -a->sign;
        }

        if (item_a->value > item_b->value) {
            return a->sign;
        }

        item_a = item_a->prev;
        item_b = item_b->prev;
    }

    return 0;
}

int8_t bigint_mul(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (a->sign == 0 || b->sign == 0) {
        result->sign = 0;

        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, result);
            bigint_destroy(result);
        }

        return 0;
    }

    bigint_item_t* item_a = a->msb;
    bigint_item_t* item_b = b->lsb;

    while (item_a) {
        bigint_t* row_result = bigint_create();

        if (!row_result) {
            return -1;
        }

        row_result->sign = a->sign * b->sign;

        bigint_item_t* item = row_result->lsb;
        bigint_item_t* prev_item = NULL;
        uint128_t carry = 0;

        while (item_b) {
            if (!item) {
                item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

                if (!item) {
                    if (orig_result_is_a) {
                        bigint_destroy(result);
                    }

                    bigint_destroy(row_result);
                    return -1;
                }

                item->next = NULL;
                item->prev = prev_item;

                if (prev_item) {
                    prev_item->next = item;
                } else {
                    row_result->lsb = item;
                }

                row_result->msb = item;
            }

            uint128_t value = (uint128_t)item_a->value * (uint128_t)item_b->value + carry;

            item->value = value & BIGINT_ITEM_MAX;

            carry = value >> BIGINT_ITEM_BITS;

            prev_item = item;
            item = item->next;

            item_b = item_b->next;

        } // while (item_b)

        row_result->msb = prev_item; // msb is at msb

        if (carry) {
            item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                return -1;
            }

            item->value = carry;

            item->next = NULL;
            item->prev = prev_item;

            if (prev_item) {
                prev_item->next = item;
            } else {
                row_result->lsb = item;
            }

            row_result->msb = item;
        }

        item_a = item_a->prev;
        item_b = b->lsb;

        bigint_t* tmp_result = bigint_create();

        if(bigint_shl(tmp_result, result, BIGINT_ITEM_BITS) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(row_result);
            bigint_destroy(tmp_result);
            return -1;
        }

        if(bigint_add(result, tmp_result, row_result) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(row_result);
            bigint_destroy(tmp_result);
            return -1;
        }

        bigint_destroy(tmp_result);
        bigint_destroy(row_result);
    }

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_pow(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if(a->sign == 0 && b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (a->sign == 0) {
        result->sign = 0;

        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, result);
            bigint_destroy(result);
        }

        return 0;
    }

    if (b->sign == 0) {
        result->sign = 1;
        bigint_set_int64(result, 1);

        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, result);
            bigint_destroy(result);
        }

        return 0;
    }

    if (b->sign < 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_t* base = bigint_create();

    if (bigint_set_bigint(base, a) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        return -1;
    }

    bigint_t* exp = bigint_create();

    if (bigint_set_bigint(exp, b) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        return -1;
    }

    bigint_t* one = bigint_one();

    if(!one) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        return -1;
    }

    bigint_t* zero = bigint_create();

    if (bigint_set_bigint(result, base) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        bigint_destroy(one);
        bigint_destroy(zero);
        return -1;
    }

    bigint_sub(exp, exp, one);

    while (exp->sign > 0) {
        if (bigint_mul(result, result, base) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(base);
            bigint_destroy(exp);
            bigint_destroy(one);
            bigint_destroy(zero);
            return -1;
        }

        if(bigint_sub(exp, exp, one) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(base);
            bigint_destroy(exp);
            bigint_destroy(one);
            bigint_destroy(zero);
            return -1;
        }
    }

    bigint_destroy(base);
    bigint_destroy(exp);
    bigint_destroy(one);
    bigint_destroy(zero);

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_div_with_remainder(bigint_t* result, bigint_t* remainder, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) { // remainder can be NULL
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (a->sign == 0) {
        result->sign = 0;

        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, result);
            bigint_destroy(result);
        }

        return 0;
    }

    if(orig_result_is_a) {
        bigint_destroy(result);
        result = NULL;
    }

    if (b->sign == 1 && a->sign == 1) {
        return bigint_div_unsigned(orig_result, remainder, a, b);
    }

    if (b->sign == -1 && a->sign == -1) {

        bigint_t* neg_a = bigint_create();

        if (bigint_neg(neg_a, a) == -1) {
            bigint_destroy(neg_a);
            return -1;
        }

        bigint_t* neg_b = bigint_create();

        if (bigint_neg(neg_b, b) == -1) {
            bigint_destroy(neg_a);
            bigint_destroy(neg_b);
            return -1;
        }

        int8_t ret = bigint_div_unsigned(orig_result, remainder, neg_a, neg_b);

        orig_result->sign = 1;

        bigint_destroy(neg_a);
        bigint_destroy(neg_b);

        return ret;
    }

    bigint_t* neg_a = bigint_create();

    if(a->sign != -1) {
        if (bigint_set_bigint(neg_a, a) == -1) {
            bigint_destroy(neg_a);
            return -1;
        }

        neg_a->sign = 1;
    } else {
        if (bigint_neg(neg_a, a) == -1) {
            bigint_destroy(neg_a);
            return -1;
        }
    }

    bigint_t* neg_b = bigint_create();

    if(b->sign != -1) {
        if (bigint_set_bigint(neg_b, b) == -1) {
            bigint_destroy(neg_a);
            bigint_destroy(neg_b);
            return -1;
        }

        neg_b->sign = 1;
    } else {
        if (bigint_neg(neg_b, b) == -1) {
            bigint_destroy(neg_a);
            bigint_destroy(neg_b);
            return -1;
        }
    }

    int8_t ret = bigint_div_unsigned(orig_result, remainder, neg_a, neg_b);

    if (ret == -1) {
        bigint_destroy(neg_a);
        bigint_destroy(neg_b);
        return -1;
    }

    bigint_t* one = bigint_one();

    if(!one) {
        bigint_destroy(neg_a);
        bigint_destroy(neg_b);
        return -1;
    }

    if(bigint_add(orig_result, orig_result, one) == -1) {
        bigint_destroy(neg_a);
        bigint_destroy(neg_b);
        bigint_destroy(one);
        return -1;
    }

    bigint_destroy(one);

    orig_result->sign = -1;

    bigint_destroy(neg_a);
    bigint_destroy(neg_b);

    return ret;
}

int8_t bigint_div_unsigned(bigint_t* result, bigint_t* remainder, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) { // remainder can be NULL
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    if (b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (a->sign == 0 && b->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (a->sign == 0) {
        if (orig_result_is_a) {
            bigint_destroy_items(orig_result);
            bigint_destroy(result);
        }

        return 0;
    }

    bigint_t* denom = bigint_create();

    if (bigint_set_bigint(denom, b) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(denom);
        return -1;
    }

    bigint_t* current = bigint_one();

    if (!current) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(denom);
        bigint_destroy(current);
        return -1;
    }

    bigint_t* tmp = bigint_create();

    if(!tmp) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(denom);
        bigint_destroy(current);
        return -1;
    }

    if(bigint_set_bigint(tmp, a) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(denom);
        bigint_destroy(current);
        bigint_destroy(tmp);
        return -1;
    }

    while (bigint_cmp(denom, a) != 1) {
        if (bigint_shl(current, current, 1) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(denom);
            bigint_destroy(current);
            bigint_destroy(tmp);
            return -1;
        }

        if (bigint_shl(denom, denom, 1) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(denom);
            bigint_destroy(current);
            bigint_destroy(tmp);
            return -1;
        }
    }

    while(!bigint_is_zero(current)) {
        if (bigint_cmp(tmp, denom) != -1) {
            if(bigint_sub(tmp, tmp, denom) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(denom);
                bigint_destroy(current);
                bigint_destroy(tmp);
                return -1;
            }

            if(bigint_or(result, result, current) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(denom);
                bigint_destroy(current);
                bigint_destroy(tmp);
                return -1;
            }
        }

        if (bigint_shr(current, current, 1) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(denom);
            bigint_destroy(current);
            bigint_destroy(tmp);
            return -1;
        }

        if (bigint_shr(denom, denom, 1) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(denom);
            bigint_destroy(current);
            bigint_destroy(tmp);
            return -1;
        }
    }

    bigint_destroy(denom);
    bigint_destroy(current);

    if(remainder) {
        bigint_remove_leading_zeros(tmp);
        bigint_set_bigint(remainder, tmp);
    }

    bigint_destroy(tmp);

    bigint_remove_leading_zeros(result);

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_div(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    return bigint_div_with_remainder(result, NULL, a, b);
}

int8_t bigint_mod(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* tmp_result = bigint_create();
    bigint_t* tmp_remainder = bigint_create();

    if (!tmp_result || !tmp_remainder) {
        bigint_destroy(tmp_result);
        bigint_destroy(tmp_remainder);
        return -1;
    }

    if (bigint_div_with_remainder(tmp_result, tmp_remainder, a, b) == -1) {
        bigint_destroy(tmp_result);
        bigint_destroy(tmp_remainder);
        return -1;
    }

    if(a->sign > 0 && b->sign > 0) {
        // do nothing
    } else if(a->sign < 0 && b->sign < 0) {
        tmp_remainder->sign = -1;
    } else if(a->sign < 0) {
        if(bigint_sub(tmp_result, b, tmp_remainder) == -1) {
            bigint_destroy(tmp_result);
            bigint_destroy(tmp_remainder);
            return -1;
        }

        if(bigint_set_bigint(tmp_remainder, tmp_result) == -1) {
            bigint_destroy(tmp_result);
            bigint_destroy(tmp_remainder);
            return -1;
        }
    } else {
        if(bigint_add(tmp_remainder, tmp_remainder, b) == -1) {
            bigint_destroy(tmp_result);
            bigint_destroy(tmp_remainder);
            return -1;
        }

        tmp_remainder->sign = -1;
    }

    bigint_destroy(tmp_result);

    bigint_remove_leading_zeros(tmp_remainder);

    if (bigint_set_bigint(result, tmp_remainder) == -1) {
        bigint_destroy(tmp_remainder);
        return -1;
    }

    bigint_destroy(tmp_remainder);

    return 0;
}

int8_t bigint_gcd(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    bigint_t* orig_result = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_items(result);
    }

    int8_t cmp = bigint_cmp(a, b);
    boolean_t a_is_smaller = false;

    if (cmp == 0) {
        if (orig_result_is_a) {
            bigint_set_bigint(orig_result, a);
            bigint_destroy(result);
        }

        return 0;
    } else if (cmp == -1) {
        a_is_smaller = true;
    }

    bigint_t* tmp_a = bigint_create();

    if (!tmp_a) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (bigint_set_bigint(tmp_a, a) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        return -1;
    }

    bigint_t* tmp_b = bigint_create();

    if (!tmp_b) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        return -1;
    }

    if (bigint_set_bigint(tmp_b, b) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);
        return -1;
    }

    bigint_t* tmp = bigint_create();

    if (!tmp) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);
        return -1;
    }

    if(!a_is_smaller) {
        bigint_t* swap = tmp_a;
        tmp_a = tmp_b;
        tmp_b = swap;
    }

    while (tmp_a->sign != 0) {
        if (bigint_mod(tmp, tmp_b, tmp_a) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);
            bigint_destroy(tmp);
            return -1;
        }

        if (bigint_set_bigint(tmp_b, tmp_a) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);
            bigint_destroy(tmp);
            return -1;
        }

        if (bigint_set_bigint(tmp_a, tmp) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_a);
            bigint_destroy(tmp_b);
            bigint_destroy(tmp);
            return -1;
        }

    }

    if (bigint_set_bigint(result, tmp_b) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
        bigint_destroy(tmp_b);
        bigint_destroy(tmp);
        return -1;
    }

    if(result->sign == -1) {
        result->sign = 1;
    }

    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);
    bigint_destroy(tmp);

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}









