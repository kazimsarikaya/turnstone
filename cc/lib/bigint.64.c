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
#include <random.h>

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
    boolean_t      neged_with_sign;
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-after-free"
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
    bigint->neged_with_sign = false;
}
#pragma GCC diagnostic pop

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

bigint_t* bigint_two(void) {
    bigint_t* bigint = bigint_create();

    if (bigint) {
        bigint_set_int64(bigint, 2);
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
    bigint_item_t* dest_item = bigint->lsb;
    bigint_item_t* prev = NULL;
    bigint_item_t* first = NULL;
    bigint_item_t* next = NULL;

    bigint->sign = src->sign;
    bigint->neged_with_sign = src->neged_with_sign;

    while(dest_item && item) { // fill the existing items
        dest_item->value = item->value;

        prev = dest_item;

        dest_item = dest_item->next;
        item = item->next;
    }

    if(!item && !dest_item) {
        // nothing to do
        return 0;
    }

    if(!item && dest_item) {
        bigint->msb = prev;

        if(bigint->msb) {
            bigint->msb->next = NULL;
        }

        // remove the rest of the items from dest
        while(dest_item) {
            next = dest_item->next;
            memory_free(dest_item);
            dest_item = next;
        }

        if(!bigint->msb) {
            bigint->lsb = NULL;
            bigint->sign = 0;
        }

        return 0;
    }

    // there are two cases fisrt dest is already empty or dest is not empty
    // prev is the last item in dest if it is null then dest is empty
    // so we need to set first as new item, otherwise first is the lsb of dest

    if(prev) {
        first = bigint->lsb;
    }

    while(item) {
        next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if(!next) {
            return -1;
        }

        next->value = item->value;
        next->prev = prev;
        next->next = NULL;

        if(prev) {
            prev->next = next;
        } else {
            first = next;
        }

        prev = next;
        item = item->next;
    }

    bigint->lsb = first;
    bigint->msb = prev;


    return 0;
}
#pragma GCC diagnostic pop

bigint_t* bigint_clone(const bigint_t* src) {
    if(!src) {
        return NULL;
    }

    bigint_t* bigint = bigint_create();

    if (bigint) {
        if (bigint_set_bigint(bigint, src) == -1) {
            bigint_destroy(bigint);
            bigint = NULL;
        }
    }

    return bigint;
}

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
        bigint->neged_with_sign = false;
    }

    if(bigint->sign == -1 && bigint->neged_with_sign) {
        item = bigint->msb;

        while (item && item->value == BIGINT_ITEM_MAX && item != bigint->lsb) {
            bigint_item_t* prev = item->prev;

            if (prev) {
                prev->next = NULL;
            }

            memory_free(item);

            item = prev;
        }

        if(item == bigint->lsb) {
            item->next = NULL;
            item->prev = NULL;
        }

        bigint->msb = item;

        if (!item) {
            bigint->sign = 0;
            bigint->neged_with_sign = false;
            bigint->lsb = NULL;
        }
    }

    return 0;
}

static int8_t bigint_neg_with_sign_inplace(bigint_t* a, boolean_t force) {
    if(a->sign == 0) {
        return 0;
    }

    if(!force && a->sign > 0) { // prevent positive to negative
        return 0;
    }

    if(!force && a->neged_with_sign && a->sign < 0) { // already neged
        return 0;
    }

    if(bigint_not(a, a) == -1) {
        return -1;
    }

    if(bigint_inc_with_overflow(a) == -1) {
        return -1;
    }

    a->neged_with_sign = !a->neged_with_sign;

    return 0;
}

