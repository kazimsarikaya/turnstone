/**
 * @file test_bigint.c
 * @brief Big integer arithmetic
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <bigint.h>
#include <strings.h>

int32_t main(void);

#define BIGINT_SET_STR(bi, val) \
        do { \
            if(bigint_set_str(bi, val) != 0) { \
                print_error("BIGINT_SET_STR failed for " val); \
                return -1; \
            } \
        } while(0)

#define BIGINT_SET_INT64(bi, val) \
        do { \
            if(bigint_set_int64(bi, val) != 0) { \
                print_error("BIGINT_SET_INT64 failed for " #val); \
                return -1; \
            } \
        } while(0)

#define BIGINT_SET_UINT64(bi, val) \
        do { \
            if(bigint_set_uint64(bi, val) != 0) { \
                print_error("BIGINT_SET_UINT64 failed for " #val); \
                return -1; \
            } \
        } while(0)

#define BIGINT_TEST(name, expected_str, res, ...) \
        do { \
            if(bigint_ ## name(res, ## __VA_ARGS__) == -1) { \
                print_error("bigint_" #name " failed"); \
                return -1; \
            } \
            str = bigint_to_str(res); \
            if (str) { \
                printf("bigint_" #name ": %s\n", str); \
                if(strncmp(str, expected_str, strlen(expected_str)) != 0 || strlen(str) != strlen(expected_str)) { \
                    print_error("bigint_" #name " failed, expected " expected_str " got %s", str); \
                    memory_free((void*)str); \
                    return -1; \
                } else { \
                    print_success("bigint_" #name " passed"); \
                } \
                memory_free((void*)str); \
            } else { \
                print_error("bigint_" #name " failed"); \
                return -1; \
            } \
        } while(0)

#define BIGINT_STR_TEST(name, expected_str, res, ...) \
        do { \
            str = bigint_to_str(res); \
            if (str) { \
                printf("bigint_" #name ": %s\n", str); \
                if(strncmp(str, expected_str, strlen(expected_str)) != 0 || strlen(str) != strlen(expected_str)) { \
                    print_error("bigint_" #name " failed, expected " expected_str " got %s", str); \
                    memory_free((void*)str); \
                    return -1; \
                } else { \
                    print_success("bigint_" #name " passed"); \
                } \
                memory_free((void*)str); \
            } else { \
                print_error("bigint_" #name " failed"); \
                return -1; \
            } \
        } while(0)

#define auto_destroy(Type, var) \
        __attribute__((cleanup(_auto_destroy_ ## Type))) Type* var = NULL

static void _auto_destroy_bigint_t(bigint_t** bigint) {
    if(bigint && *bigint) {
        bigint_destroy(*bigint);
        *bigint = NULL;
    }
}

static int32_t bigint_test_set_int(void) {
    auto_destroy(bigint_t, bigint);

    bigint = bigint_create();

    if (!bigint) {
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint);
    bigint = bigint_create();

    char_t* str = NULL;

    if(bigint) {
        BIGINT_SET_INT64(bigint, 0x1234567890ABCDEFLL);

        BIGINT_STR_TEST(to_str, "1234567890ABCDEF", bigint);

        BIGINT_SET_INT64(bigint, -0x1234567890ABCDEFLL);

        BIGINT_STR_TEST(to_str, "-1234567890ABCDEF", bigint);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_set_str(void) {
    auto_destroy(bigint_t, bigint);
    bigint = bigint_create();

    if (!bigint) {
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint);
    bigint = bigint_create();

    char_t* str = NULL;

    if(bigint) {
        BIGINT_SET_STR(bigint, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_STR_TEST(to_str, "1234567890ABCDEF1234567890ABCDEF", bigint);

        BIGINT_SET_STR(bigint, "-1");

        BIGINT_STR_TEST(to_str, "-1", bigint);

        BIGINT_SET_STR(bigint, "-1234567890ABCDEF1234567890ABCDEF");

        BIGINT_STR_TEST(to_str, "-1234567890ABCDEF1234567890ABCDEF", bigint);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_shl_shr(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);

    bigint_1 = bigint_create();
    bigint_2 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2) {
        BIGINT_SET_STR(bigint_1, "1234");

        BIGINT_TEST(shl, "12340", bigint_2, bigint_1, 4);

        BIGINT_TEST(shr, "123", bigint_2, bigint_1, 4);

        BIGINT_TEST(shl, "1234", bigint_2, bigint_1, 0);

        BIGINT_TEST(shr, "1234", bigint_2, bigint_1, 0);

        BIGINT_TEST(shl, "12340000000000000000", bigint_2, bigint_1, 64);

        BIGINT_TEST(shr, "0", bigint_2, bigint_1, 64);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(shr, "1234567890ABCDEF", bigint_2, bigint_1, 64);

        BIGINT_SET_STR(bigint_1, "1");

        BIGINT_TEST(shl, "10000000000000000", bigint_2, bigint_1, 64);

        BIGINT_SET_STR(bigint_1, "800000000000");

        BIGINT_TEST(shl, "1000000000000", bigint_2, bigint_1, 1);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(shr_one, "91A2B3C4855E6F7891A2B3C4855E6F7", bigint_1);

        BIGINT_SET_STR(bigint_1, "867890ABCDEF1234567890AB");

        BIGINT_TEST(shl_one, "10CF121579BDE2468ACF12156", bigint_1);

        BIGINT_SET_STR(bigint_1, "-1234");

        BIGINT_TEST(shl_one, "-2468", bigint_1);

        BIGINT_SET_STR(bigint_1, "-1234");

        BIGINT_TEST(shl, "-91A00000000000", bigint_1, bigint_1, 43);

        BIGINT_SET_STR(bigint_1, "-1234");

        BIGINT_TEST(shr_one, "-91A", bigint_1);

        BIGINT_SET_STR(bigint_1, "-91A00000000000");

        BIGINT_TEST(shr, "-1234", bigint_1, bigint_1, 43);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_not(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2) {
        BIGINT_SET_STR(bigint_1, "1234");

        BIGINT_TEST(not, "FFFFFFFFFFFFEDCB", bigint_2, bigint_1);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_and(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "5678");

        BIGINT_TEST(and, "1230", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(and, "1234567890ABCDEF1234567890ABCDEF", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        BIGINT_TEST(and, "1234567890ABCDEF1234567890ABCDEF", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234");
        BIGINT_SET_STR(bigint_2, "-9876");

        BIGINT_TEST(and, "-9A78", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(and, "-1234567890BBFDFF7AB5FFFDFFBBFE00", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(and, "1234567890A9C9A90224543010A9C9AA", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF123456");

        BIGINT_TEST(and, "1030106881A9856F103010", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_or(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "9876");

        BIGINT_TEST(or, "9A76", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "-9876");

        BIGINT_TEST(or, "-8842", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234");
        BIGINT_SET_STR(bigint_2, "9876");

        BIGINT_TEST(or, "-202", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234");
        BIGINT_SET_STR(bigint_2, "-9876");

        BIGINT_TEST(or, "-1032", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(or, "1234567890ABCDEF1234567890ABCDEF", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        BIGINT_TEST(or, "1234567891ABCDEF1234567890ABCDEF", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(or, "-204461010024880020445", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(or, "-1030106881A9856F103011", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF123456");

        BIGINT_TEST(or, "-1234567890A9C9A90224543010A9C9A9", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_xor(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "5678");

        BIGINT_TEST(xor, "444C", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(xor, "0", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        BIGINT_TEST(xor, "10000000000000000000000", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(xor, "1234567890B9F9B96AA5FDB57FB9F9BB", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(xor, "-1234567890B9F9B96AA5FDB57FB9F9BB", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF123456");

        BIGINT_TEST(xor, "-1234567890B9F9B96AA5FDB57FB9F9B9", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_bitwise(void) {
    int32_t ret = 0;

    ret = bigint_test_not();

    if(ret != 0) {
        return ret;
    }

    ret = bigint_test_and();

    if(ret != 0) {
        return ret;
    }

    ret = bigint_test_or();

    if(ret != 0) {
        return ret;
    }

    ret = bigint_test_xor();

    if(ret != 0) {
        return ret;
    }

    return 0;
}

static int32_t bigint_test_cmp(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();

    if(bigint_1 && bigint_2) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "5678");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "1234");

        if(bigint_cmp(bigint_1, bigint_2) != 0) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "5678");
        BIGINT_SET_STR(bigint_2, "1234");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "-5678");
        BIGINT_SET_STR(bigint_2, "-1234");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "-1234");
        BIGINT_SET_STR(bigint_2, "-5678");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "-1234");
        BIGINT_SET_STR(bigint_2, "5678");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "-5678");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "5678");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-5678");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            print_error("bigint_cmp failed");
            return -1;
        }

        print_success("bigint_cmp passed");
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_add_sub(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "5678");

        BIGINT_TEST(add, "68AC", bigint_3, bigint_1, bigint_2);

        BIGINT_TEST(sub, "-4444", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(add, "2468ACF121579BDE2468ACF121579BDE", bigint_3, bigint_1, bigint_2);

        BIGINT_TEST(sub, "0", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        BIGINT_TEST(sub, "-10000000000000000000000", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF1234567890ABCDEF");

        BIGINT_TEST(add, "-10000000000000000000000", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "-1234567891ABCDEF123456");

        BIGINT_TEST(add, "-1234567890BE02458AC602467FBE0245", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "91A000000000");
        BIGINT_SET_STR(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(sub, "-1234567890ABCDEF1233C4D890ABCDEF", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234");
        BIGINT_SET_STR(bigint_2, "5678");

        BIGINT_TEST(add, "4444", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_mul(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "5678");

        BIGINT_TEST(mul, "6260060", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "3");
        BIGINT_SET_STR(bigint_2, "-5");

        BIGINT_TEST(mul, "-F", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234");

        BIGINT_TEST(mul, "14B60B60AA97760A3D760B60AA97760A28C", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "FEDCBA9876543210");

        BIGINT_TEST(mul, "121FA00ACD77D742358D290922D96432236D88FE55618CF0", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "-FEDCBA9876543210");

        BIGINT_TEST(mul, "-121FA00ACD77D742358D290922D96432236D88FE55618CF0", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "480795EF8E54BE25D4B2");

        BIGINT_TEST(mul, "144444904AFA2F71646AF4F5A6613A1F642B4BC4", bigint_1, bigint_1, bigint_1);

        BIGINT_SET_STR(bigint_1, "480795EF8E54BE25D4B275AC532E410C48D115DC755780CE2C0327E4581BF3");

        BIGINT_TEST(mul, "144444904AFA2F71646B372D8E6E9EAE180F1E88D62D1CA9436B8B99710477CB646E30BC3F0ABFD28821F694C79003E8A62EB8775DD61EB54626121D28A9", bigint_1, bigint_1, bigint_1);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_pow(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "3");
        BIGINT_SET_STR(bigint_2, "2");

        BIGINT_TEST(pow, "9", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "12");
        BIGINT_SET_STR(bigint_2, "34");

        BIGINT_TEST(pow, "1C9040830AA8880352DFDF4C48E4FBA82690BE45210000000000000", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_div(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "56");

        BIGINT_TEST(div, "36", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890AB");
        BIGINT_SET_STR(bigint_2, "56");

        BIGINT_TEST(div, "3630A22567", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234");

        BIGINT_TEST(div, "10004C01602D88D768FF32BC2824B", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "FEDCBA9876543210");

        BIGINT_TEST(div, "124924923F07FFFE", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "-FEDCBA9876543210");

        BIGINT_TEST(div, "-124924923F07FFFE", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(div, "124924923F07FFFE", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_mod(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "56");

        BIGINT_TEST(mod, "10", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890AB");
        BIGINT_SET_STR(bigint_2, "56");

        BIGINT_TEST(mod, "11", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234");

        BIGINT_TEST(mod, "10B3", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "FEDCBA9876543210");

        BIGINT_TEST(mod, "FC6C9395FCD4320F", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "-FEDCBA9876543210");

        BIGINT_TEST(mod, "FC6C9395FCD4320F", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(mod, "-FC6C9395FCD4320F", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "FEDCBA9876543210");

        BIGINT_TEST(mod, "-FC6C9395FCD4320F", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    printf("bigint_mod tests completed\n");

    return 0;
}

static int32_t bigint_test_gcd(void) {
    printf("Starting bigint_gcd tests\n");
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();

    char_t* str = NULL;

    if(bigint_1 && bigint_2 && bigint_3) {
        BIGINT_SET_STR(bigint_1, "1234");
        BIGINT_SET_STR(bigint_2, "56");

        BIGINT_TEST(gcd, "2", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890AB");
        BIGINT_SET_STR(bigint_2, "56");

        BIGINT_TEST(gcd, "1", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        BIGINT_SET_STR(bigint_2, "1234");

        BIGINT_TEST(gcd, "5", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "FEDCBA9876543210");

        BIGINT_TEST(gcd, "F", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_2, "-FEDCBA9876543210");

        BIGINT_TEST(gcd, "F", bigint_3, bigint_1, bigint_2);

        BIGINT_SET_STR(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");

        BIGINT_TEST(gcd, "F", bigint_3, bigint_1, bigint_2);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_isqrt(void) {
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();

    char_t* str = NULL;

    if(!bigint_1 || !bigint_2) {
        print_error("bigint_create failed");
        return -1;
    }

    BIGINT_SET_STR(bigint_1, "144");

    BIGINT_TEST(isqrt, "12", bigint_2, bigint_1);
    return 0;

}

static int32_t bigint_test_mul_mod(void){
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    auto_destroy(bigint_t, bigint_4);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();
    bigint_4 = bigint_create();

    char_t* str = NULL;

    if(!bigint_1 || !bigint_2 || !bigint_3 || !bigint_4) {
        print_error("bigint_create failed");
        return -1;
    }

    BIGINT_SET_STR(bigint_1, "41929F51");
    BIGINT_SET_STR(bigint_2, "4DAA51D9");
    BIGINT_SET_STR(bigint_3, "9B54A3B3");

    BIGINT_TEST(mul_mod, "2CE10231", bigint_4, bigint_1, bigint_2, bigint_3);

    BIGINT_SET_STR(bigint_1, "480795EF8E54BE25D4B275AC532E410C48D115DC755780CE2C0327E4581BF3");
    BIGINT_SET_STR(bigint_2, "480795EF8E54BE25D4B275AC532E410C48D115DC755780CE2C0327E4581BF3");
    BIGINT_SET_STR(bigint_3, "9B54A3A2FDB8D8A969870D87E3C05C21DA5F74CD413596E2213FBC7BFA800E3B");

    BIGINT_TEST(mul_mod, "8FD619722A77625BA23AD86CEF48FAB83EC2E83B905B4A475E5BD88383296653", bigint_4, bigint_1, bigint_2, bigint_3);

    // Case 1: Squaring p-1 (Result must be 1)
    // (p-1)*(p-1) mod p == (-1)*(-1) mod p == 1
    BIGINT_SET_STR(bigint_1, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEC");
    BIGINT_SET_STR(bigint_3, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");
    BIGINT_TEST(mul_mod, "1", bigint_4, bigint_1, bigint_1, bigint_3);

    // Case 2: Squaring the 254th bit
    // (2^254 * 2^254) mod p == 2^508 mod p
    // 2^508 is (2^255 * 2^253). Since 2^255 = 19 mod p,
    // this is 19 * 2^253 mod p.
    BIGINT_SET_STR(bigint_1, "4000000000000000000000000000000000000000000000000000000000000000");
    // Expected: 19 * 2^253 = 0x4C00000000000000000000000000000000000000000000000000000000000000
    BIGINT_TEST(mul_mod, "600000000000000000000000000000000000000000000000000000000000004C", bigint_4, bigint_1, bigint_1, bigint_3);

    BIGINT_SET_STR(bigint_1, "1234");
    BIGINT_SET_STR(bigint_3, "5678");
    BIGINT_TEST(mul_mod, "B8", bigint_1, bigint_1, bigint_1, bigint_3);

    return 0;
}

static int32_t bigint_test_pow_mod(void){
    auto_destroy(bigint_t, bigint_1);
    auto_destroy(bigint_t, bigint_2);
    auto_destroy(bigint_t, bigint_3);
    auto_destroy(bigint_t, bigint_4);
    bigint_1 = bigint_create();
    bigint_2 = bigint_create();
    bigint_3 = bigint_create();
    bigint_4 = bigint_create();

    char_t* str = NULL;

    if(!bigint_1 || !bigint_2 || !bigint_3 || !bigint_4) {
        print_error("bigint_create failed");
        return -1;
    }

    BIGINT_SET_STR(bigint_1, "41929F51");
    BIGINT_SET_STR(bigint_2, "4DAA51D9");
    BIGINT_SET_STR(bigint_3, "9B54A3B3");

    BIGINT_TEST(pow_mod, "1", bigint_4, bigint_1, bigint_2, bigint_3);

    BIGINT_SET_STR(bigint_1, "4FA20A442C5B6AC2CDC7B9A0F1E996F6F819B68C8289F8132BC722D04B85DCB0");
    BIGINT_SET_STR(bigint_2, "4DAA51D17EDC6C54B4C386C3F1E02E10ED2FBA66A09ACB71109FDE3DFD40071D");
    BIGINT_SET_STR(bigint_3, "9B54A3A2FDB8D8A969870D87E3C05C21DA5F74CD413596E2213FBC7BFA800E3B");

    BIGINT_TEST(pow_mod, "1", bigint_4, bigint_1, bigint_2, bigint_3);

    return 0;
}

static int32_t bigint_test_prime(void) {
    auto_destroy(bigint_t, bigint_1);
    bigint_1 = bigint_create();

    if(bigint_1) {
        BIGINT_SET_STR(bigint_1, "1234");

        if(bigint_is_prime(bigint_1)) {
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");

        BIGINT_SET_STR(bigint_1, "3D");

        if(!bigint_is_prime(bigint_1)) {
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");

        BIGINT_SET_STR(bigint_1, "8D6BEBFBC797F75551AAFC5679E488C9A42576BBB377C1F3");

        if(!bigint_is_prime(bigint_1)) {
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");

        BIGINT_SET_STR(bigint_1, "9B54A3A2FDB8D8A969870D87E3C05C21DA5F74CD413596E2213FBC7BFA800E3B");

        if(!bigint_is_prime(bigint_1)) {
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");


        int32_t test_count = 6;
        int32_t bit_count  = 32;

        while(test_count--) {
            bigint_destroy(bigint_1);

            time_t start_time = time_ns(NULL);
            bigint_1 = bigint_random_prime(bit_count);
            time_t end_time = time_ns(NULL);
            uint64_t passed_time = end_time - start_time;
            printf("bigint_random_prime (%i-bits) time: %llu ns %llu ms\n", bit_count, passed_time, passed_time / 1000000);

            if(bigint_1) {
                char_t* str = bigint_to_str(bigint_1);
                printf("bigint_random_prime: %s (%i(%lli)-bits)\n", str, bit_count, bigint_bit_length(bigint_1) + 1);
                memory_free((void*)str);
                print_success("bigint_random_prime passed");
            } else {
                print_error("bigint_random_prime failed");
                return -1;
            }

            bit_count <<= 1;
        }


        print_success("prime tests passed");
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_from_to_bytes(void) {
    auto_destroy(bigint_t, bigint_1);
    bigint_1 = bigint_create();
    uint8_t buffer[16];
    size_t buffer_len = sizeof(buffer);
    uint8_t buffer2[20];
    size_t buffer2_len = sizeof(buffer2);

    char_t* str = NULL;

    if(!bigint_1) {
        print_error("bigint_create failed");
        return -1;
    }

    BIGINT_SET_STR(bigint_1, "1234567890ABCDEF1234567890ABCDEF");

    printf("bit count: %llu\n", bigint_bit_length(bigint_1) + 1);

    if(bigint_to_bytes(bigint_1, buffer, buffer_len) == -1) {
        print_error("bigint_to_bytes failed");
        return -1;
    }

    printf("bigint_to_bytes: ");
    for(size_t i = 0; i < buffer_len; i++) {
        printf("%02X", buffer[i]);
    }
    printf("\n");

    if(bigint_to_bytes(bigint_1, buffer2, buffer2_len) == -1) {
        print_error("bigint_to_bytes failed");
        return -1;
    }

    printf("bigint_to_bytes (with larger buffer): ");
    for(size_t i = 0; i < buffer2_len; i++) {
        printf("%02X", buffer2[i]);
    }
    printf("\n");

    // check first 4 bytes are 0
    for(size_t i = 0; i < 4; i++) {
        if(buffer2[i] != 0) {
            print_error("bigint_to_bytes with larger buffer failed");
            return -1;
        }
    }

    // check remaining bytes match
    for(size_t i = 0; i < buffer_len; i++) {
        if(buffer2[i + 4] != buffer[i]) {
            print_error("bigint_to_bytes with larger buffer failed");
            return -1;
        }
    }

    bigint_destroy(bigint_1);
    bigint_1 = bigint_create();

    if(!bigint_1) {
        print_error("bigint_create failed");
        return -1;
    }

    BIGINT_TEST(from_bytes, "1234567890ABCDEF1234567890ABCDEF", bigint_1, buffer, buffer_len);

    if(bigint_to_bytes_le(bigint_1, buffer, buffer_len) == -1) {
        print_error("bigint_to_bytes_le failed");
        return -1;
    }

    BIGINT_TEST(from_bytes_le, "1234567890ABCDEF1234567890ABCDEF", bigint_1, buffer, buffer_len);

    return 0;
}

static int32_t bigint_test_sub_add_mod(void) {
    auto_destroy(bigint_t, a);
    auto_destroy(bigint_t, b);
    auto_destroy(bigint_t, m);
    auto_destroy(bigint_t, res);
    a = bigint_create();
    b = bigint_create();
    m = bigint_create();
    res = bigint_create();
    char_t* str;

    // Modulus m = 13 (0xD)
    BIGINT_SET_INT64(m, 13);

    // --- Test 1: add_mod (Wrap-around) ---
    // (8 + 7) mod 13 = 15 mod 13 = 2
    BIGINT_SET_INT64(a, 8);
    BIGINT_SET_INT64(b, 7);
    if(bigint_add_mod(res, a, b, m) == -1) {
        print_error("Test 1 Failed: add_mod wrap-around");
        return -1;
    }

    if (!bigint_is_int64(res, 2)) {
        str = bigint_to_str(res);
        printf("Test 1 Failed: Expected 2, got %s\n", str);
        return -1;
    }
    print_success("add_mod wrap-around passed");

    // --- Test 2: sub_mod (Underflow) ---
    // (3 - 10) mod 13 = -7 mod 13 = 6
    BIGINT_SET_INT64(a, 3);
    BIGINT_SET_INT64(b, 10);
    if(bigint_sub_mod(res, a, b, m) == -1) {
        print_error("Test 2 Failed: sub_mod underflow");
        return -1;
    }

    if (!bigint_is_int64(res, 6)) {
        str = bigint_to_str(res);
        printf("Test 2 Failed: Expected 6, got %s\n", str);
        return -1;
    }
    print_success("sub_mod underflow passed");

    // --- Test 3: Large Numbers (ECC Style) ---
    // a = 0xFF...FF, b = 1, m = 0xFF...FF (Same as a)
    // (m + 1) mod m = 1
    BIGINT_SET_STR(a, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    BIGINT_SET_STR(m, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    BIGINT_SET_INT64(b, 1);
    if(bigint_add_mod(res, a, b, m) == -1) {
        print_error("Test 3 Failed: Large number addition");
        return -1;
    }

    if (!bigint_is_int64(res, 1)) {
        print_error("Test 3 Failed: Large number reduction");
        return -1;
    }
    print_success("add_mod large numbers passed");

    BIGINT_SET_STR(a, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEC");
    BIGINT_SET_STR(b, "1");
    BIGINT_SET_STR(m, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");
    BIGINT_TEST(add_mod, "0", res, a, b, m);

    BIGINT_TEST(add_mod, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEB", res, a, a, m);

    BIGINT_SET_STR(a, "4000000000000000000000000000000000000000000000000000000000000000");
    BIGINT_SET_STR(b, "4000000000000000000000000000000000000000000000000000000000000000");
    BIGINT_TEST(add_mod, "13", res, a, b, m);

    BIGINT_SET_STR(a, "7000000000000000000000000000000000000000000000000000000000000000");
    BIGINT_SET_STR(b, "1000000000000000000000000000000000000000000000000000000000000000");
    BIGINT_TEST(add_mod, "13", res, a, b, m);

    // Case 1: Standard small borrow
    BIGINT_SET_STR(a, "1");
    BIGINT_SET_STR(b, "2");
    BIGINT_SET_STR(m, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");
    BIGINT_TEST(sub_mod, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEC", res, a, b, m);

    // Case 2: Zero result
    BIGINT_TEST(sub_mod, "0", res, a, a, m);

    // Case 3: P - (P-1)
    BIGINT_SET_STR(b, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEC");
    BIGINT_TEST(sub_mod, "1", res, m, b, m);

    return 0;
}

static int32_t bigint_test_mod_inv(void) {
    auto_destroy(bigint_t, a);
    auto_destroy(bigint_t, n);
    auto_destroy(bigint_t, inv);
    auto_destroy(bigint_t, check);
    a = bigint_create();
    n = bigint_create();
    inv = bigint_create();
    check = bigint_create();

    char_t* str = NULL;

    // Test 1: Simple prime modulus
    // a = 3, n = 11. Inverse should be 4 (because 3*4 = 12, 12 % 11 = 1)
    BIGINT_SET_INT64(a, 3);
    BIGINT_SET_INT64(n, 11);
    BIGINT_TEST(mod_inv, "4", inv, a, n);

    // Test 2: Larger number
    // a = 127, n = 101. Inverse should be 12 (127*12 = 1524, 1524 % 101 = 9, wait...)
    // Let's use a known pair: 17 mod 3120 (from RSA e/phi). inv = 2753
    BIGINT_SET_INT64(a, 17);
    BIGINT_SET_INT64(n, 3120);
    BIGINT_TEST(mod_inv, "AC1", inv, a, n);

    // Verify Property: (a * inv) % n == 1
    if(bigint_mul_mod(check, a, inv, n) == -1) {
        print_error("mod_inv property verification failed (multiplication)");
        return -1;
    }
    if (!bigint_is_int64(check, 1)) {
        print_error("mod_inv property verification failed");
        return -1;
    }

    // Test 3: Not invertible (gcd != 1)
    // a = 6, n = 9. Should return -1
    BIGINT_SET_INT64(a, 6);
    BIGINT_SET_INT64(n, 9);
    if (bigint_mod_inv(inv, a, n) != -1) {
        print_error("mod_inv Test 3 failed (Should have failed for non-coprime)");
        return -1;
    }

    // Case 1: Inverse of 2
    // Inverse of 2 mod p is (p+1)/2
    BIGINT_SET_STR(a, "2");
    BIGINT_SET_STR(n, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");
    // Expected: 0x3FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF77
    BIGINT_TEST(mod_inv, "3FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7", inv, a, n);

    print_success("bigint_mod_inv tests passed");

    return 0;
}

static int32_t bigint_test_mod_sqrt(void) {
    auto_destroy(bigint_t, a);
    auto_destroy(bigint_t, p);
    auto_destroy(bigint_t, sqrt);
    auto_destroy(bigint_t, check);
    a = bigint_create();
    p = bigint_create();
    sqrt  = bigint_create();
    check = bigint_create();

    char_t* str = NULL;

    // Test 1: Simple case
    // a = 10, p = 13. sqrt should be 6 (because 6*6=36, 36 % 13 = 10) or 7 (because 7*7=49, 49 % 13 = 10)
    BIGINT_SET_INT64(a, 10);
    BIGINT_SET_INT64(p, 13);
    if (bigint_mod_sqrt(sqrt, a, p) == -1) {
        print_error("mod_sqrt Test 1 failed");
        return -1;
    }

    str = bigint_to_str(sqrt);
    printf("mod_sqrt Test 1 result: %s\n", str);
    memory_free((void*)str);

    // Verify Property: (sqrt * sqrt) % p == a
    if(bigint_mul_mod(check, sqrt, sqrt, p) == -1) {
        print_error("mod_sqrt property verification failed (multiplication)");
        return -1;
    }
    if (bigint_cmp(check, a) != 0) {
        print_error("mod_sqrt property verification failed");
        return -1;
    }

    // Test 2
    BIGINT_SET_INT64(a, 56);
    BIGINT_SET_INT64(p, 101);
    if (bigint_mod_sqrt(sqrt, a, p) == -1) {
        print_error("mod_sqrt Test 2 failed");
        return -1;
    }

    str = bigint_to_str(sqrt);
    printf("mod_sqrt Test 2 result: %s\n", str);
    memory_free((void*)str);

    // Verify Property: (sqrt * sqrt) % p == a
    if(bigint_mul_mod(check, sqrt, sqrt, p) == -1) {
        print_error("mod_sqrt property verification failed (multiplication)");
        return -1;
    }
    if (bigint_cmp(check, a) != 0) {
        print_error("mod_sqrt property verification failed");
        return -1;
    }

    // Test 3
    BIGINT_SET_INT64(a, 10);
    BIGINT_SET_INT64(p, 13);
    if (bigint_mod_sqrt(sqrt, a, p) == -1) {
        print_error("mod_sqrt Test 3 failed");
        return -1;
    }

    str = bigint_to_str(sqrt);
    printf("mod_sqrt Test 3 result: %s\n", str);
    memory_free((void*)str);

    // Verify Property: (sqrt * sqrt) % p == a
    if(bigint_mul_mod(check, sqrt, sqrt, p) == -1) {
        print_error("mod_sqrt property verification failed (multiplication)");
        return -1;
    }
    if (bigint_cmp(check, a) != 0) {
        print_error("mod_sqrt property verification failed");
        return -1;
    }

    // Test 4
    BIGINT_SET_STR(a, "4E30F56AA3991ABB06678FA27E58DBC43E9E29DA59189CCD76D4C41233D85DD2");
    BIGINT_SET_STR(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");
    if (bigint_mod_sqrt(sqrt, a, p) == -1) {
        print_error("mod_sqrt Test 4 failed");
        return -1;
    }

    str = bigint_to_str(sqrt);
    printf("mod_sqrt Test 4 result: %s\n", str);
    memory_free((void*)str);

    // Verify Property: (sqrt * sqrt) % p == a
    if(bigint_mul_mod(check, sqrt, sqrt, p) == -1) {
        print_error("mod_sqrt property verification failed (multiplication)");
        return -1;
    }
    if (bigint_cmp(check, a) != 0) {
        print_error("mod_sqrt property verification failed");
        return -1;
    }

    // Test 5
    BIGINT_SET_INT64(a, 2);
    BIGINT_SET_INT64(p, 17);
    if (bigint_mod_sqrt(sqrt, a, p) == -1) {
        print_error("mod_sqrt Test 5 failed");
        return -1;
    }

    str = bigint_to_str(sqrt);
    printf("mod_sqrt Test 5 result: %s\n", str);
    memory_free((void*)str);

    // Verify Property: (sqrt * sqrt) % p == a
    if(bigint_mul_mod(check, sqrt, sqrt, p) == -1) {
        print_error("mod_sqrt property verification failed (multiplication)");
        return -1;
    }
    if (bigint_cmp(check, a) != 0) {
        print_error("mod_sqrt property verification failed");
        return -1;
    }

    print_success("bigint_mod_sqrt tests passed");

    return 0;
}

static int32_t bigint_test_uint64_ops(void) {
    auto_destroy(bigint_t, a);
    auto_destroy(bigint_t, expected);
    a = bigint_create();
    expected = bigint_create();

    if (!a || !expected) {
        print_error("bigint_create failed");
        return -1;
    }

    BIGINT_SET_UINT64(a, 0xFFFFFFFFFFFFFFFFULL);
    if(bigint_add_uint64(a, 1) == -1) {
        print_error("bigint_add_uint64 failed");
        return -1;
    }

    BIGINT_SET_STR(expected, "10000000000000000");

    if (bigint_cmp(a, expected) != 0) {
        print_error("bigint_add_uint64 failed");
        return -1;
    }

    BIGINT_SET_UINT64(a, 0x100000000ULL);
    if(bigint_sub_uint64(a, 1) == -1) {
        print_error("bigint_sub_uint64 failed");
        return -1;
    }

    if(!bigint_is_uint64(a, 0xFFFFFFFFULL)) {
        print_error("bigint_sub_uint64 failed");
        return -1;
    }

    BIGINT_SET_UINT64(a, 5);
    if(bigint_sub_uint64(a, 10) == -1) {
        print_error("bigint_sub_uint64 failed");
        return -1;
    }

    if(!bigint_is_negative(a)) {
        print_error("bigint_sub_uint64 negative check failed");
        return -1;
    }

    if(!bigint_is_int64(a, -5)) {
        print_error("bigint_sub_uint64 negative result failed");
        return -1;
    }

    BIGINT_SET_UINT64(a, 0xFFFFFFFFFFFFFFFFULL);
    if(bigint_mul_uint64(a, 2) == -1) {
        print_error("bigint_mul_uint64 failed");
        return -1;
    }
    BIGINT_SET_STR(expected, "1FFFFFFFFFFFFFFFE");

    if (bigint_cmp(a, expected) != 0) {
        print_error("bigint_mul_uint64 failed");
        return -1;
    }

    BIGINT_SET_STR(a, "1234567890ABCDEF1234567890ABCDEF");
    uint64_t rem = bigint_mod_uint64(a, 97);

    if(rem != 0x53) {
        print_error("bigint_mod_uint64 failed");
        return -1;
    }

    if(bigint_sub_uint64(a, rem) == -1) {
        print_error("bigint_sub_uint64 failed");
        return -1;
    }

    uint64_t check_rem = bigint_mod_uint64(a, 97);

    if (check_rem != 0) {
        print_error("bigint_mod_uint64 failed");
        return -1;
    }

    if(bigint_set_zero(a) == -1) {
        print_error("bigint_set_zero failed");
        return -1;
    }
    if(bigint_mul_uint64(a, 12345) == -1) {
        print_error("bigint_mul_uint64 failed");
        return -1;
    }

    if (!bigint_is_uint64(a, 0)) {
        print_error("bigint_mul_uint64 zero check failed");
        return -1;
    }

    print_success("bigint uint64 ops tests passed");
    return 0;
}

int32_t main(void) {
    int32_t result = 0;

    result = bigint_test_set_int();

    if(result != 0) {
        return result;
    }

    result = bigint_test_set_str();

    if(result != 0) {
        return result;
    }

    result = bigint_test_from_to_bytes();

    if(result != 0) {
        return result;
    }

    result = bigint_test_shl_shr();

    if(result != 0) {
        return result;
    }

    result = bigint_test_bitwise();

    if(result != 0) {
        return result;
    }

    result = bigint_test_cmp();

    if(result != 0) {
        return result;
    }

    result = bigint_test_add_sub();

    if(result != 0) {
        return result;
    }

    result = bigint_test_mul();

    if(result != 0) {
        return result;
    }

    result = bigint_test_pow();

    if(result != 0) {
        return result;
    }

    result = bigint_test_div();

    if(result != 0) {
        return result;
    }

    result = bigint_test_mod();

    if(result != 0) {
        return result;
    }

    result = bigint_test_gcd();

    if(result != 0) {
        return result;
    }

    result = bigint_test_isqrt();

    if(result != 0) {
        return result;
    }

    result = bigint_test_mul_mod();

    if(result != 0) {
        return result;
    }

    result = bigint_test_pow_mod();

    if(result != 0) {
        return result;
    }

    result = bigint_test_mod_inv();

    if(result != 0) {
        return result;
    }

    result = bigint_test_mod_sqrt();

    if(result != 0) {
        return result;
    }

    result = bigint_test_uint64_ops();

    if(result != 0) {
        return result;
    }

    result = bigint_test_sub_add_mod();

    if(result != 0) {
        return result;
    }

    result = bigint_test_prime();

    if (result == 0) {
        print_success("bigint test passed");
    } else {
        print_error("bigint test failed");
    }

    return result;
}
