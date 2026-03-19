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
#include <stdbufs.h>
#include <logging.h>
#include <random.h>
#include <int_limits.h>

MODULE("turnstone.lib");

/*! each value is 48 bits */
#define BIGINT_LIMB_BITS           64
#define BIGINT_LIMB_BYTES          (BIGINT_LIMB_BITS / 8)
#define BIGINT_HEX_DIGITS_PER_LIMB 16
#define BIGINT_INITIAL_CAPACITY     4
#define BIGINT_MAX_SAFE_CAPACITY   (1024 * 1024 * 16) // 16 million limbs = 128MB

struct bigint_t {
    uint64_t* limbs; // Dynamic array, limbs[0] = LSB
    uint64_t  limb_count; // Allocated/used size
    uint64_t  capacity; // For growth (e.g., start with 4, double on resize)
    int32_t   sign;
    boolean_t neged_with_sign;
};

bigint_t* bigint_create(void) {
    bigint_t* bigint = (bigint_t*)memory_malloc(sizeof(bigint_t));

    if (!bigint) {
        return NULL;
    }

    bigint->limbs = (uint64_t*)memory_malloc(sizeof(uint64_t) * BIGINT_INITIAL_CAPACITY);
    if (!bigint->limbs) {
        memory_free(bigint);
        return NULL;
    }

    bigint->limb_count      = 1;
    bigint->capacity        = BIGINT_INITIAL_CAPACITY;
    bigint->sign            = 0;
    bigint->neged_with_sign = false;

    return bigint;
}

void bigint_destroy(bigint_t* bigint) {
    if(!bigint) {
        return;
    }

    memory_free(bigint->limbs);
    memory_free(bigint);
}

bigint_t* bigint_zero(void) {
    return bigint_create();
}

bigint_t* bigint_one(void) {
    bigint_t* bigint = bigint_create();

    if (bigint) {
        if(bigint_set_int64(bigint, 1) == -1) {
            bigint_destroy(bigint);
            return NULL;
        }
    }

    return bigint;
}

bigint_t* bigint_two(void) {
    bigint_t* bigint = bigint_create();

    if (bigint) {
        if(bigint_set_int64(bigint, 2) == -1) {
            bigint_destroy(bigint);
            return NULL;
        }
    }

    return bigint;
}

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

__attribute__((nonnull(1))) static void bigint_destroy_limbs(bigint_t* bigint) {
    for (uint64_t i = 0; i < bigint->limb_count; i++) {
        bigint->limbs[i] = 0;
    }

    bigint->capacity        = BIGINT_INITIAL_CAPACITY;
    bigint->limb_count      = 1;
    bigint->sign            = 0;
    bigint->neged_with_sign = true;
}

BIGINT_CHECK_RESULT static int8_t bigint_ensure_capacity(bigint_t* bigint, uint64_t required_capacity) {
    if (!bigint) {
        return -1;
    }
    if (bigint->capacity >= required_capacity) {
        return 0;
    }

    uint64_t new_capacity = bigint->capacity;

    // Fix 1: Handle initial zero capacity
    if (new_capacity == 0) {
        new_capacity = BIGINT_INITIAL_CAPACITY;
    }

    // Fix 2: Prevent infinite loop and handle scaling
    while (new_capacity < required_capacity) {
        // Check for overflow before doubling
        if (new_capacity > (UINT64_MAX >> 1)) {
            new_capacity = required_capacity;
            break;
        }
        new_capacity <<= 1; // Faster than *= 2
    }

    // Fix 3: Sanity limit (e.g., don't allow 1GB for a single BigInt)
    if (new_capacity > BIGINT_MAX_SAFE_CAPACITY ||
        new_capacity < bigint->limb_count ||
        new_capacity < required_capacity ||
        new_capacity == 0) {
        return -1;
    }

    uint64_t* new_limbs = (uint64_t*)memory_malloc(sizeof(uint64_t) * new_capacity);
    if (!new_limbs) {
        return -1;
    }

    memory_memcopy(bigint->limbs, new_limbs, sizeof(uint64_t) * bigint->limb_count);
    memory_free(bigint->limbs);

    bigint->limbs    = new_limbs;
    bigint->capacity = new_capacity;
    return 0;
}

int8_t bigint_set_int64(bigint_t* bigint, int64_t value) {
    if(!bigint) {
        return -1;
    }

    uint64_t uvalue;

    bigint_destroy_limbs(bigint);

    if(value == 0) {
        bigint->sign       = 0;
        bigint->limb_count = 1;
        return 0;
    }

    if (value < 0) {
        bigint->sign            = -1;
        bigint->neged_with_sign = true;
        uvalue                  = -value;
    } else {
        bigint->sign = 1;
        uvalue       = value;
    }

    bigint->limbs[0]   = uvalue;
    bigint->limb_count = 1;

    return 0;
}

int8_t bigint_set_uint64(bigint_t* bigint, uint64_t value) {
    bigint_destroy_limbs(bigint);

    if (value == 0) {
        bigint->sign       = 0;
        bigint->limb_count = 1;
        return 0;
    }

    bigint->sign = 1;

    bigint->limbs[0]   = value;
    bigint->limb_count = 1;

    return 0;
}

int8_t bigint_set_zero(bigint_t* bigint) {
    return bigint_set_int64(bigint, 0);
}

int8_t bigint_set_bigint(bigint_t* bigint, const bigint_t* src) {
    if(!bigint || !src) {
        return -1;
    }

    if(bigint == src) {
        return 0;
    }

    memory_free(bigint->limbs);
    bigint->limbs = (uint64_t*)memory_malloc(sizeof(uint64_t) * src->capacity);

    if (!bigint->limbs) {
        return -1;
    }

    memory_memcopy(src->limbs, bigint->limbs, sizeof(uint64_t) * src->limb_count);

    bigint->capacity        = src->capacity;
    bigint->limb_count      = src->limb_count;
    bigint->sign            = src->sign;
    bigint->neged_with_sign = src->neged_with_sign;

    return 0;
}

__attribute__((nonnull(1))) static void bigint_normalize(bigint_t* bigint) {
    // Case 1: Sign-Magnitude (Normal) or Positive
    // We strip leading Zeros.
    if (bigint->sign >= 0 || (bigint->sign == -1 && bigint->neged_with_sign)) {
        while (bigint->limb_count > 1 && bigint->limbs[bigint->limb_count - 1] == 0) {
            bigint->limb_count--;
        }

        if(bigint->limb_count == 0) {
            bigint->limb_count = 1;
        }

        // If the resulting value is 0, fix the sign
        if (bigint->limb_count == 1 && bigint->limbs[0] == 0) {
            bigint->sign            = 0;
            bigint->neged_with_sign = true;
        }
        return;
    }

    // Case 2: Two's Complement (neged_with_sign == false)
    // We strip leading UINT64_MAX (sign extension).
    if (bigint->sign == -1 && !bigint->neged_with_sign) {
        while (bigint->limb_count > 1 && bigint->limbs[bigint->limb_count - 1] == UINT64_MAX) {
            bigint->limb_count--;
        }

        if(bigint->limb_count == 0) {
            bigint->limb_count = 1;
        }

        // Safety: If it stripped down to 0, it's actually zero.
        if (bigint->limb_count == 1 && bigint->limbs[0] == 0) {
            bigint->sign            = 0;
            bigint->neged_with_sign = true;
        }
    }
}

BIGINT_CHECK_RESULT static int8_t bigint_ensure_normal_form(bigint_t* a) {
    if (!a || a->sign >= 0 || (a->sign < 0 && a->neged_with_sign)) {
        return 0; // Already normal or zero
    }

    /* To convert from two's complement back to magnitude:
       1. Invert all bits
       2. Add 1
     */
    for (uint64_t i = 0; i < a->limb_count; i++) {
        a->limbs[i] = ~a->limbs[i];
    }

    // Add 1 to the magnitude
    uint64_t carry = 1;
    for (uint64_t i = 0; i < a->limb_count && carry; i++) {
        uint64_t val = a->limbs[i] + carry;
        carry       = (val < a->limbs[i]) ? 1 : 0;
        a->limbs[i] = val;
    }

    if (carry) {
        if (bigint_ensure_capacity(a, a->limb_count + 1) == -1) {
            return -1;
        }
        a->limbs[a->limb_count++] = carry;
    }

    a->neged_with_sign = true; // Mark as normal form
    bigint_normalize(a);
    return 0;
}

BIGINT_CHECK_RESULT static int8_t bigint_copy(bigint_t* dest, const bigint_t* src) {
    if (!dest || !src) {
        return -1;
    }
    if (dest == src) {
        return 0; // Self-copy protection
    }
    // 1. Match capacity
    if (bigint_ensure_capacity(dest, src->limb_count) == -1) {
        return -1;
    }

    // 2. Copy the actual limb data
    if (src->limb_count > 0) {
        memory_memcopy(src->limbs, dest->limbs, src->limb_count * sizeof(uint64_t));
    }

    // 3. Match metadata
    dest->limb_count      = src->limb_count;
    dest->sign            = src->sign;
    dest->neged_with_sign = src->neged_with_sign;

    // 4. Ensure extra limbs in destination are zeroed out
    if (dest->capacity > dest->limb_count) {
        memory_memclean(dest->limbs + dest->limb_count, (dest->capacity - dest->limb_count) * sizeof(uint64_t));
    }

    bigint_normalize(dest);

    return 0;
}

BIGINT_CHECK_RESULT static int8_t bigint_abs_copy(bigint_t* dest, const bigint_t* src) {
    if (!dest || !src) {
        return -1;
    }

    // 1. Perform a standard copy
    if (bigint_copy(dest, src) == -1) {
        return -1;
    }

    // 2. If it's zero, we are done
    if (dest->sign == 0) {
        return 0;
    }

    // 3. If it's in Two's Complement form, normalize it to Magnitude form
    if (!dest->neged_with_sign) {
        if (bigint_ensure_normal_form(dest) == -1) {
            return -1;
        }
    }

    // 4. Force the sign to positive
    dest->sign            = 1;
    dest->neged_with_sign = true;

    return 0;
}

static inline int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