static int8_t bigint_neg_with_sign(bigint_t* result, const bigint_t* a, boolean_t force) {
    if (result == a) {
        return bigint_neg_with_sign_inplace(result, force);
    }

    if (a->sign == 0) {
        result->sign = 0;

        return 0;
    }

    if(!force && a->sign > 0) { // prevent positive to negative
        return 0;
    }

    if(!force && a->neged_with_sign && a->sign < 0) { // already neged
        return 0;
    }

    if (bigint_set_bigint(result, a) == -1) {
        return -1;
    }

    if(bigint_not(result, result) == -1) {
        return -1;
    }

    if(bigint_inc_with_overflow(result) == -1) {
        return -1;
    }

    result->neged_with_sign = !result->neged_with_sign;

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

        if(bigint->neged_with_sign) {
            bigint_neg_with_sign_inplace((bigint_t*)bigint, true);
        }

    }

    while (item && item->value == 0) {
        item = item->prev;
    }

    if(!item) {
        buffer_destroy(buffer);
        return strdup("0");
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

boolean_t bigint_is_even(const bigint_t* a) {
    if (!a) {
        return false;
    }

    if (a->sign == 0) {
        return true;
    }

    if(!a->lsb) {
        return false;
    }

    return (a->lsb->value & 1) == 0;
}

boolean_t bigint_is_int64(const bigint_t* a, int64_t value) {
    if (!a) {
        return false;
    }

    if (a->sign == 0) {
        return value == 0;
    }

    if (a->sign < 0) {
        if (value > 0) {
            return false;
        }
    }

    if(a->sign > 0) {
        if(value < 0) {
            return false;
        }
    }

    uint64_t uvalue = (value < 0) ? -value : value;
    uint64_t low = uvalue & BIGINT_ITEM_MAX;
    uint64_t high = uvalue >> BIGINT_ITEM_BITS;

    uint64_t item_count = bigint_item_count(a);

    if(item_count == 0) {
        return false;
    }

    if (a->lsb->value != low) {
        return false;
    }

    if (high == 0) {
        return item_count == 1;
    }

    if (item_count != 2) {
        return false;
    }

    bigint_item_t* item = a->lsb->next;

    return item->value == high;
}

boolean_t bigint_is_uint64(const bigint_t* a, uint64_t value) {
    if (!a) {
        return false;
    }

    if (a->sign == 0) {
        return value == 0;
    }

    uint64_t low = value & BIGINT_ITEM_MAX;
    uint64_t high = value >> BIGINT_ITEM_BITS;

    uint64_t bit_count = bigint_bit_length(a);

    if(bit_count > 63) {
        return false;
    }

    if (a->lsb->value != low) {
        return false;
    }

    if (high == 0) {
        return bit_count <= 47;
    }

    bigint_item_t* item = a->lsb->next;

    return item->value == high;
}

static int8_t bigint_sign_extend(bigint_t* bigint, uint64_t item_count) {
    if(!bigint) {
        return -1;
    }

    uint64_t count = bigint_item_count(bigint);

    if (count >= item_count) {
        return 0;
    }

    if(bigint->sign < 0 && !bigint->neged_with_sign && bigint_neg_with_sign_inplace(bigint, false) == -1) {
        return -1;
    }

    item_count -= count; // how many items to add

    for (uint64_t i = 0; i < item_count; i++) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            return -1;
        }

        item->value = (bigint->sign < 0) ? BIGINT_ITEM_MAX : 0;
        item->next = NULL;

        if(bigint->msb) {
            item->prev = bigint->msb;

            bigint->msb->next = item;
            bigint->msb = item;
        } else {
            bigint->lsb = item;
            bigint->msb = item;
        }

    }

    return 0;
}

int8_t bigint_neg(bigint_t* result, const bigint_t* a) {
    if(!result || !a) {
        return -1;
    }

    if(a->sign < 0 && a->neged_with_sign){
        if(bigint_neg_with_sign_inplace((bigint_t*)a, true) == -1) {
            return -1;
        }
    }

    if (result == a) {
        result->sign = -result->sign; // just change the sign

        return 0;
    }

    // bigint_destroy_items(result);

    if (a->sign == 0) {
        return 0;
    }


    if (bigint_set_bigint(result, a) == -1) {
        return -1;
    }

    result->sign = -result->sign;

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

    bigint_t* tmp_a = (bigint_t*)a;
    bigint_t* tmp_b = (bigint_t*)b;

    if (a->sign < 0) {
        if(bigint_neg_with_sign_inplace(tmp_a, false) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }
    }

    if (b->sign < 0) {
        if(bigint_neg_with_sign_inplace(tmp_b, false) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b));

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

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

    if(result->sign < 0) {
        result->neged_with_sign = true;
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

static int8_t bigint_or_inplace(bigint_t* result, const bigint_t* a) {
    if (result->sign == 0) {
        return bigint_set_bigint(result, a);
    }

    if (a->sign == 0) {
        return bigint_set_bigint(result, a);
    }

    if (result->sign < 0) {
        if(bigint_neg_with_sign(result, result, false) == -1) {
            return -1;
        }
    }

    if (result->sign > 0 && a->sign > 0) {
        result->sign = 1;
    } else {
        result->sign = -1;
    }

    bigint_t* tmp_a = (bigint_t*)a;

    if (a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a, false) == -1) {
            return -1;
        }
    }

    uint64_t item_count_a = bigint_item_count(a);

    uint64_t item_count = MAX(bigint_item_count(result), item_count_a);

    if (bigint_sign_extend(result, item_count) == -1) {
        return -1;
    }

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        return -1;
    }


    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_result = result->lsb;

    while (item_a && item_result) {
        item_result->value |= item_a->value;

        item_a = item_a->next;
        item_result = item_result->next;
    }

    if(result->sign < 0) {
        result->neged_with_sign = true;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        return -1;
    }

    return 0;
}

int8_t bigint_or(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    if (result == a) {
        return bigint_or_inplace(result, b);
    }

    bigint_destroy_items(result);

    if (a->sign == 0) {
        return bigint_set_bigint(result, b);
    }

    if (b->sign == 0) {
        return bigint_set_bigint(result, a);
    }

    if (a->sign > 0 && b->sign > 0) {
        result->sign = 1;
    } else {
        result->sign = -1;
    }

    bigint_t* tmp_a = (bigint_t*)a;
    bigint_t* tmp_b = (bigint_t*)b;

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a, false) == -1) {
            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b, false) == -1) {
            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b));

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        return -1;
    }

    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
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

    result->msb = prev;

    if(result->sign < 0) {
        result->neged_with_sign = true;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        return -1;
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

    bigint_t* tmp_a = (bigint_t*)a;
    bigint_t* tmp_b = (bigint_t*)b;

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a, false) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b, false) == -1) {
            if(orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b));

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

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

    if(result->sign < 0) {
        result->neged_with_sign = true;
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

static int8_t bigint_not_inplace(bigint_t* a) {
    if(!a) {
        return -1;
    }

    if (a->sign == 0) {
        return 0;
    }

    bigint_item_t* item = a->lsb;

    while (item) {
        item->value = ~item->value;
        item->value &= BIGINT_ITEM_MAX;

        item = item->next;
    }

    return 0;
}

int8_t bigint_not(bigint_t* result, const bigint_t* a) {
    if(!result || !a) {
        return -1;
    }

    if(result == a) {
        return bigint_not_inplace(result);
    }

    bigint_destroy_items(result);

    result->sign = a->sign;

    bigint_item_t* item = a->lsb;
    bigint_item_t* prev = NULL;

    while (item) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

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

int8_t bigint_shl_one(bigint_t* a) {
    if(!a) {
        return -1;
    }

    if(a->sign == 0) {
        return 0;
    }

    bigint_item_t* item = a->lsb;
    uint64_t carry = 0;

    while (item) {
        uint64_t next_carry = item->value >> (BIGINT_ITEM_BITS - 1);

        item->value <<= 1;
        item->value |= carry;

        carry = next_carry;

        item->value &= BIGINT_ITEM_MAX;

        item = item->next;
    }

    if (carry) {
        bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!next) {
            return -1;
        }

        if(a->sign < 0 && a->neged_with_sign) {
            next->value = BIGINT_ITEM_MAX;
        } else {
            next->value = carry;
        }

        next->next = NULL;
        next->prev = a->msb;

        a->msb->next = next;
        a->msb = next;
    }

    return 0;
}

int8_t bigint_shr_one(bigint_t* a) {
    if(!a) {
        return -1;
    }

    if(a->sign == 0) {
        return 0;
    }

    bigint_item_t* item = a->msb;
    uint64_t carry = (a->sign < 0 && a->neged_with_sign) ? BIGINT_ITEM_MSB : 0;

    while (item) {
        uint64_t next_carry = item->value & 1;

        item->value = carry | (item->value >> 1);

        carry = next_carry << (BIGINT_ITEM_BITS - 1);

        item = item->prev;
    }

    bigint_remove_leading_zeros(a);

    return 0;
}

int8_t bigint_shl(bigint_t* result, const bigint_t* a, int64_t shift) {
    if(!result || !a) {
        return -1;
    }

    if (a->sign == 0) {
        return 0;
    }

    if (shift < 0) {
        return bigint_shr(result, a, -shift);
    }

    result->sign = a->sign;
    result->neged_with_sign = a->neged_with_sign;

    int64_t item_shift = shift / BIGINT_ITEM_BITS;
    int64_t bit_shift = shift % BIGINT_ITEM_BITS;

    uint64_t result_bit_count = bigint_bit_length(a) + bit_shift + 1;

    uint64_t result_item_count = result_bit_count / BIGINT_ITEM_BITS;

    if (result_bit_count % BIGINT_ITEM_BITS) {
        result_item_count++;
    }

    if (bigint_sign_extend(result, result_item_count) == -1) {
        return -1;
    }

    uint64_t carry = 0;

    bigint_item_t* item = a->lsb;
    bigint_item_t* item_result = result->lsb;

    while (item) {
        uint64_t next_carry = item->value >> (BIGINT_ITEM_BITS - bit_shift);

        item_result->value = (item->value << bit_shift) | carry;
        item_result->value &= BIGINT_ITEM_MAX;

        carry = next_carry;

        item = item->next;
        item_result = item_result->next;
    }

    if (carry) {
        if(!item_result) {
            item_result = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

            if (!item_result) {
                return -1;
            }

            item_result->next = NULL;
            item_result->prev = result->msb;

            result->msb->next = item_result;
            result->msb = item_result;
        }

        if(result->sign < 0 && result->neged_with_sign) {
            uint64_t msb_bit_pos = bit_most_significant(carry);
            uint64_t mask = (BIGINT_ITEM_MAX - (BIGINT_ITEM_MAX >> (BIGINT_ITEM_BITS - msb_bit_pos)));
            item_result->value = carry | mask;
        } else {
            item_result->value = carry;
        }

        item_result = item_result->next;
    }

    while (item_result) {
        item_result->value = 0;

        item_result = item_result->next;
    }

    if (item_shift) {
        bigint_item_t* prev = result->lsb;

        for (int64_t i = 0; i < item_shift; i++) {
            bigint_item_t* next = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

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
    result->neged_with_sign = a->neged_with_sign;

    int64_t item_shift = shift / BIGINT_ITEM_BITS;
    int64_t bit_shift = shift % BIGINT_ITEM_BITS;

    bigint_item_t* item = a->msb;
    bigint_item_t* prev = NULL;
    // if a is negative and neged with sign then
    // carry's left bit_shift bits are 1
    uint64_t carry = (a->sign < 0 && a->neged_with_sign) ? (BIGINT_ITEM_MAX - (BIGINT_ITEM_MAX >> bit_shift)) : 0;

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

    if (a->sign == 0) {
        return bigint_neg(result, b);
    }

    if(b->sign == 0) {
        if(a == result) {
            return 0;
        }

        return bigint_set_bigint(result, a);
    }

    uint64_t a_item_count = bigint_item_count(a);
    uint64_t b_item_count = bigint_item_count(b);

    uint64_t result_item_count = MAX(a_item_count, b_item_count);

    bigint_t* tmp_a = (bigint_t*)a;
    bigint_t* tmp_b = (bigint_t*)b;

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a, false) == -1) {
            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b, false) == -1) {
            return -1;
        }
    }

    if (bigint_sign_extend(tmp_a, result_item_count) == -1) {
        return -1;
    }

    if (bigint_sign_extend(tmp_b, result_item_count) == -1) {
        return -1;
    }

    if(a != result) {
        if (bigint_sign_extend(result, result_item_count) == -1) {
            return -1;
        }
    }

    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
    bigint_item_t* item_result = result->lsb;

    uint64_t borrow = 0;

    while (item_a && item_b) {
        uint64_t a_value = item_a->value;
        uint64_t b_value = item_b->value;
        boolean_t next_borrow = false;

        if (borrow) {
            if (a_value == 0) {
                a_value = BIGINT_ITEM_MAX;
                next_borrow = true;
            } else {
                a_value--;
            }
        }

        if (a_value < b_value) {
            borrow = 1;
            a_value += BIGINT_ITEM_MAX + 1;
        } else if (!next_borrow) {
            borrow = 0;
        }

        item_result->value = a_value - b_value;
        item_result->value &= BIGINT_ITEM_MAX;

        item_a = item_a->next;
        item_b = item_b->next;
        item_result = item_result->next;
    }

    if(item_result) {
        printf("do we hit?\n");
        bigint_item_t* tmp = item_result;

        while(tmp) {
            tmp->value = 0;
            tmp = tmp->next;
        }
    }

    if (borrow) {
        result->sign = -1;
        result->neged_with_sign = true;
    } else {
        result->sign = 1;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        return -1;
    }

    return 0;
}