int8_t bigint_set_str(bigint_t* bigint, const char* str) {
    if (!bigint || !str) {
        return -1;
    }

    // 1. Handle Sign
    int32_t sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // 2. Skip Leading Zeros
    while (*str == '0') {
        str++;
    }

    size_t len = strlen(str);

    if (len == 0) {
        memory_memclean(bigint->limbs, bigint->capacity * sizeof(uint64_t));
        bigint->sign            = 0;
        bigint->limb_count      = 1;
        bigint->neged_with_sign = true;
        return 0;
    }

    // 3. Allocate Memory
    size_t limbs_needed = (len + BIGINT_HEX_DIGITS_PER_LIMB - 1) / BIGINT_HEX_DIGITS_PER_LIMB;
    if (bigint_ensure_capacity(bigint, limbs_needed) == -1) {
        return -1;
    }

    // Clear limbs before use to avoid garbage values
    memory_memclean(bigint->limbs, bigint->capacity * sizeof(uint64_t));
    bigint->limb_count = limbs_needed;

    // 4. Parse String (Right to Left)
    // We process the string from the end because the end is the Least Significant Digit.
    const char* end = str + len;
    for (size_t i = 0; i < limbs_needed; i++) {
        uint64_t current_limb = 0;

        // We take up to 16 characters from the string, moving backwards
        for (int j = 0; j < BIGINT_HEX_DIGITS_PER_LIMB; j++) {
            const char* current_char_ptr = end - 1 - (i * BIGINT_HEX_DIGITS_PER_LIMB + j);

            // If we've reached the start of the string (str), stop for this limb
            if (current_char_ptr < str) {
                break;
            }

            int val = hex_digit_value(*current_char_ptr);
            if (val < 0) {
                return -1; // Invalid hex
            }

            current_limb |= ((uint64_t)val) << (j * 4);
        }

        bigint->limbs[i] = current_limb;
    }

    // 5. Finalize Sign and Normalization
    // (A quick normalization in case the string had zeros that were skipped or was small)
    while (bigint->limb_count > 0 && bigint->limbs[bigint->limb_count - 1] == 0) {
        bigint->limb_count--;
    }

    if (bigint->limb_count == 0) {
        bigint->limb_count = 1;
        bigint->sign       = 0;
    } else {
        bigint->sign = sign;
        if(sign < 0) {
            // Since input is a normal string, it's not in two's complement yet
            bigint->neged_with_sign = true;
        }
    }

    return 0;
}

static int8_t bigint_inc_with_overflow(bigint_t* bigint) {
    if (!bigint) {
        return -1;
    }

    if (bigint->sign == 0) {
        return bigint_set_int64(bigint, 1);
    }

    uint64_t carry = 1;
    size_t i       = 0;

    // Process from LSB upward
    while (carry && i < bigint->limb_count)
    {
        uint64_t new_value = bigint->limbs[i] + carry;

        if (new_value >= bigint->limbs[i]) {
            bigint->limbs[i] = new_value;
            return 0;
        }

        bigint->limbs[i] = 0;
        carry            = 1;
        i++;
    }

    return 0;
}