int8_t bigint_add(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if(!result || !a || !b) {
        return -1;
    }

    if (a->sign == 0) {
        return bigint_set_bigint(result, b);
    }

    if (b->sign == 0) {
        if (result == a) {
            return 0;
        }

        return bigint_set_bigint(result, a);
    }

    bigint_t* tmp_a = (bigint_t*)a;
    bigint_t* tmp_b = (bigint_t*)b;

    if (tmp_a->sign < 0) {
        if(bigint_neg_with_sign(tmp_a, tmp_a, false) == -1) {
            return -1;
        }
    }

    if (tmp_b->sign < 0) {
        if(bigint_neg_with_sign(tmp_b, tmp_b, false) == -1) {
            return -1;
        }
    }

    int32_t sign = 1;

    if (a->sign < 0 && b->sign < 0) {
        sign = -1;
    } else if(a->sign < 0 || b->sign < 0) { // one of them is negative
        sign = -2; // -2 means we need to check the carry
    }

    uint64_t item_count = MAX(bigint_item_count(tmp_a), bigint_item_count(tmp_b)) + 1;

    if(result != a) {
        if (bigint_sign_extend(result, item_count) == -1) {
            return -1;
        }

        result->sign = 1;
    }

    if (bigint_sign_extend(tmp_a, item_count) == -1) {
        return -1;
    }

    if (bigint_sign_extend(tmp_b, item_count) == -1) {
        return -1;
    }

    bigint_item_t* item_result = result->lsb;
    bigint_item_t* item_a = tmp_a->lsb;
    bigint_item_t* item_b = tmp_b->lsb;
    bigint_item_t* prev = NULL;
    uint64_t carry = 0;

    while (item_a && item_b) {
        item_result->value = item_a->value + item_b->value + carry;
        carry = item_result->value >> BIGINT_ITEM_BITS;
        item_result->value &= BIGINT_ITEM_MAX;


        prev = item_result;

        item_a = item_a->next;
        item_b = item_b->next;
        item_result = item_result->next;
    }

    if(item_result) {
        bigint_item_t* tmp = item_result;

        while(tmp) {
            tmp->value = 0;
            tmp = tmp->next;
        }
    }

    if (carry) {
        if(sign == -2) {
            result->sign = 1;
            result->neged_with_sign = false;
        } else {
            if(sign == 1) {
                printf("do we hit?\n");
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

            if(sign == -1) {
                result->sign = -1;
                result->neged_with_sign = true;
            }

            result->sign = sign;
        }
    } else if(sign != -2) {
        result->sign = sign;
    } else {
        result->sign = -1;
        result->neged_with_sign = true;
    }

    if (bigint_remove_leading_zeros(result) == -1) {
        return -1;
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-after-free"
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


    bigint_item_t* item = bigint->msb;

    while(item && item->value == 0) {
        item = item->prev;
    }

    if(!item) {
        return 0;
    }

    uint64_t length = bit_most_significant(item->value);

    item = item->prev;

    while (item) {
        length += BIGINT_ITEM_BITS;

        item = item->prev;
    }

    return length;

}
#pragma GCC diagnostic pop

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

    if(a->sign < 0 && a->neged_with_sign) {
        if(bigint_neg_with_sign_inplace((bigint_t*)a, true) == -1) {
            return -1;
        }
    }

    if(b->sign < 0 && b->neged_with_sign) {
        if(bigint_neg_with_sign_inplace((bigint_t*)b, true) == -1) {
            return -1;
        }
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

    while(item_a && item_a->value == 0) { // remove leading zeros
        item_a = item_a->prev;
    }

    bigint_item_t* item_b = b->msb;

    while(item_b && item_b->value == 0) { // remove leading zeros
        item_b = item_b->prev;
    }

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

    if(a->sign < 0 && a->neged_with_sign) {
        if(bigint_neg_with_sign_inplace((bigint_t*)a, true) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }
    }

    if(b->sign < 0 && b->neged_with_sign) {
        if(bigint_neg_with_sign_inplace((bigint_t*)b, true) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            return -1;
        }
    }

    bigint_item_t* item_a = a->msb;
    bigint_item_t* item_b = b->lsb;

    bigint_t* row_result = bigint_create();

    if (!row_result) {
        return -1;
    }

    if(bigint_sign_extend(row_result, bigint_item_count(b) + 1) == -1) {
        bigint_destroy(row_result);

        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    row_result->sign = 1;

    while (item_a) {
        bigint_item_t* item = row_result->lsb;
        uint128_t carry = 0;

        while (item_b) {
            uint128_t value = (uint128_t)item_a->value * (uint128_t)item_b->value + carry;

            item->value = value & BIGINT_ITEM_MAX;

            carry = value >> BIGINT_ITEM_BITS;

            item = item->next;

            item_b = item_b->next;

        } // while (item_b)

        if (carry) {
            item->value = carry;
        } else {
            item->value = 0;
        }

        while(item->next) {
            item = item->next;
            item->value = 0;
        }

        item_a = item_a->prev;
        item_b = b->lsb;

        if(bigint_shl(result, result, BIGINT_ITEM_BITS) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(row_result);
            return -1;
        }

        if(bigint_add(result, result, row_result) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(row_result);
            return -1;
        }
    }

    bigint_destroy(row_result);

    result->sign = a->sign * b->sign;
    result->neged_with_sign = false;

    // bigint_remove_leading_zeros(result);

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

    bigint_t* base = bigint_clone(a);

    if (!base) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        return -1;
    }

    bigint_t* exp = bigint_clone(b);

    if (!exp) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        return -1;
    }

    if (bigint_set_uint64(result, 1) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        return -1;
    }

    while (exp->sign > 0) {
        if(bigint_is_odd(exp)) {
            if (bigint_mul(result, result, base) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(base);
                bigint_destroy(exp);
                return -1;
            }
        }

        if (bigint_mul(base, base, base) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(base);
            bigint_destroy(exp);
            return -1;
        }

        if(bigint_shr_one(exp) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(base);
            bigint_destroy(exp);
            return -1;
        }
    }

    bigint_destroy(base);
    bigint_destroy(exp);

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_mul_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* c) {
    if(!result || !a || !b || !c) {
        return -1;
    }

    if (c->sign == 0) {
        return -1;
    }


    if(a->sign == 0 || b->sign == 0) {
        if(a == result) {
            return 0;
        }

        bigint_destroy_items(result);

        return 0;
    }

    if (bigint_mul(result, a, b) == -1) {
        return -1;
    }

    if (bigint_mod(result, result, c) == -1) {
        return -1;
    }

    return 0;
}

int8_t bigint_pow_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* c) {
    if(!result || !a || !b || !c) {
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

    bigint_t* base = bigint_clone(a);

    if (!base) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        return -1;
    }

    if (bigint_mod(base, base, c) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        return -1;
    }

    bigint_t* exp = bigint_clone(b);

    if (!exp) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        return -1;
    }

    if (bigint_set_uint64(result, 1) == -1) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(base);
        bigint_destroy(exp);
        return -1;
    }

    while (exp->sign > 0) {
        if(bigint_is_odd(exp)) {
            if (bigint_mul_mod(result, result, base, c) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(base);
                bigint_destroy(exp);
                return -1;
            }
        }

        if (bigint_mul_mod(base, base, base, c) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(base);
            bigint_destroy(exp);
            return -1;
        }

        if(bigint_shr_one(exp) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(base);
            bigint_destroy(exp);
            return -1;
        }

    }

    bigint_destroy(base);
    bigint_destroy(exp);

    bigint_remove_leading_zeros(result);

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

        if(a->neged_with_sign && bigint_neg_with_sign_inplace((bigint_t*)a, true) == -1) {
            bigint_destroy(neg_a);
            return -1;
        }

        if (bigint_neg(neg_a, a) == -1) {
            bigint_destroy(neg_a);
            return -1;
        }

        bigint_t* neg_b = bigint_create();

        if(b->neged_with_sign && bigint_neg_with_sign_inplace((bigint_t*)b, true) == -1) {
            bigint_destroy(neg_a);
            bigint_destroy(neg_b);
            return -1;
        }

        if (bigint_neg(neg_b, b) == -1) {
            bigint_destroy(neg_a);
            bigint_destroy(neg_b);
            return -1;
        }

        int8_t ret = bigint_div_unsigned(orig_result, remainder, neg_a, neg_b);

        orig_result->sign = 1;
        orig_result->neged_with_sign = false;

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
        if(a->neged_with_sign && bigint_neg_with_sign_inplace((bigint_t*)a, true) == -1) {
            bigint_destroy(neg_a);
            return -1;
        }

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
        if(b->neged_with_sign && bigint_neg_with_sign_inplace((bigint_t*)b, true) == -1) {
            bigint_destroy(neg_a);
            bigint_destroy(neg_b);
            return -1;
        }

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

    uint64_t a_length = bigint_bit_length(a);
    uint64_t b_length = bigint_bit_length(b);

    int64_t max_bit_length = MAX(a_length, b_length);

    bigint_t* quotient = result; // for readability

    bigint_t* tmp_remainder = bigint_create();

    if(!tmp_remainder) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    for(int64_t i = max_bit_length; i > -1; i--) {
        if(bigint_shl_one(tmp_remainder) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_remainder);
            return -1;
        }

        boolean_t bit = false;

        if(bigint_get_bit(a, i, &bit) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }

            bigint_destroy(tmp_remainder);
            return -1;
        }

        if(bit) {
            if(bigint_set_bit(tmp_remainder, 0, true) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(tmp_remainder);
                return -1;
            }
        }

        if(bigint_cmp(tmp_remainder, b) != -1) {
            if(bigint_sub(tmp_remainder, tmp_remainder, b) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(tmp_remainder);
                return -1;
            }

            if(bigint_set_bit(quotient, i, true) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(tmp_remainder);
                return -1;
            }
        }
    }

    if(remainder) {
        bigint_remove_leading_zeros(tmp_remainder);
        bigint_set_bigint(remainder, tmp_remainder);
    }

    bigint_destroy(tmp_remainder);

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
        if (tmp_remainder->sign < 0 && tmp_remainder->neged_with_sign) {
            bigint_neg_with_sign_inplace((bigint_t*)tmp_remainder, true);
        }

        tmp_remainder->sign = -1;
        tmp_remainder->neged_with_sign = false;
    } else if(a->sign < 0) {
        if(bigint_sub(tmp_remainder, tmp_remainder, b) == -1) {
            bigint_destroy(tmp_result);
            bigint_destroy(tmp_remainder);
            return -1;
        }

        if(bigint_neg_with_sign_inplace((bigint_t*)tmp_remainder, true) == -1) {
            bigint_destroy(tmp_result);
            bigint_destroy(tmp_remainder);
            return -1;
        }

        tmp_remainder->sign = 1;
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

    bigint_t* tmp_a = bigint_clone(a);

    if (!tmp_a) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        return -1;
    }

    bigint_t* tmp_b = bigint_clone(b);

    if (!tmp_b) {
        if (orig_result_is_a) {
            bigint_destroy(result);
        }

        bigint_destroy(tmp_a);
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

    bigint_remove_leading_zeros(result);

    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);
    bigint_destroy(tmp);

    if (orig_result_is_a) {
        bigint_set_bigint(orig_result, result);
        bigint_destroy(result);
    }

    return 0;
}

static bigint_t* bigint_random_internal(uint64_t bits, boolean_t force_msb) {
    bigint_t* result = bigint_create();

    if (!result) {
        return NULL;
    }

    if (bits == 0) {
        return result;
    }

    uint64_t item_count = bits / BIGINT_ITEM_BITS;
    uint64_t remaining_bits = bits % BIGINT_ITEM_BITS;

    if(item_count == 0 && remaining_bits == 0) {
        bigint_destroy(result);
        return NULL;
    }

    bigint_item_t* prev = NULL;

    for (uint64_t i = 0; i < item_count; i++) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            bigint_destroy(result);
            return NULL;
        }

        item->value = rand64();
        item->value &= BIGINT_ITEM_MAX;
        item->next = NULL;
        item->prev = prev;

        if (prev) {
            prev->next = item;
        } else {
            result->lsb = item;
        }

        prev = item;
    }

    if (remaining_bits) {
        bigint_item_t* item = (bigint_item_t*)memory_malloc(sizeof(bigint_item_t));

        if (!item) {
            bigint_destroy(result);
            return NULL;
        }

        item->value = rand64();
        item->value &= (1ULL << remaining_bits) - 1;
        item->next = NULL;
        item->prev = prev;

        if (prev) {
            prev->next = item;
        } else {
            result->lsb = item;
        }

        prev = item;
    }

    if(!prev) { // never happens
        bigint_destroy(result);
        return NULL;
    }

    result->msb = prev;

    result->sign = 1;

    if(!force_msb) {
        return result;
    }

    // ensure the most significant bit is set
    if (remaining_bits) {
        result->msb->value |= 1ULL << (remaining_bits - 1);
    } else {
        result->msb->value |= 1ULL << (BIGINT_ITEM_BITS - 1);
    }


    return result;
}