BIGINT_CHECK_RESULT static int8_t bigint_neg_with_sign_inplace(bigint_t* a, boolean_t force) {
    if(!a) {
        return -1;
    }

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

char* bigint_to_str(const bigint_t* bigint) {
    if (!bigint) {
        return NULL;
    }

    if (bigint->sign == 0 || bigint->limb_count == 0) {
        return strdup("0");
    }

    buffer_t* buf = buffer_new_with_capacity(NULL, 32); // reasonable starting size
    if (!buf) {
        return NULL;
    }

    boolean_t is_negative = (bigint->sign < 0);
    if (is_negative) {
        buffer_append_byte(buf, '-');

        if (!bigint->neged_with_sign) {
            if(bigint_neg_with_sign_inplace((bigint_t*)bigint, true) == -1) {
                buffer_destroy(buf);
                return NULL;
            }
        }
    }

    size_t idx = bigint->limb_count - 1;
    while (idx > 0 && bigint->limbs[idx] == 0) {
        idx--;
    }

    if (bigint->limbs[idx] == 0) {
        buffer_destroy(buf);
        return strdup("0");
    }

    buffer_printf(buf, "%llx", bigint->limbs[idx]);

    while (idx > 0) {
        idx--;
        buffer_printf(buf, "%016llx", bigint->limbs[idx]);
    }

    buffer_append_byte(buf, '\0');

    uint8_t* result = buffer_get_all_bytes_and_destroy(buf, NULL);

    return (char*)result;
}

boolean_t bigint_is_zero(const bigint_t* a) {
    if (!a) {
        return true;
    }

    return a->sign == 0 || (a->limb_count == 1 && a->limbs[0] == 0);
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

    return a->limbs[0] & 1;
}

boolean_t bigint_is_even(const bigint_t* a) {
    if (!a) {
        return false;
    }

    if (a->sign == 0) {
        return true;
    }

    return !(a->limbs[0] & 1);
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

    if(a->limb_count > 1) {
        return false;
    }

    uint64_t uvalue = (value < 0) ? -value : value;

    return a->limbs[0] == uvalue;
}

boolean_t bigint_is_uint64(const bigint_t* a, uint64_t value) {
    if (!a) {
        return false;
    }

    if (a->sign == 0) {
        return value == 0;
    }

    if (a->sign < 0) {
        return false;
    }

    if(a->limb_count > 1) {
        return false;
    }

    return a->limbs[0] == value;
}

int8_t bigint_neg(bigint_t* result, const bigint_t* a) {
    if(!result || !a) {
        return -1;
    }

    if(a->sign < 0 && a->neged_with_sign) {
        if(bigint_neg_with_sign_inplace((bigint_t*)a, true) == -1) {
            return -1;
        }
    }

    if (result == a) {
        result->sign = -result->sign; // just change the sign

        return 0;
    }

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
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Handle Zero Case
    if (a->sign == 0 || b->sign == 0) {
        // 0 & anything is 0
        // If result is 'a' or 'b', bigint_set_zero handles it safely
        return bigint_set_zero(result);
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

    // 2. Determine the working range
    // We need to iterate up to the largest limb count to handle
    // Two's Complement sign extension properly.
    uint64_t max_limbs = (a->limb_count > b->limb_count) ? a->limb_count : b->limb_count;

    if (bigint_ensure_capacity(result, max_limbs) == -1) {
        return -1;
    }

    // 3. Bitwise AND with Virtual Sign Extension
    // A negative number in Two's Complement form is conceptually padded with 1s to the left.
    // A positive number is conceptually padded with 0s.

    uint64_t a_pad = (a->sign < 0 && !a->neged_with_sign) ? 0xFFFFFFFFFFFFFFFFUL : 0;
    uint64_t b_pad = (b->sign < 0 && !b->neged_with_sign) ? 0xFFFFFFFFFFFFFFFFUL : 0;

    for (uint64_t i = 0; i < max_limbs; i++) {
        uint64_t val_a = (i < a->limb_count) ? a->limbs[i] : a_pad;
        uint64_t val_b = (i < b->limb_count) ? b->limbs[i] : b_pad;

        result->limbs[i] = val_a & val_b;
    }

    result->limb_count = max_limbs;

    // 4. Determine Result Metadata
    // Logic: negative & negative = negative
    if (a->sign < 0 && b->sign < 0) {
        result->sign            = -1;
        result->neged_with_sign = false; // Keep in Two's Complement
    } else {
        result->sign            = 1;
        result->neged_with_sign = false; // Result is a positive magnitude
    }

    // 5. Cleanup
    bigint_normalize(result);

    return 0;
}

int8_t bigint_or(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Handle Zero Cases
    if (a->sign == 0) {
        return bigint_copy(result, b);
    }

    if (b->sign == 0) {
        return bigint_copy(result, a);
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

    // 2. Determine working range
    uint64_t max_limbs = (a->limb_count > b->limb_count) ? a->limb_count : b->limb_count;

    if (bigint_ensure_capacity(result, max_limbs) == -1) {
        return -1;
    }

    // 3. Bitwise OR with Virtual Sign Extension
    // Negative Two's Complement: conceptually padded with 1s (0xFF...FF)
    // Positive/Magnitude: conceptually padded with 0s
    uint64_t a_pad = (a->sign < 0 && !a->neged_with_sign) ? 0xFFFFFFFFFFFFFFFFUL : 0;
    uint64_t b_pad = (b->sign < 0 && !b->neged_with_sign) ? 0xFFFFFFFFFFFFFFFFUL : 0;

    for (uint64_t i = 0; i < max_limbs; i++) {
        uint64_t val_a = (i < a->limb_count) ? a->limbs[i] : a_pad;
        uint64_t val_b = (i < b->limb_count) ? b->limbs[i] : b_pad;

        result->limbs[i] = val_a | val_b;
    }

    result->limb_count = max_limbs;

    // 4. Metadata
    // Logic: If either is negative (1), the result is negative (1 | 0 = 1)
    if (a->sign < 0 || b->sign < 0) {
        result->sign = -1;
        // If either was already in Two's Complement, keep result that way
        result->neged_with_sign = false;
    } else {
        result->sign            = 1;
        result->neged_with_sign = true;
    }

    // 5. Cleanup
    bigint_normalize(result);

    return 0;
}

int8_t bigint_xor(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Handle Zero Cases
    if (a->sign == 0) {
        return bigint_copy(result, b);
    }

    if (b->sign == 0) {
        return bigint_copy(result, a);
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

    // 2. Determine working range
    uint64_t max_limbs = (a->limb_count > b->limb_count) ? a->limb_count : b->limb_count;

    if (bigint_ensure_capacity(result, max_limbs) == -1) {
        return -1;
    }

    // 3. Bitwise XOR with Virtual Sign Extension
    uint64_t a_pad = (a->sign < 0 && !a->neged_with_sign) ? 0xFFFFFFFFFFFFFFFFUL : 0;
    uint64_t b_pad = (b->sign < 0 && !b->neged_with_sign) ? 0xFFFFFFFFFFFFFFFFUL : 0;

    for (uint64_t i = 0; i < max_limbs; i++) {
        uint64_t val_a = (i < a->limb_count) ? a->limbs[i] : a_pad;
        uint64_t val_b = (i < b->limb_count) ? b->limbs[i] : b_pad;

        result->limbs[i] = val_a ^ val_b;
    }

    result->limb_count = max_limbs;

    // 4. Metadata
    // XOR logic: Result is negative only if signs are different
    if ((a->sign < 0) ^ (b->sign < 0)) {
        result->sign            = -1;
        result->neged_with_sign = false; // Result is in Two's Complement
    } else {
        result->sign            = (a->sign == 0 && b->sign == 0) ? 0 : 1;
        result->neged_with_sign = true; // Result is a magnitude
    }

    // 5. Cleanup
    bigint_normalize(result);

    return 0;
}

BIGINT_CHECK_RESULT static int8_t bigint_not_inplace(bigint_t* a) {
    if(!a) {
        return -1;
    }

    if (a->sign == 0) {
        return 0;
    }

    for (uint64_t i = 0; i < a->limb_count; i++) {
        a->limbs[i] = ~a->limbs[i] & UINT64_MAX;
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

    memory_free(result->limbs);

    result->limb_count = a->limb_count;
    result->capacity   = a->capacity;

    result->limbs = (uint64_t*)memory_malloc(sizeof(uint64_t) * result->capacity);

    if (!result->limbs) {
        return -1;
    }

    result->sign = a->sign;

    for (uint64_t i = 0; i < a->limb_count; i++) {
        result->limbs[i] = ~a->limbs[i] & UINT64_MAX;
    }

    return 0;
}

int8_t bigint_shl_one(bigint_t* a) {
    if (!a) {
        return -1;
    }

    if (a->sign == 0) {
        return 0;
    }

    // 1. Check if the current MSB will overflow
    // If the top bit of the top limb is 1, we definitely need a new limb.
    uint64_t msb_val     = a->limbs[a->limb_count - 1];
    int needs_extra_limb = (msb_val >> (BIGINT_LIMB_BITS - 1)) & 1;

    if (needs_extra_limb) {
        if (bigint_ensure_capacity(a, a->limb_count + 1) == -1) {
            return -1;
        }
    }

    // 2. The Ripple Shift
    uint64_t carry = 0;
    for (uint64_t i = 0; i < a->limb_count; i++) {
        uint64_t current = a->limbs[i];

        // New value is (current shifted) OR (carry from previous limb)
        a->limbs[i] = (current << 1) | carry;

        // Carry for the NEXT limb is the bit that falls off the top of THIS limb
        carry = (current >> (BIGINT_LIMB_BITS - 1)) & 1;
    }

    // 3. Handle the final carry if it exists
    if (carry) {
        // Here we handle your specific logic for negative two's complement
        if (a->sign < 0 && !a->neged_with_sign) {
            // Sign extend with 1s if it's a negative two's complement value
            a->limbs[a->limb_count] = UINT64_MAX;
        } else {
            a->limbs[a->limb_count] = carry;
        }
        a->limb_count++;
    }

    // 4. Cleanup
    bigint_normalize(a);
    return 0;
}

int8_t bigint_shr_one(bigint_t* a) {
    if (!a) {
        return -1;
    }

    if (a->sign == 0) {
        return 0;
    }

    uint64_t carry = 0;

    // We work from MSB down to LSB (High index to Low index)
    // to handle the carry flowing from higher bits to lower bits.
    for (int64_t i = (int64_t)a->limb_count - 1; i >= 0; i--) {
        uint64_t current = a->limbs[i];

        // Shift current limb right and pull in the carry from the higher limb
        uint64_t next_val = (current >> 1) | carry;

        // The carry for the NEXT (lower) limb is the bit that falls off the bottom of THIS limb
        carry = (current & 1ULL) << (BIGINT_LIMB_BITS - 1);

        a->limbs[i] = next_val;
    }

    // Handle Arithmetic Shift for Two's Complement
    // If negative and not in "normal" form, we must set the MSB back to 1
    if (a->sign < 0 && !a->neged_with_sign) {
        a->limbs[a->limb_count - 1] |= (1ULL << (BIGINT_LIMB_BITS - 1));
    }

    // Normalizing is important because if the MSB was 1 and it shifted right,
    // the highest limb might become 0.
    bigint_normalize(a);

    return 0;
}

int8_t bigint_shl(bigint_t* result, const bigint_t* a, int64_t shift) {
    if (!result || !a) {
        return -1;
    }

    if (a->sign == 0) {
        return bigint_set_zero(result); // Helper to set limb_count to 0 and sign to 0
    }
    if (shift < 0) {
        return bigint_shr(result, a, -shift);
    }

    if (shift == 0) {
        return bigint_copy(result, a); // Helper for deep copy
    }

    int64_t limb_shift = shift / BIGINT_LIMB_BITS;
    int32_t bit_shift  = (int32_t)(shift % BIGINT_LIMB_BITS);

    // 1. Calculate new required size
    // We need current used limbs + limb_shift, plus potentially 1 extra if bit_shift causes an overflow
    uint64_t needed_limbs = a->limb_count + limb_shift + 1;

    // 2. Ensure capacity (Helper function to realloc limbs if needed)
    if (bigint_ensure_capacity(result, needed_limbs) == -1) {
        return -1;
    }

    // 3. Handle the bit-level shift and move
    // We work backwards to handle the case where result == a
    uint64_t carry = 0;
    for (int64_t i = (int64_t)a->limb_count - 1; i >= 0; i--) {
        uint64_t val        = a->limbs[i];
        uint64_t next_carry = (bit_shift == 0) ? 0 : (val >> (BIGINT_LIMB_BITS - bit_shift));

        result->limbs[i + limb_shift + 1] = 0; // Clear the potential carry slot
        result->limbs[i + limb_shift]     = (val << bit_shift) | carry;

        carry = next_carry;
    }

    // Place the final carry if it exists
    if (carry) {
        result->limbs[a->limb_count + limb_shift] = carry;
        result->limb_count                        = a->limb_count + limb_shift + 1;
    } else {
        result->limb_count = a->limb_count + limb_shift;
    }

    // 4. Zero out the low-order limbs created by limb_shift
    if (limb_shift > 0) {
        memory_memclean(result->limbs, limb_shift * sizeof(uint64_t));
    }

    // 5. Handle sign and "neged_with_sign" (Two's Complement logic)
    result->sign            = a->sign;
    result->neged_with_sign = a->neged_with_sign;

    if (result->sign < 0 && !result->neged_with_sign) {
        // If it's in two's complement form and negative,
        // shifting left usually requires sign extension on the new bits.
        // However, standard bigint SHL usually treats the number as a
        // magnitude. If you want to maintain the "Two's Complement"
        // invariant, you may need to mask or extend the MSB here.
    }

    // 6. Final cleanup: normalize (remove leading zero limbs)
    bigint_normalize(result);

    return 0;
}

int8_t bigint_shr(bigint_t* result, const bigint_t* a, int64_t shift) {
    if (!result || !a) {
        return -1;
    }
    if (shift < 0) {
        return bigint_shl(result, a, -shift);
    }

    // If shift is 0, just copy
    if (shift == 0) {
        return bigint_copy(result, a);
    }

    // If a is zero, or shift is so large it wipes everything out
    uint64_t total_bits = a->limb_count * BIGINT_LIMB_BITS;
    if (a->sign == 0 || (uint64_t)shift >= total_bits) {
        // For standard bigints, shifting right beyond length results in 0
        // (Unless handling negative two's complement, which results in -1)
        if (a->sign < 0 && !a->neged_with_sign) {
            if(bigint_set_int64(result, -1) == -1) {
                return -1;
            }
        } else {
            if(bigint_set_zero(result) == -1) {
                return -1;
            }
        }
        return 0;
    }

    int64_t limb_shift = shift / BIGINT_LIMB_BITS;
    int32_t bit_shift  = (int32_t)(shift % BIGINT_LIMB_BITS);

    // 1. Ensure result has enough space (no growth needed usually, but result might be a different object)
    if (bigint_ensure_capacity(result, a->limb_count) == -1) {
        return -1;
    }

    uint64_t new_limb_count = a->limb_count - limb_shift;

    // 2. Perform the shift
    // We work forward (from LSB to MSB) to safely handle result == a
    for (uint64_t i = 0; i < new_limb_count; i++) {
        uint64_t current_limb = a->limbs[i + limb_shift];
        uint64_t next_limb    = (i + limb_shift + 1 < a->limb_count) ? a->limbs[i + limb_shift + 1] : 0;

        uint64_t val = current_limb >> bit_shift;
        if (bit_shift > 0 && (i + limb_shift + 1 < a->limb_count)) {
            // Pull bits from the next higher limb
            val |= (next_limb << (BIGINT_LIMB_BITS - bit_shift));
        }

        result->limbs[i] = val;
    }

    result->limb_count      = new_limb_count;
    result->sign            = a->sign;
    result->neged_with_sign = a->neged_with_sign;

    // 3. Handle Sign Extension for Two's Complement
    // If negative and in two's complement (neged_with_sign == false),
    // we must fill the vacated upper bits with 1s.
    if (result->sign < 0 && !result->neged_with_sign) {
        uint64_t msb_idx = result->limb_count - 1;
        // Calculate how many bits in the top limb should be 1
        // (Bits shifted in from "infinity")
        uint64_t mask = 0xFFFFFFFFFFFFFFFFUL << (BIGINT_LIMB_BITS - bit_shift);
        if (bit_shift == 0) {
            mask = 0;
        }

        result->limbs[msb_idx] |= mask;
    }

    // 4. Cleanup
    bigint_normalize(result);

    return 0;
}

int8_t bigint_set_bit(bigint_t* bigint, uint64_t bit, boolean_t value) {
    if (!bigint) {
        return -1;
    }

    const uint64_t limb_index   = bit / BIGINT_LIMB_BITS;
    const uint64_t bit_in_limb  = bit % BIGINT_LIMB_BITS;
    const uint64_t needed_limbs = limb_index + 1;

    // 1 & 2. Ensure Capacity and Adjust limb_count
    if (bigint_ensure_capacity(bigint, needed_limbs) == -1) {
        return -1;
    }

    uint64_t fill = (bigint->sign < 0 && !bigint->neged_with_sign) ? UINT64_MAX : 0ULL;
    for (uint64_t i = bigint->limb_count; i < needed_limbs; i++) {
        bigint->limbs[i] = fill;
    }
    if (needed_limbs > bigint->limb_count) {
        bigint->limb_count = needed_limbs;
    }

    // 3. Set the Bit
    if (value) {
        bigint->limbs[limb_index] |= (1ULL << bit_in_limb);
        // If the number was zero, it is now positive
        if (bigint->sign == 0) {
            bigint->sign            = 1;
            bigint->neged_with_sign = true;
        }
    } else {
        bigint->limbs[limb_index] &= ~(1ULL << bit_in_limb);
    }

    bigint_normalize(bigint);
    return 0;
}

int8_t bigint_get_bit(const bigint_t* bigint, uint64_t bit, boolean_t* value) {
    if (!bigint || !value) {
        return -1;
    }

    // 0 is always all 0 bits
    if (bigint->sign == 0) {
        *value = false;
        return 0;
    }

    const uint64_t limb_index  = bit / BIGINT_LIMB_BITS;
    const uint64_t bit_in_limb = bit % BIGINT_LIMB_BITS;

    // Handle bits within the existing allocated limbs
    if (limb_index < bigint->limb_count) {
        *value = (bigint->limbs[limb_index] >> bit_in_limb) & 1ULL;
        return 0;
    }

    // Handle bits beyond the physical limbs (Virtual Sign Extension)
    // If it's Two's Complement and Negative, the "infinite" bits are 1s.
    // Otherwise (Magnitude or Positive), the infinite bits are 0s.
    if (bigint->sign < 0 && !bigint->neged_with_sign) {
        *value = true;
    } else {
        *value = false;
    }

    return 0;
}

int8_t bigint_flip_bit(bigint_t* bigint, uint64_t bit) {
    if (!bigint) {
        return -1;
    }

    if (bigint->sign == 0 || bigint->limb_count == 0) {
        if (bit > 0) {
            return 0; // no change needed
        }

        if (bigint->capacity == 0) {
            uint64_t* new_limbs = memory_malloc(BIGINT_INITIAL_CAPACITY * sizeof(uint64_t));
            if (!new_limbs) {
                return -1;
            }
            bigint->limbs    = new_limbs;
            bigint->capacity = BIGINT_INITIAL_CAPACITY;
        }
        bigint->limbs[0]        = 1;
        bigint->limb_count      = 1;
        bigint->sign            = 1;
        bigint->neged_with_sign = false;
        return 0;
    }

    const uint64_t limb_index  = bit / BIGINT_LIMB_BITS;
    const uint64_t bit_in_limb = bit % BIGINT_LIMB_BITS;

    uint64_t needed = limb_index + 1;

    if(bigint_ensure_capacity(bigint, needed) == -1) {
        return -1;
    }

    if (needed > bigint->limb_count) {
        bigint->limb_count = needed;
    }

    uint64_t fill = (bigint->sign < 0) ? UINT64_MAX : 0ULL;
    for (uint64_t i = bigint->limb_count; i < needed; i++) {
        bigint->limbs[i] = fill;
    }

    bigint->limbs[limb_index] ^= (1ULL << bit_in_limb);

    bigint_normalize(bigint);

    return 0;
}

int8_t bigint_clear_bit(bigint_t* bigint, uint64_t bit) {
    if (!bigint) {
        return -1;
    }

    if (bigint->sign == 0 || bigint->limb_count == 0) {
        return 0;
    }

    const uint64_t limb_index  = bit / BIGINT_LIMB_BITS;
    const uint64_t bit_in_limb = bit % BIGINT_LIMB_BITS;

    if (limb_index >= bigint->limb_count) {
        return 0;
    }

    bigint->limbs[limb_index] &= ~(1ULL << bit_in_limb);

    return 0;
}

static int8_t bigint_cmp_magnitudes(const bigint_t* a, const bigint_t* b) {
    if (!a || !b) {
        return -2; // Error case
    }
    // 1. Compare by limb count first (O(1) check)
    // Because the bigint is normalized, more limbs always means a larger magnitude.
    if (a->limb_count > b->limb_count) {
        return 1;
    }
    if (a->limb_count < b->limb_count) {
        return -1;
    }

    // 2. If limb counts are equal, compare from MSB to LSB
    // We start at the highest index because it holds the highest power of 2^64.
    for (int64_t i = (int64_t)a->limb_count - 1; i >= 0; i--) {
        if (a->limbs[i] > b->limbs[i]) {
            return 1;
        }
        if (a->limbs[i] < b->limbs[i]) {
            return -1;
        }
    }

    // 3. All limbs are identical
    return 0;
}

int8_t bigint_cmp(const bigint_t* a, const bigint_t* b) {
    if (!a || !b) {
        return -2; // Use -2 or a specific error code since -1 is a valid result
    }

    // 1. Compare Signs
    if (a->sign > b->sign) {
        return 1;
    }

    if (a->sign < b->sign) {
        return -1;
    }

    // If both are zero
    if (a->sign == 0) {
        return 0;
    }

    // 2. Compare Lengths (Limb Count)
    // Both numbers have the same sign at this point.
    // Note: This assumes numbers are normalized (no leading zero limbs).
    if (a->limb_count > b->limb_count) {
        return (a->sign > 0) ? 1 : -1;
    }
    if (a->limb_count < b->limb_count) {
        return (a->sign > 0) ? -1 : 1;
    }

    // 3. Compare Limb-by-Limb (from MSB to LSB)
    // We start from the highest index because it carries the most weight.
    for (int64_t i = (int64_t)a->limb_count - 1; i >= 0; i--) {
        if (a->limbs[i] > b->limbs[i]) {
            return (a->sign > 0) ? 1 : -1;
        }
        if (a->limbs[i] < b->limbs[i]) {
            return (a->sign > 0) ? -1 : 1;
        }
    }

    // All limbs are equal
    return 0;
}

/**
 * Internal Helper: Subtracts |b| from |a|. Assumes |a| > |b|.
 */
static int8_t bigint_sub_magnitudes(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (bigint_ensure_capacity(result, a->limb_count) == -1) {
        return -1;
    }

    uint64_t borrow = 0;
    for (uint64_t i = 0; i < a->limb_count; i++) {
        uint64_t val_a = a->limbs[i];
        uint64_t val_b = (i < b->limb_count) ? b->limbs[i] : 0;

        // Perform subtraction with borrow
        // Logic: (val_a - borrow) - val_b
        uint64_t temp = val_a - borrow;
        if (temp > val_a) { // Underflow from the previous borrow
            // borrow remains 1
        } else if (temp < val_b) {
            borrow = 1;
        } else {
            borrow = 0;
        }

        result->limbs[i] = temp - val_b;
    }

    result->limb_count = a->limb_count;
    bigint_normalize(result);
    return 0;
}

/**
 * Internal Helper: Adds |a| and |b|.
 */
static int8_t bigint_add_magnitudes(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    uint64_t max_limbs = (a->limb_count > b->limb_count) ? a->limb_count : b->limb_count;

    // We might need one extra limb for the final carry
    if (bigint_ensure_capacity(result, max_limbs + 1) == -1) {
        return -1;
    }

    uint64_t carry = 0;
    for (uint64_t i = 0; i < max_limbs; i++) {
        uint64_t val_a = (i < a->limb_count) ? a->limbs[i] : 0;
        uint64_t val_b = (i < b->limb_count) ? b->limbs[i] : 0;

        // sum = val_a + val_b + carry
        // We use two steps to safely detect overflow for the carry
        uint64_t sum        = val_a + val_b;
        uint64_t next_carry = (sum < val_a) ? 1 : 0;

        sum += carry;
        if (sum < carry) {
            next_carry++;
        }

        result->limbs[i] = sum;
        carry            = next_carry;
    }

    if (carry) {
        result->limbs[max_limbs] = carry;
        result->limb_count       = max_limbs + 1;
    } else {
        result->limb_count = max_limbs;
    }

    bigint_normalize(result);
    return 0;
}

int8_t bigint_sub(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Normalize inputs if they are in two's complement
    const bigint_t * op_a = a;
    const bigint_t * op_b = b;
    bigint_t * tmp_a      = NULL;
    bigint_t * tmp_b      = NULL;

    if (!a->neged_with_sign && a->sign < 0) {
        tmp_a = bigint_create();

        if (!tmp_a) {
            return -1;
        }

        if(bigint_copy(tmp_a, a) == -1) {
            bigint_destroy(tmp_a);
            return -1;
        }
        if(bigint_ensure_normal_form(tmp_a) == -1) {
            bigint_destroy(tmp_a);
            return -1;
        }
        op_a = tmp_a;
    }
    if (!b->neged_with_sign && b->sign < 0) {
        tmp_b = bigint_create();

        if (!tmp_b) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            return -1;
        }

        if(bigint_copy(tmp_b, b) == -1) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            bigint_destroy(tmp_b);
            return -1;
        }
        if(bigint_ensure_normal_form(tmp_b) == -1) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            bigint_destroy(tmp_b);
            return -1;
        }
        op_b = tmp_b;
    }

    // 2. Standard Subtraction Logic
    int8_t status = 0;
    if (op_b->sign == 0) {
        status = bigint_copy(result, op_a);
    } else if (op_a->sign == 0) {
        status = bigint_copy(result, op_b);
        if (status == 0) {
            result->sign = -op_b->sign;
        }
    } else if (op_a->sign != op_b->sign) {
        // Different signs: (-a) - (+b) = -(a+b) or (+a) - (-b) = +(a+b)
        status = bigint_add_magnitudes(result, op_a, op_b);
        if (status == 0) {
            result->sign = op_a->sign;
        }
    } else {
        // Same signs: (+a) - (+b) or (-a) - (-b)
        int8_t cmp = bigint_cmp_magnitudes(op_a, op_b);
        if (cmp == 0) {
            if(bigint_set_zero(result) == -1) {
                status = -1;
            }
        } else if (cmp > 0) {
            status = bigint_sub_magnitudes(result, op_a, op_b);
            if (status == 0) {
                result->sign = op_a->sign;
            }
        } else {
            status = bigint_sub_magnitudes(result, op_b, op_a);
            if (status == 0) {
                result->sign = -op_a->sign;
            }
        }
    }

    if (tmp_a) {
        bigint_destroy(tmp_a);
    }

    if (tmp_b) {
        bigint_destroy(tmp_b);
    }

    result->neged_with_sign = true;
    return status;
}

int8_t bigint_add(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. If inputs are in Two's Complement, we must normalize them.
    // Since a and b are const, if they aren't normal, we must copy them
    // to a temporary bigint to normalize.
    const bigint_t * op_a = a;
    const bigint_t * op_b = b;
    bigint_t * tmp_a      = NULL;
    bigint_t * tmp_b      = NULL;

    if (!a->neged_with_sign && a->sign < 0) {
        tmp_a = bigint_create(); // Use your library's creator

        if (!tmp_a) {
            return -1;
        }

        if(bigint_copy(tmp_a, a) == -1) {
            bigint_destroy(tmp_a);
            return -1;
        }
        if(bigint_ensure_normal_form(tmp_a) == -1) {
            bigint_destroy(tmp_a);
            return -1;
        }
        op_a = tmp_a;
    }
    if (!b->neged_with_sign && b->sign < 0) {
        tmp_b = bigint_create();

        if (!tmp_b) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            return -1;
        }

        if(bigint_copy(tmp_b, b) == -1) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            bigint_destroy(tmp_b);
            return -1;
        }
        if(bigint_ensure_normal_form(tmp_b) == -1) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            bigint_destroy(tmp_b);
            return -1;
        }
        op_b = tmp_b;
    }

    // 2. Standard Sign-Magnitude Addition Logic
    int8_t status = 0;
    if (op_a->sign == 0) {
        status = bigint_copy(result, op_b);
    } else if (op_b->sign == 0) {
        status = bigint_copy(result, op_a);
    } else if (op_a->sign == op_b->sign) {
        status = bigint_add_magnitudes(result, op_a, op_b);
        if (status == 0) {
            result->sign = op_a->sign;
        }
    } else {
        // Signs differ: Use magnitude subtraction
        int8_t cmp = bigint_cmp_magnitudes(op_a, op_b);
        if (cmp == 0) {
            if(bigint_set_zero(result) == -1) {
                status = -1;
            }
        } else if (cmp > 0) {
            status = bigint_sub_magnitudes(result, op_a, op_b);
            if (status == 0) {
                result->sign = op_a->sign;
            }
        } else {
            status = bigint_sub_magnitudes(result, op_b, op_a);
            if (status == 0) {
                result->sign = op_b->sign;
            }
        }
    }

    // 3. Cleanup temps
    if (tmp_a) {
        bigint_destroy(tmp_a);
    }

    if (tmp_b) {
        bigint_destroy(tmp_b);
    }

    result->neged_with_sign = true;
    return status;
}