bigint_t* bigint_random(uint64_t bits) {
    return bigint_random_internal(bits, true);
}

bigint_t* bigint_random_range(const bigint_t* min, const bigint_t* max) {
    if(!min || !max) {
        return NULL;
    }

    bigint_t* range = bigint_create();

    if (!range) {
        return NULL;
    }

    if (bigint_sub(range, max, min) == -1) {
        bigint_destroy(range);
        return NULL;
    }

    bigint_t* result = bigint_create();

    if (!result) {
        bigint_destroy(range);
        return NULL;
    }

    uint64_t bits_upper = bigint_bit_length(range);

    uint64_t bits_random = rand64() % bits_upper;


    bigint_t* tmp = bigint_random_internal(bits_random, false);

    if (!tmp) {
        bigint_destroy(range);
        bigint_destroy(result);
        return NULL;
    }

    if (bigint_add(result, min, tmp) == -1) {
        bigint_destroy(range);
        bigint_destroy(result);
        bigint_destroy(tmp);
        return NULL;
    }

    bigint_destroy(range);
    bigint_destroy(tmp);

    return result;
}

static boolean_t bigint_is_prime_miller_rabin(const bigint_t* a, uint64_t try) {
    if(!a) {
        return false;
    }

    if(a->sign <= 0) {
        return false;
    }

    if(bigint_is_uint64(a, 2) || bigint_is_uint64(a, 3)) {
        return true;
    }

    if(bigint_is_uint64(a, 1)) {
        return false;
    }

    if(bigint_is_even(a)) {
        return false;
    }

    bigint_t* one = bigint_one();
    bigint_t* two = bigint_two();

    if(!one || !two) {
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    bigint_t* n_minus_1 = bigint_clone(a);

    if (!n_minus_1) {
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    if (bigint_sub(n_minus_1, n_minus_1, one) == -1) {
        bigint_destroy(n_minus_1);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    bigint_t* d = bigint_clone(n_minus_1);

    if (!d) {
        bigint_destroy(n_minus_1);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    bigint_t* r = bigint_create();

    if (!r) {
        bigint_destroy(n_minus_1);
        bigint_destroy(d);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    while (bigint_is_even(d)) {
        if(bigint_add(r, r, one) == -1) {
            bigint_destroy(n_minus_1);
            bigint_destroy(d);
            bigint_destroy(r);
            bigint_destroy(one);
            bigint_destroy(two);
            return false;
        }

        if (bigint_shr_one(d) == -1) {
            bigint_destroy(n_minus_1);
            bigint_destroy(d);
            bigint_destroy(r);
            bigint_destroy(one);
            bigint_destroy(two);
            return false;
        }

        if(d->sign == 0) {
            break;
        }
    }

    if(bigint_sub(r, r, one) == -1) {
        bigint_destroy(n_minus_1);
        bigint_destroy(d);
        bigint_destroy(r);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    bigint_t* r_minus_1 = r; // for readability

    bigint_t* x = bigint_create();

    if (!x) {
        bigint_destroy(n_minus_1);
        bigint_destroy(d);
        bigint_destroy(r);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    bigint_t* a_minus_2 = bigint_clone(a);

    if (!a_minus_2) {
        bigint_destroy(n_minus_1);
        bigint_destroy(d);
        bigint_destroy(r);
        bigint_destroy(x);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    if (bigint_sub(a_minus_2, a_minus_2, two) == -1) {
        bigint_destroy(n_minus_1);
        bigint_destroy(d);
        bigint_destroy(r);
        bigint_destroy(x);
        bigint_destroy(a_minus_2);
        bigint_destroy(one);
        bigint_destroy(two);
        return false;
    }

    // const char * str = NULL;

    for (uint64_t i = 0; i < try; i++) {
        bigint_t* test_random = bigint_random_range(two, a_minus_2);
        if (!test_random) {
            bigint_destroy(n_minus_1);
            bigint_destroy(d);
            bigint_destroy(r);
            bigint_destroy(x);
            bigint_destroy(a_minus_2);
            bigint_destroy(one);
            bigint_destroy(two);
            return false;
        }

        if (bigint_pow_mod(x, test_random, d, a) == -1) {
            bigint_destroy(n_minus_1);
            bigint_destroy(d);
            bigint_destroy(r);
            bigint_destroy(x);
            bigint_destroy(test_random);
            bigint_destroy(a_minus_2);
            bigint_destroy(one);
            bigint_destroy(two);
            return false;
        }

        if (!bigint_is_uint64(x, 1) && bigint_cmp(x, n_minus_1) != 0) {
            bigint_t* j = bigint_create();

            if (!j) {
                bigint_destroy(n_minus_1);
                bigint_destroy(d);
                bigint_destroy(r);
                bigint_destroy(x);
                bigint_destroy(test_random);
                bigint_destroy(a_minus_2);
                bigint_destroy(one);
                bigint_destroy(two);
                return false;
            }

            while(bigint_cmp(j, r_minus_1) == -1 && bigint_cmp(x, n_minus_1) != 0) {
                if (bigint_pow_mod(x, x, two, a) == -1) {
                    bigint_destroy(n_minus_1);
                    bigint_destroy(d);
                    bigint_destroy(r);
                    bigint_destroy(x);
                    bigint_destroy(test_random);
                    bigint_destroy(a_minus_2);
                    bigint_destroy(j);
                    bigint_destroy(one);
                    bigint_destroy(two);
                    return false;
                }

                if (bigint_is_uint64(x, 1)) {
                    bigint_destroy(test_random);
                    bigint_destroy(n_minus_1);
                    bigint_destroy(d);
                    bigint_destroy(r);
                    bigint_destroy(x);
                    bigint_destroy(a_minus_2);
                    bigint_destroy(one);
                    bigint_destroy(two);
                    return false;
                }

                bigint_add(j, j, one);
            }

            bigint_destroy(j);

            if (bigint_cmp(x, n_minus_1) != 0) {
                bigint_destroy(test_random);
                bigint_destroy(n_minus_1);
                bigint_destroy(d);
                bigint_destroy(r);
                bigint_destroy(x);
                bigint_destroy(a_minus_2);
                bigint_destroy(one);
                bigint_destroy(two);
                return false;
            }
        }

        bigint_destroy(test_random);
    }

    bigint_destroy(n_minus_1);
    bigint_destroy(d);
    bigint_destroy(r);
    bigint_destroy(x);
    bigint_destroy(a_minus_2);
    bigint_destroy(one);
    bigint_destroy(two);

    return true;
}

boolean_t bigint_is_prime(const bigint_t* a) {
    if(!a) {
        return false;
    }

    return bigint_is_prime_miller_rabin(a, 128);
}

bigint_t* bigint_random_prime(uint64_t bits) {
    if(bits < 2) {
        return NULL;
    }

    bigint_t* two = bigint_one();

    if (!two) {
        return NULL;
    }

    while(true) {
        bigint_t* result = bigint_random(bits);

        if (!result) {
            bigint_destroy(two);
            return NULL;
        }

        result->lsb->value |= 1;

        for (uint64_t i = 0; i < 100; i++) {
            if (bigint_is_prime(result)) {
                bigint_destroy(two);
                return result;
            }

            if(bigint_add(result, result, two) == -1) {
                bigint_destroy(result);
                bigint_destroy(two);
                return NULL;
            }

        }

        bigint_destroy(result);
    }

    bigint_destroy(two);

    return NULL;
}