uint64_t bigint_bit_length(const bigint_t* bigint) {
    if (!bigint || bigint->sign == 0 || bigint->limb_count == 0) {
        return 0;
    }

    size_t i = bigint->limb_count - 1;
    while (i > 0 && bigint->limbs[i] == 0) {
        i--;
    }

    if (bigint->limbs[i] == 0) {
        return 0;
    }

    return (i * BIGINT_LIMB_BITS) + bit_most_significant(bigint->limbs[i]);
}

int8_t bigint_mul(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Handle zero cases
    if (a->sign == 0 || b->sign == 0) {
        return bigint_set_zero(result);
    }

    // 2. Normalize inputs if they are in two's complement
    // Multiplication MUST be done on magnitudes.
    const bigint_t * op_a = a;
    const bigint_t * op_b = b;
    bigint_t * tmp_a      = NULL;
    bigint_t * tmp_b      = NULL;

    if (!a->neged_with_sign && a->sign < 0) {
        tmp_a = bigint_create();
        if(bigint_copy(tmp_a, a) == -1) {
            return -1;
        }
        if(bigint_ensure_normal_form(tmp_a) == -1) {
            bigint_destroy(tmp_a);
            return -1;
        }
        op_a = tmp_a;
    }
    if (!b->neged_with_sign && b->sign < 0) {
        tmp_b = bigint_create();
        if(bigint_copy(tmp_b, b) == -1) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            return -1;
        }
        if(bigint_ensure_normal_form(tmp_b) == -1) {
            if (tmp_a) {
                bigint_destroy(tmp_a);
            }
            bigint_destroy(tmp_b);
            return -1;
        }
        op_b = tmp_b;
    }

    // 3. Prepare the result
    // Max limbs required for a * b is a->limb_count + b->limb_count
    uint64_t total_limbs = op_a->limb_count + op_b->limb_count;

    // If result is the same as a or b, we need a temporary buffer to avoid corruption
    uint64_t* res_limbs;
    boolean_t use_temp_res = (result == a || result == b);

    if (use_temp_res) {
        res_limbs = (uint64_t*)memory_malloc(total_limbs * sizeof(uint64_t));
        if (!res_limbs) {
            return -1;
        }
    } else {
        if (bigint_ensure_capacity(result, total_limbs) == -1) {
            return -1;
        }
        res_limbs = result->limbs;
    }
    memory_memclean(res_limbs, total_limbs * sizeof(uint64_t));

    // 4. Nested Loop Multiplication (Comba/Schoolbook)
    for (uint64_t i = 0; i < op_a->limb_count; i++) {
        uint64_t carry = 0;
        for (uint64_t j = 0; j < op_b->limb_count; j++) {
            // Use 128-bit math for the 64x64 multiply + accumulation
            uint128_t product = (uint128_t)op_a->limbs[i] * op_b->limbs[j];
            product += res_limbs[i + j];
            product += carry;

            res_limbs[i + j] = (uint64_t)product; // Lower 64 bits
            carry            = (uint64_t)(product >> 64); // Upper 64 bits
        }
        res_limbs[i + op_b->limb_count] = carry;
    }

    // 5. Finalize Result
    if (use_temp_res) {
        memory_free(result->limbs);
        result->limbs    = res_limbs;
        result->capacity = total_limbs;
    }

    result->limb_count      = total_limbs;
    result->sign            = a->sign * b->sign;
    result->neged_with_sign = true;

    // Cleanup inputs
    if (tmp_a) {
        bigint_destroy(tmp_a);
    }

    if (tmp_b) {
        bigint_destroy(tmp_b);
    }

    bigint_normalize(result);
    return 0;
}

int8_t bigint_pow(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Handle special cases for exponent b
    if (b->sign == 0) {
        return bigint_set_int64(result, 1); // a^0 = 1
    }
    if (b->sign < 0) {
        return bigint_set_zero(result); // a^-n = 0 for integers
    }

    // 2. Handle special cases for base a
    if (a->sign == 0) {
        return bigint_set_zero(result); // 0^b = 0
    }

    int8_t err = -1;

    // 3. Normalize inputs (ensure magnitudes are accessible)
    bigint_t* base = bigint_create();
    bigint_t* exp  = bigint_create();
    bigint_t* res  = bigint_create();

    if(bigint_copy(base, a) != 0) {
        goto cleanup;
    }
    if(bigint_copy(exp, b) != 0) {
        goto cleanup;
    }
    if(bigint_ensure_normal_form(base) != 0) {
        goto cleanup;
    }
    if(bigint_ensure_normal_form(exp) != 0) {
        goto cleanup;
    }

    // 4. Square-and-Multiply Algorithm
    if(bigint_set_int64(res, 1) == -1) {
        goto cleanup;
    }

    while (exp->sign > 0) {
        // If exp is odd (bit 0 is 1)
        if (exp->limbs[0] & 1) {
            if (bigint_mul(res, res, base) == -1) {
                goto cleanup;
            }
        }

        // base = base * base
        if (bigint_mul(base, base, base) == -1) {
            goto cleanup;
        }

        // exp = exp >> 1
        if (bigint_shr_one(exp) == -1) {
            goto cleanup;
        }
    }

    // 5. Finalize Sign
    // Negative base raised to odd exponent is negative
    if (a->sign < 0 && (b->limbs[0] & 1)) {
        res->sign = -1;
    } else {
        res->sign = 1;
    }

    if(bigint_copy(result, res) == -1) {
        goto cleanup;
    }

    err = 0;

cleanup:
    bigint_destroy(base);
    bigint_destroy(exp);
    bigint_destroy(res);
    return err;
}

int8_t bigint_add_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m) {
    if (!result || !a || !b || !m || m->sign == 0) {
        return -1;
    }

    // 1. Perform standard addition
    if (bigint_add(result, a, b) == -1) {
        return -1;
    }

    // 2. Optimization: If result >= m, result = result - m
    // This replaces the expensive bigint_mod (division)
    if (bigint_cmp_magnitudes(result, m) >= 0) {
        if (bigint_sub(result, result, m) == -1) {
            return -1;
        }
    }

    // Safety: ensure it handles negative results if inputs weren't reduced
    if (result->sign < 0) {
        return bigint_mod(result, result, m);
    }

    return 0;
}

int8_t bigint_sub_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m) {
    if (!result || !a || !b || !m || m->sign == 0) {
        return -1;
    }

    // 1. Perform standard subtraction
    if (bigint_sub(result, a, b) == -1) {
        return -1;
    }

    // 2. Optimization: If result < 0, result = result + m
    if (result->sign < 0) {
        if (bigint_add(result, result, m) == -1) {
            return -1;
        }
    }

    // 3. Final check: ensure result < m (in case inputs were large)
    if (bigint_cmp_magnitudes(result, m) >= 0) {
        return bigint_mod(result, result, m);
    }

    return 0;
}

int8_t bigint_mul_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m) {
    if(!result || !a || !b || !m) {
        return -1;
    }

    if (m->sign == 0) {
        return -1;
    }


    if(a->sign == 0 || b->sign == 0) {
        if(a == result) {
            return 0;
        }

        bigint_destroy_limbs(result); // 4 limbs and all set to 0

        return 0;
    }

    if (bigint_mul(result, a, b) == -1) {
        return -1;
    }

    if (bigint_mod(result, result, m) == -1) {
        return -1;
    }

    return 0;
}

int8_t bigint_pow_mod(bigint_t* result, const bigint_t* a, const bigint_t* b, const bigint_t* m) {
    if(!result || !a || !b || !m) {
        return -1;
    }

    bigint_t* orig_result      = result;
    boolean_t orig_result_is_a = false;

    if (result == a) {
        result = bigint_create();

        if (!result) {
            return -1;
        }

        orig_result_is_a = true;
    }else {
        bigint_destroy_limbs(result);
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
            if(bigint_set_bigint(orig_result, result) == -1) {
                bigint_destroy(result);
                return -1;
            }
            bigint_destroy(result);
        }

        return 0;
    }

    if (b->sign == 0) {
        result->sign = 1;
        if(bigint_set_int64(result, 1) == -1) {
            if (orig_result_is_a) {
                bigint_destroy(result);
            }
            return -1;
        }

        if (orig_result_is_a) {
            if(bigint_set_bigint(orig_result, result) == -1) {
                bigint_destroy(result);
                return -1;
            }
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

    if (bigint_mod(base, base, m) == -1) {
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
            if (bigint_mul_mod(result, result, base, m) == -1) {
                if (orig_result_is_a) {
                    bigint_destroy(result);
                }

                bigint_destroy(base);
                bigint_destroy(exp);
                return -1;
            }
        }

        if (bigint_mul_mod(base, base, base, m) == -1) {
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

    bigint_normalize(result);

    if (orig_result_is_a) {
        if(bigint_set_bigint(orig_result, result) == -1) {
            bigint_destroy(result);
            return -1;
        }
        bigint_destroy(result);
    }

    return 0;
}

int8_t bigint_div_with_remainder(bigint_t* result, bigint_t* remainder, const bigint_t* a, const bigint_t* b) {
    if (!a || !b || b->sign == 0) {
        return -1;
    }

    if (a->sign == 0) {
        if (result) {
            if(bigint_set_zero(result) == -1) {
                return -1;
            }
        }

        if (remainder) {
            if(bigint_set_zero(remainder) == -1) {
                return -1;
            }
        }

        return 0;
    }

    int err = -1;

    bigint_t* op_a = NULL;
    bigint_t* op_b = NULL;
    bigint_t* q    = NULL;
    bigint_t* r    = NULL;

    // Normalizing inputs
    op_a = bigint_create();
    if(bigint_copy(op_a, a) == -1) {
        bigint_destroy(op_a);
        return -1;
    }
    if(bigint_ensure_normal_form(op_a) == -1) {
        bigint_destroy(op_a);
        return -1;
    }

    op_b = bigint_create();
    if(bigint_copy(op_b, b) == -1) {
        bigint_destroy(op_a);
        bigint_destroy(op_b);
        return -1;
    }
    if(bigint_ensure_normal_form(op_b) == -1) {
        bigint_destroy(op_a);
        bigint_destroy(op_b);
        return -1;
    }

    // Prepare internal outputs
    q = bigint_create();
    r = bigint_create();

    if (bigint_div_unsigned(q, r, op_a, op_b) == -1) {
        goto cleanup;
    }

    // Safely assign result
    if (result) {
        if(bigint_copy(result, q) == -1) {
            goto cleanup;
        }
        result->sign            = a->sign * b->sign;
        result->neged_with_sign = true;
    }

    // Safely assign remainder only if pointer is NOT NULL
    if (remainder) {
        if(bigint_copy(remainder, r) == -1) {
            goto cleanup;
        }
        remainder->sign            = a->sign;
        remainder->neged_with_sign = true;
    }

    err = 0;

cleanup:
    bigint_destroy(op_a);
    bigint_destroy(op_b);
    bigint_destroy(q);
    bigint_destroy(r);

    return err;
}

int8_t bigint_div_unsigned(bigint_t* quotient, bigint_t* remainder, const bigint_t* a, const bigint_t* b) {
    if (!quotient || !a || !b) {
        return -1;
    }

    // 1. Handle NULL remainder by creating a local temporary one
    bigint_t* internal_rem = remainder;
    boolean_t destroy_rem  = false;

    if (remainder == NULL) {
        internal_rem = bigint_create();
        if (!internal_rem) {
            return -1;
        }

        destroy_rem = true;
    }

    bigint_t* internal_quotient = quotient;
    boolean_t destroy_quotient  = false;

    if (quotient == NULL) {
        internal_quotient = bigint_create();
        if (!internal_quotient) {
            if (destroy_rem) {
                bigint_destroy(internal_rem);
            }
            return -1;
        }
        destroy_quotient = true;
    }

    // 2. Initial comparison
    int8_t cmp = bigint_cmp_magnitudes(a, b);
    if (cmp < 0) {
        if(bigint_set_zero(internal_quotient) == -1) {
            goto fail;
        }
        if(bigint_copy(internal_rem, a) == -1) {
            goto fail;
        }
        goto success;
    }
    if (cmp == 0) {
        if(bigint_set_int64(internal_quotient, 1) == -1) {
            goto fail;
        }
        if(bigint_set_zero(internal_rem) == -1) {
            goto fail;
        }
        goto success;
    }

    // 3. Setup for bitwise division
    if(bigint_set_zero(internal_quotient) == -1) {
        goto fail;
    }
    if(bigint_set_zero(internal_rem) == -1) {
        goto fail;
    }

    uint64_t a_bits = bigint_bit_length(a) + 1; // +1 to include the highest bit
    if (bigint_ensure_capacity(quotient, a->limb_count) == -1) {
        goto fail;
    }
    memory_memclean(quotient->limbs, quotient->capacity * sizeof(uint64_t));

    // 4. Main Loop
    for (int64_t i = (int64_t)a_bits - 1; i >= 0; i--) {
        // Shift remainder up to make room for the next bit
        if (bigint_shl_one(internal_rem) == -1) {
            goto fail;
        }

        // Extract i-th bit from 'a' and put it in remainder's LSB
        boolean_t bit_value;
        if (bigint_get_bit(a, (uint64_t)i, &bit_value) == -1) {
            goto fail;
        }

        if (bigint_set_bit(internal_rem, 0, bit_value) == -1) {
            goto fail;
        }

        // Critical: normalization is required for cmp_magnitudes to work!
        bigint_normalize(internal_rem);

        if (bigint_cmp_magnitudes(internal_rem, b) >= 0) {
            // remainder -= b
            if (bigint_sub_magnitudes(internal_rem, internal_rem, b) == -1) {
                goto fail;
            }

            // Set bit in quotient
            if (bigint_set_bit(internal_quotient, (uint64_t)i, true) == -1) {
                goto fail;
            }
        }
    }

success:
    bigint_normalize(internal_quotient);
    if (quotient != internal_quotient) {
        if(bigint_copy(quotient, internal_quotient) == -1) {
            goto fail;
        }
    }

    bigint_normalize(internal_rem);
    if (remainder != internal_rem) {
        if(bigint_copy(remainder, internal_rem) == -1) {
            goto fail;
        }
    }

    if (destroy_rem) {
        bigint_destroy(internal_rem);
    }

    if (destroy_quotient) {
        bigint_destroy(internal_quotient);
    }

    return 0;

fail:
    if (destroy_rem) {
        bigint_destroy(internal_rem);
    }

    if (destroy_quotient) {
        bigint_destroy(internal_quotient);
    }

    return -1;
}

int8_t bigint_div(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    return bigint_div_with_remainder(result, NULL, a, b);
}

int8_t bigint_mod(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // Use a temporary to handle cases where result is the same as a or b
    bigint_t* tmp_remainder = bigint_create();
    if (!tmp_remainder) {
        return -1;
    }

    // bigint_div_with_remainder already calculates the remainder r
    // where sign(r) == sign(a).
    // result is NULL because we don't care about the quotient here.
    if (bigint_div_with_remainder(NULL, tmp_remainder, a, b) == -1) {
        bigint_destroy(tmp_remainder);
        return -1;
    }

    // Since we are using C-style truncated division:
    // a % b has the same sign as a.
    // div_with_remainder already handles this correctly.

    // Ensure the output is in normal form (Sign-Magnitude)
    bigint_normalize(tmp_remainder);
    if(bigint_ensure_normal_form(tmp_remainder) == -1) {
        bigint_destroy(tmp_remainder);
        return -1;
    }

    if (bigint_copy(result, tmp_remainder) == -1) {
        bigint_destroy(tmp_remainder);
        return -1;
    }

    bigint_destroy(tmp_remainder);
    return 0;
}

int8_t bigint_gcd(bigint_t* result, const bigint_t* a, const bigint_t* b) {
    if (!result || !a || !b) {
        return -1;
    }

    // 1. Handle special cases: GCD(0, x) = |x|, GCD(x, 0) = |x|
    if (a->sign == 0) {
        return bigint_abs_copy(result, b);
    }

    if (b->sign == 0) {
        return bigint_abs_copy(result, a);
    }

    int err = -1;

    // 2. Prepare temporary variables for the Euclidean loop
    // We use magnitudes because GCD is always positive.
    bigint_t* tmp_a = bigint_clone(a);
    bigint_t* tmp_b = bigint_clone(b);
    bigint_t* rem   = bigint_create();

    if (!tmp_a || !tmp_b || !rem) {
        goto cleanup;
    }

    // Ensure they are in normal form and positive
    if(bigint_ensure_normal_form(tmp_a) == -1) {
        goto cleanup;
    }
    if(bigint_ensure_normal_form(tmp_b) == -1) {
        goto cleanup;
    }
    tmp_a->sign = 1;
    tmp_b->sign = 1;

    // 3. Euclidean Algorithm Loop
    // Logic: while a != 0: b = b % a, then swap(a, b)
    while (tmp_a->sign != 0) {
        if (bigint_mod(rem, tmp_b, tmp_a) == -1) {
            goto cleanup;
        }

        // tmp_b = tmp_a
        if(bigint_copy(tmp_b, tmp_a) == -1) {
            goto cleanup;
        }
        // tmp_a = rem
        if(bigint_copy(tmp_a, rem) == -1) {
            goto cleanup;
        }
    }

    // 4. Finalize result
    // The GCD is stored in tmp_b
    if(bigint_copy(result, tmp_b) == -1) {
        goto cleanup;
    }
    result->sign            = 1;
    result->neged_with_sign = true;

    err = 0;

cleanup:
    // 5. Cleanup
    bigint_destroy(tmp_a);
    bigint_destroy(tmp_b);
    bigint_destroy(rem);

    return err;
}

int8_t bigint_isqrt(bigint_t* result, const bigint_t* a) {
    if (!result || !a || a->sign < 0) {
        return -1; // Square root of negative is error

    }
    if (a->sign == 0) {
        return bigint_set_zero(result);
    }

    int8_t err = -1;

    // Initial Guess: 2^(bit_length / 2)
    bigint_t* x   = bigint_create();
    bigint_t* y   = bigint_create();
    bigint_t* tmp = bigint_create();
    if (!x || !y || !tmp) {
        goto cleanup;
    }

    // Use a starting guess of 2^((bits/2)+1)
    uint64_t bits = bigint_bit_length(a);
    if(bigint_set_bit(x, (bits / 2) + 1, true) == -1) {
        goto cleanup;
    }

    while (true) {
        // y = a / x
        if (bigint_div(y, a, x) == -1) {
            goto cleanup;
        }

        // tmp = x + y
        if (bigint_add(tmp, x, y) == -1) {
            goto cleanup;
        }

        // y = tmp >> 1 (Divide by 2)
        if (bigint_shr_one(tmp) == -1) {
            goto cleanup;
        }

        if (bigint_copy(y, tmp) == -1) {
            goto cleanup;
        }

        // If y >= x, we have converged
        if (bigint_cmp_magnitudes(y, x) >= 0) {
            break;
        }

        if(bigint_copy(x, y) == -1) {
            goto cleanup;
        }
    }

    if(bigint_copy(result, x) == -1) {
        goto cleanup;
    }

    err = 0;

cleanup:
    if(x) {
        bigint_destroy(x);
    }
    if(y) {
        bigint_destroy(y);
    }
    if(tmp) {
        bigint_destroy(tmp);
    }
    return err;
}

int8_t bigint_mod_inv(bigint_t* result, const bigint_t* a, const bigint_t* n) {
    if (!result || !a || !n || bigint_is_zero(n)) {
        return -1;
    }

    // GCD is only defined for positive magnitudes in this context
    if (a->sign < 0) {
        bigint_t* tmp_a = bigint_clone(a);
        if (!tmp_a) {
            return -1;
        }
        if(bigint_mod(tmp_a, tmp_a, n) != 0) { // Bring into range [0, n-1]
            bigint_destroy(tmp_a);
            return -1;
        }
        int8_t ret = bigint_mod_inv(result, tmp_a, n);
        bigint_destroy(tmp_a);
        return ret;
    }

    int8_t err = -1;

    bigint_t * t    = bigint_create(); // Coefficient t
    bigint_t * newt = bigint_create();
    bigint_t * r    = bigint_clone(n); // Remainder r
    bigint_t * newr = bigint_clone(a);
    bigint_t * q    = bigint_create();
    bigint_t * tmp  = bigint_create();
    bigint_t * prod = bigint_create();

    if (!t || !newt || !r || !newr || !q || !tmp || !prod) {
        goto cleanup;
    }

    if(bigint_set_int64(t, 0) == -1) {
        goto cleanup;
    }
    if(bigint_set_int64(newt, 1) == -1) {
        goto cleanup;
    }

    // Standard Extended Euclidean Algorithm
    while (!bigint_is_zero(newr)) {
        // Quotient q = r / newr, Remainder tmp = r % newr
        if (bigint_div_with_remainder(q, tmp, r, newr) == -1) {
            goto cleanup;
        }

        // r = newr, newr = tmp
        if(bigint_copy(r, newr) == -1) {
            goto cleanup;
        }
        if(bigint_copy(newr, tmp) == -1) {
            goto cleanup;
        }

        // (t, newt) = (newt, t - q * newt)
        if(bigint_copy(tmp, t) != 0) { // tmp = old_t
            goto cleanup;
        }
        if(bigint_copy(t, newt) != 0) {
            goto cleanup;
        }

        if (bigint_mul(prod, q, newt) == -1) {
            goto cleanup;
        }
        if (bigint_sub(newt, tmp, prod) == -1) {
            goto cleanup;
        }
    }

    // If r > 1, then a is not invertible (gcd(a, n) != 1)
    if (!bigint_is_int64(r, 1)) {
        goto cleanup;
    }

    // If t is negative, add n to make it positive
    if (bigint_is_negative(t)) {
        if (bigint_add(t, t, n) == -1) {
            goto cleanup;
        }
    }

    if(bigint_copy(result, t) == -1) {
        goto cleanup;
    }

    err = 0;

cleanup:
    if(t) {
        bigint_destroy(t);
    }
    if(newt) {
        bigint_destroy(newt);
    }
    if(r) {
        bigint_destroy(r);
    }
    if(newr) {
        bigint_destroy(newr);
    }
    if(q) {
        bigint_destroy(q);
    }
    if(tmp) {
        bigint_destroy(tmp);
    }
    if(prod) {
        bigint_destroy(prod);
    }
    return err;
}

int8_t bigint_mod_sqrt(bigint_t* result, const bigint_t* a, const bigint_t* p) {
    if (!result || !a || !p || bigint_is_even(p)) {
        return -1;
    }
    if (bigint_is_zero(a)) {
        if(bigint_set_zero(result) == -1) {
            return -1;
        }
        return 0;
    }

    int8_t err = -1;

    bigint_t* exp         = NULL;
    bigint_t* check       = NULL;
    bigint_t* Q           = NULL;
    bigint_t* z           = NULL;
    bigint_t* p_minus_one = NULL;
    bigint_t* M           = NULL;
    bigint_t* c           = NULL;
    bigint_t* t           = NULL;
    bigint_t* R           = NULL;

    // Legendre symbol check: a^((p-1)/2) % p must be 1
    exp   = bigint_create();
    check = bigint_create();

    if (!exp || !check) {
        goto cleanup;
    }

    if(bigint_copy(exp, p) != 0) {
        goto cleanup;
    }

    if(bigint_sub_uint64(exp, 1) != 0) {
        goto cleanup;
    }

    if(bigint_shr_one(exp) != 0) {
        goto cleanup;
    }

    if(bigint_pow_mod(check, a, exp, p) != 0) {
        goto cleanup;
    }

    if (!bigint_is_uint64(check, 1)) {
        printf("No square root exists for the given input.\n");
        err = -2; // Not a quadratic residue (no solution)
        goto cleanup;
    }

    // 1. Factor p-1 = Q * 2^S
    uint64_t S = 0;
    Q = bigint_clone(exp); // exp is already (p-1)/2, so we start there

    if (!Q) {
        goto cleanup;
    }

    S = 1;
    while (bigint_is_even(Q)) {
        if(bigint_shr_one(Q) == -1) {
            goto cleanup;
        }
        S++;
    }

    // 2. Find a non-residue 'z'
    z = bigint_two();
    if (!z) {
        goto cleanup;
    }

    p_minus_one = bigint_clone(p);
    if (!p_minus_one) {
        goto cleanup;
    }

    if(bigint_sub_uint64(p_minus_one, 1) != 0) {
        goto cleanup;
    }

    while (true) {
        if(bigint_pow_mod(check, z, exp, p) != 0) { // Reuse (p-1)/2 exp
            goto cleanup;
        }
        if (bigint_cmp(check, p_minus_one) == 0) {
            // If check == p-1 (which is exp * 2), it's a non-residue
            // Note: in mod p, p-1 is -1.
            break;
        }
        if(bigint_add_uint64(z, 1) != 0) {
            goto cleanup;
        }
    }

    // 3. Initialize variables
    M = bigint_create();
    c = bigint_create();
    t = bigint_create();
    R = bigint_create();

    if (!M || !c || !t || !R) {
        goto cleanup;
    }

    if(bigint_set_uint64(M, S) != 0) {
        goto cleanup;
    }

    if(bigint_pow_mod(c, z, Q, p) != 0) { // c = z^Q mod p
        goto cleanup;
    }

    if(bigint_pow_mod(t, a, Q, p) != 0) { // t = a^Q mod p
        goto cleanup;
    }

    if(bigint_set_bigint(exp, Q) != 0) {
        goto cleanup;
    }

    if(bigint_add_uint64(exp, 1) != 0) {
        goto cleanup;
    }

    if(bigint_shr_one(exp) != 0) {
        goto cleanup;
    }

    if(bigint_pow_mod(R, a, exp, p) != 0) { // R = a^((Q+1)/2) mod p
        goto cleanup;
    }

    // 4. Loop
    while (!bigint_is_uint64(t, 1)) {
        uint64_t i        = 0;
        bigint_t * temp_t = bigint_clone(t);

        uint64_t uint64_M = 0;

        if(bigint_get_uint64(M, &uint64_M) != 0) {
            bigint_destroy(temp_t);
            goto cleanup;
        }

        for (i = 1; i < uint64_M; i++) {
            if(bigint_mul_mod(temp_t, temp_t, temp_t, p) != 0) {
                bigint_destroy(temp_t);
                goto cleanup;
            }

            if (bigint_is_uint64(temp_t, 1)) {
                break;
            }
        }

        bigint_t * b = bigint_clone(c);

        if (!b) {
            bigint_destroy(temp_t);
            goto cleanup;
        }

        // exponent = 2^(M - i - 1)
        if(bigint_get_uint64(M, &uint64_M) != 0) {
            bigint_destroy(temp_t);
            bigint_destroy(b);
            goto cleanup;
        }
        uint64_t pow_count = uint64_M - i - 1;

        for (uint64_t j = 0; j < pow_count; j++) {
            if(bigint_mul_mod(b, b, b, p) != 0) {
                bigint_destroy(temp_t);
                bigint_destroy(b);
                goto cleanup;
            }
        }

        if(bigint_set_uint64(M, i) != 0) {
            bigint_destroy(temp_t);
            bigint_destroy(b);
            goto cleanup;
        }

        if(bigint_mul_mod(c, b, b, p) != 0) {
            bigint_destroy(temp_t);
            bigint_destroy(b);
            goto cleanup;
        }

        if(bigint_mul_mod(t, t, c, p) != 0) {
            bigint_destroy(temp_t);
            bigint_destroy(b);
            goto cleanup;
        }

        if(bigint_mul_mod(R, R, b, p) != 0) {
            bigint_destroy(temp_t);
            bigint_destroy(b);
            goto cleanup;
        }

        bigint_destroy(temp_t); bigint_destroy(b);
    }

    if(bigint_set_bigint(result, R) != 0) {
        goto cleanup;
    }

    err = 0;

    // Cleanup everything
cleanup:
    bigint_destroy(exp); bigint_destroy(check); bigint_destroy(Q);
    bigint_destroy(z); bigint_destroy(M); bigint_destroy(c);
    bigint_destroy(t); bigint_destroy(R); bigint_destroy(p_minus_one);

    return err;
}

int8_t bigint_add_uint64(bigint_t* a, uint64_t b) {
    if (!a) {
        return -1;
    }
    if (b == 0) {
        return 0;
    }
    if (a->sign == 0) {
        return bigint_set_uint64(a, b);
    }

    // If signs are different, we redirect to subtraction
    if (a->sign < 0) {
        a->sign = 1; // Temporarily make positive
        int8_t res = bigint_sub_uint64(a, b);
        if (a->sign != 0) {
            a->sign *= -1; // Restore/Flip sign
        }
        return res;
    }

    if (bigint_ensure_capacity(a, a->limb_count + 1) == -1) {
        return -1;
    }

    uint64_t carry = b;
    for (uint64_t i = 0; i < a->limb_count && carry > 0; i++) {
        uint64_t old = a->limbs[i];
        a->limbs[i] += carry;
        carry        = (a->limbs[i] < old) ? 1 : 0;
    }

    if (carry) {
        a->limbs[a->limb_count] = carry;
        a->limb_count++;
    }

    bigint_normalize(a);

    return 0;
}

int8_t bigint_sub_uint64(bigint_t* a, uint64_t b) {
    if (!a) {
        return -1;
    }
    if (b == 0) {
        return 0;
    }
    if (a->sign == 0) {
        int8_t res = bigint_set_uint64(a, b);
        a->sign = -1;
        return res;
    }

    // If signs different (Negative - Positive), it's actually an addition
    if (a->sign < 0) {
        a->sign = 1;
        int8_t res = bigint_add_uint64(a, b);
        a->sign = -1;
        return res;
    }

    // If a < b, result will be negative
    if (a->limb_count == 1 && a->limbs[0] < b) {
        a->limbs[0] = b - a->limbs[0];
        a->sign     = -1;
        return 0;
    }

    uint64_t borrow = b;
    for (uint64_t i = 0; i < a->limb_count && borrow > 0; i++) {
        uint64_t old = a->limbs[i];
        a->limbs[i] -= borrow;
        borrow       = (old < a->limbs[i]) ? 1 : 0;
    }

    bigint_normalize(a);

    return 0;
}

int8_t bigint_mul_uint64(bigint_t* a, uint64_t b) {
    if (!a) {
        return -1;
    }
    if (b == 0 || a->sign == 0) {
        return bigint_set_zero(a);
    }
    if (b == 1) {
        return 0;
    }

    if (bigint_ensure_capacity(a, a->limb_count + 1) == -1) {
        return -1;
    }

    uint64_t carry = 0;
    for (uint64_t i = 0; i < a->limb_count; i++) {
        uint128_t res = (uint128_t)a->limbs[i] * b + carry;
        a->limbs[i] = (uint64_t)res;
        carry       = (uint64_t)(res >> 64);
    }

    if (carry) {
        a->limbs[a->limb_count] = carry;
        a->limb_count++;
    }

    bigint_normalize(a);

    return 0;
}

static inline uint64_t bigint_mod128_by_64(uint64_t high, uint64_t low, uint64_t m) {
    uint64_t quotient; // We need a place for the quotient even if we ignore it
    uint64_t remainder;

    __asm__ (
        "divq %[divisor]"
        : "=a" (quotient), // %0: RAX receives quotient
        "=d" (remainder) // %1: RDX receives remainder
        : "a" (low), // %2: Input RAX
        "d" (high), // %3: Input RDX
        [divisor] "rm" (m) // %4: Can be register or memory
        : "cc"
        );

    return remainder;
}

uint64_t bigint_mod_uint64(const bigint_t* a, uint64_t m) {
    if (!a || m == 0 || a->sign == 0) {
        return 0;
    }
    if (m == 1) {
        return 0;
    }

    uint64_t rem = 0;
    for (int64_t i = (int64_t)a->limb_count - 1; i >= 0; i--) {
        rem = bigint_mod128_by_64(rem, a->limbs[i], m);
    }

    return rem;
}

BIGINT_CHECK_RESULT static bigint_t* bigint_random_internal(uint64_t bits, boolean_t force_msb) {
    if (bits == 0) {
        return bigint_create(); // Returns a zero bigint
    }

    bigint_t* result = bigint_create();
    if (!result) {
        return NULL;
    }

    uint64_t item_count = (bits + BIGINT_LIMB_BITS - 1) / BIGINT_LIMB_BITS;

    // Use your helper to ensure capacity and zero initialization
    if (bigint_ensure_capacity(result, item_count) == -1) {
        bigint_destroy(result);
        return NULL;
    }

    result->sign            = 1;
    result->limb_count      = item_count;
    result->neged_with_sign = true;

    // Fill all limbs with random data
    for (uint64_t i = 0; i < item_count; i++) {
        result->limbs[i] = rand64();
    }

    // --- Fix the MSB Limb ---

    // How many bits are actually used in the highest limb?
    // If bits = 64, used_in_msb = 64. If bits = 65, used_in_msb = 1.
    uint64_t used_in_msb = bits % BIGINT_LIMB_BITS;
    if (used_in_msb == 0) {
        used_in_msb = BIGINT_LIMB_BITS;
    }

    // 1. Mask out bits above the requested length
    // This ensures that even if force_msb is false, the number isn't larger than 'bits'
    if (used_in_msb < 64) {
        uint64_t mask = (1ULL << used_in_msb) - 1;
        result->limbs[item_count - 1] &= mask;
    }

    // 2. Force the MSB to be 1 if requested
    if (force_msb) {
        result->limbs[item_count - 1] |= (1ULL << (used_in_msb - 1));
    }

    // Final normalization in case the random bits resulted in leading zeros
    bigint_normalize(result);

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

BIGINT_CHECK_RESULT static boolean_t bigint_is_prime_miller_rabin(const bigint_t* a, uint64_t try) {
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

    boolean_t is_prime = false;

    bigint_t* one       = bigint_one();
    bigint_t* two       = bigint_two();
    bigint_t* n_minus_1 = NULL;
    bigint_t* d         = NULL;
    bigint_t* r         = NULL;
    bigint_t* x         = NULL;
    bigint_t* a_minus_2 = NULL;

    if(!one || !two) {
        goto cleanup;
    }

    n_minus_1 = bigint_clone(a);

    if (!n_minus_1) {
        goto cleanup;
    }

    if (bigint_sub(n_minus_1, n_minus_1, one) == -1) {
        goto cleanup;
    }

    d = bigint_clone(n_minus_1);

    if (!d) {
        goto cleanup;
    }

    r = bigint_create();

    if (!r) {
        goto cleanup;
    }

    while (bigint_is_even(d)) {
        if(bigint_add(r, r, one) == -1) {
            goto cleanup;
        }

        if (bigint_shr_one(d) == -1) {
            goto cleanup;
        }

        if(d->sign == 0) {
            break;
        }
    }

    if(bigint_sub(r, r, one) == -1) {
        goto cleanup;
    }

    bigint_t* r_minus_1 = r; // for readability

    x = bigint_create();

    if (!x) {
        goto cleanup;
    }

    a_minus_2 = bigint_clone(a);

    if (!a_minus_2) {
        goto cleanup;
    }

    if (bigint_sub(a_minus_2, a_minus_2, two) == -1) {
        goto cleanup;
    }

    // const char * str = NULL;

    for (uint64_t i = 0; i < try; i++) {
        bigint_t* test_random = bigint_random_range(two, a_minus_2);
        if (!test_random) {
            goto cleanup;
        }

        if (bigint_pow_mod(x, test_random, d, a) == -1) {
            bigint_destroy(test_random);
            goto cleanup;
        }

        if (!bigint_is_uint64(x, 1) && bigint_cmp(x, n_minus_1) != 0) {
            bigint_t* j = bigint_create();

            if (!j) {
                goto cleanup;
            }

            while(bigint_cmp(j, r_minus_1) == -1 && bigint_cmp(x, n_minus_1) != 0) {
                if (bigint_pow_mod(x, x, two, a) == -1) {
                    bigint_destroy(j);
                    bigint_destroy(test_random);
                    goto cleanup;
                }

                if (bigint_is_uint64(x, 1)) {
                    bigint_destroy(j);
                    bigint_destroy(test_random);
                    goto cleanup;
                }

                if(bigint_add(j, j, one) == -1) {
                    bigint_destroy(j);
                    bigint_destroy(test_random);
                    goto cleanup;
                }
            }

            if (bigint_cmp(x, n_minus_1) != 0) {
                bigint_destroy(j);
                bigint_destroy(test_random);
                goto cleanup;
            }

            bigint_destroy(j);
        }

        bigint_destroy(test_random);
    }

    is_prime = true;

cleanup:
    bigint_destroy(one);
    bigint_destroy(two);
    bigint_destroy(n_minus_1);
    bigint_destroy(d);
    bigint_destroy(r);
    bigint_destroy(x);
    bigint_destroy(a_minus_2);

    return is_prime;
}

boolean_t bigint_is_prime(const bigint_t* a) {
    if(!a) {
        return false;
    }

    return bigint_is_prime_miller_rabin(a, 128);
}

bigint_t* bigint_random_prime(uint64_t bits) {
    if (bits < 2) {
        return NULL;
    }

    // We need a constant '2' to increment our odd candidates
    bigint_t* two = bigint_two();
    if (!two) {
        return NULL;
    }

    while (true) {
        // Generate a random number with the MSB forced to 1 to ensure bit length
        bigint_t* result = bigint_random_internal(bits, true);
        if (!result) {
            bigint_destroy(two);
            return NULL;
        }

        // Ensure the candidate is odd
        result->limbs[0] |= 1;
        if (result->limb_count == 0) {
            result->limb_count = 1;
        }

        // Search in a local window of 1000 candidates (adding 2 each time)
        for (uint64_t i = 0; i < 1000; i++) {
            // Optimization: Trial division by small primes (like 3, 5, 7, 11...)
            // could be added here to fail 80% of composites instantly.

            if (bigint_is_prime(result)) {
                bigint_destroy(two);
                return result;
            }

            // result = result + 2 (keeps the candidate odd)
            if (bigint_add(result, result, two) == -1) {
                bigint_destroy(result);
                bigint_destroy(two);
                return NULL;
            }
        }

        // If no prime found in this window, pick a new random starting point
        bigint_destroy(result);
    }
}

int8_t bigint_to_bytes(const bigint_t* a, uint8_t* buf, uint64_t len) {
    if (!a || (!buf && len > 0)) {
        return -1;
    }

    if (len > 0) {
        memory_memclean(buf, len);
    }

    if (a->sign == 0) {
        return 0;
    }

    uint64_t required_bytes = (bigint_bit_length(a) + 1 + 8 - 1) / 8;

    if (len < required_bytes) {
        printf("Required bytes: %llu, Provided bytes: %llu\n", required_bytes, len);
        return -1; // Buffer too small
    }

    // Fill buffer from limbs (buffer is big-endian)
    for(uint64_t i = 0; i < required_bytes; i++) {
        uint64_t limb_index  = i / BIGINT_LIMB_BYTES;
        uint64_t byte_offset = i % BIGINT_LIMB_BYTES;
        buf[len - 1 - i] = (a->limbs[limb_index] >> (8 * byte_offset)) & 0xFF;
    }

    return 0;
}

int8_t bigint_from_bytes(bigint_t* a, const uint8_t* buf, uint64_t len) {
    if (!a || (!buf && len > 0)) {
        return -1;
    }

    bigint_destroy_limbs(a);

    if (len == 0) {
        return bigint_set_zero(a);
    }

    uint64_t required_limbs = (len + BIGINT_LIMB_BYTES - 1) / BIGINT_LIMB_BYTES;

    if (bigint_ensure_capacity(a, required_limbs) == -1) {
        return -1;
    }

    memory_memclean(a->limbs, a->capacity * sizeof(uint64_t));

    // Fill limbs from the byte buffer (buffer is big-endian)
    for(uint64_t i = 0; i < len; i++) {
        uint64_t byte        = buf[len - 1 - i];
        uint64_t limb_index  = i / BIGINT_LIMB_BYTES;
        uint64_t byte_offset = i % BIGINT_LIMB_BYTES;
        a->limbs[limb_index] |= (byte << (8 * byte_offset));
    }

    a->limb_count      = required_limbs;
    a->sign            = 1;
    a->neged_with_sign = true;

    bigint_normalize(a);
    return 0;
}

int8_t bigint_to_bytes_le(const bigint_t* a, uint8_t* buf, uint64_t len) {
    if (!a || (!buf && len > 0)) {
        return -1;
    }

    if (len > 0) {
        memory_memclean(buf, len);
    }

    if (a->sign == 0) {
        return 0;
    }

    uint64_t required_bytes = (bigint_bit_length(a) + 1 + 8 - 1) / 8;

    if (len < required_bytes) {
        return -1; // Buffer too small
    }

    // Fill buffer from limbs (buffer is little-endian)
    for(uint64_t i = 0; i < required_bytes; i++) {
        uint64_t limb_index  = i / BIGINT_LIMB_BYTES;
        uint64_t byte_offset = i % BIGINT_LIMB_BYTES;
        buf[i] = (a->limbs[limb_index] >> (8 * byte_offset)) & 0xFF;
    }

    return 0;
}

int8_t bigint_from_bytes_le(bigint_t* a, const uint8_t* buf, uint64_t len) {
    if (!a || (!buf && len > 0)) {
        return -1;
    }

    bigint_destroy_limbs(a);

    if (len == 0) {
        return bigint_set_zero(a);
    }

    uint64_t required_limbs = (len + BIGINT_LIMB_BYTES - 1) / BIGINT_LIMB_BYTES;

    if (bigint_ensure_capacity(a, required_limbs) == -1) {
        return -1;
    }

    memory_memclean(a->limbs, a->capacity * sizeof(uint64_t));

    // Fill limbs from the byte buffer (buffer is little-endian)
    for(uint64_t i = 0; i < len; i++) {
        uint64_t byte        = buf[i];
        uint64_t limb_index  = i / BIGINT_LIMB_BYTES;
        uint64_t byte_offset = i % BIGINT_LIMB_BYTES;
        a->limbs[limb_index] |= (byte << (8 * byte_offset));
    }

    a->limb_count      = required_limbs;
    a->sign            = 1;
    a->neged_with_sign = true;

    bigint_normalize(a);
    return 0;
}

int8_t bigint_get_uint64(const bigint_t* a, uint64_t* value) {
    if (!a || !value) {
        return -1;
    }
    if (a->sign == 0) {
        *value = 0;
        return 0;
    }
    if (a->limb_count > 1) {
        return -1; // Too large to fit in uint64_t
    }
    *value = a->limbs[0];
    return 0;
}

int8_t bigint_cswap(bigint_t* a, bigint_t* b, uint8_t swap) {
    if (!a || !b) {
        return -1;
    }
    // Ensure swap is exactly 0 or 1 for the mask to work
    swap = (swap != 0);

    uint64_t mask64 = -(uint64_t)swap;
    uint8_t mask8   = -(uint8_t)swap;

    // 1. Swap Limbs
    // Note: We use the actual limb_count used by the bigints
    // In X25519, these are always 4 limbs (256 bits)
    uint64_t max_limbs = (a->limb_count > b->limb_count) ? a->limb_count : b->limb_count;
    if(max_limbs > a->capacity || max_limbs > b->capacity) {
        return -1; // Safety check to avoid OOB
    }
    for (uint64_t i = 0; i < max_limbs; i++) {
        uint64_t tmp = (a->limbs[i] ^ b->limbs[i]) & mask64;
        a->limbs[i] ^= tmp;
        b->limbs[i] ^= tmp;
    }

    // 2. Swap limb_count
    uint64_t tmp_cnt = (a->limb_count ^ b->limb_count) & mask64;
    a->limb_count ^= tmp_cnt;
    b->limb_count ^= tmp_cnt;

    // 3. Swap Sign (Treat as raw bytes to avoid signed overflow/bit issues)
    uint8_t* a_sign = (uint8_t*)&a->sign;
    uint8_t* b_sign = (uint8_t*)&b->sign;
    uint8_t tmp_s   = (*a_sign ^ *b_sign) & mask8;
    *a_sign ^= tmp_s;
    *b_sign ^= tmp_s;

    // 4. Swap neged_with_sign (Treat as raw bytes)
    uint8_t* a_neg = (uint8_t*)&a->neged_with_sign;
    uint8_t* b_neg = (uint8_t*)&b->neged_with_sign;
    uint8_t tmp_n  = (*a_neg ^ *b_neg) & mask8;
    *a_neg ^= tmp_n;
    *b_neg ^= tmp_n;

    return 0;
}
