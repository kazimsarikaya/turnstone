/**
 * @file test_bigint.c
 * @brief Big integer arithmetic
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x800000
#include "setup.h"
#include <bigint.h>
#include <strings.h>

int32_t main(void);

static int32_t bigint_test_set_int(void) {
    bigint_t* bigint = bigint_create();

    if (bigint) {
        bigint_destroy(bigint);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    bigint = bigint_create();

    if(bigint) {
        bigint_set_int64(bigint, 0x1234567890ABCDEFLL);

        const char_t* str = bigint_to_str(bigint);

        if (str) {
            printf("bigint_to_string: %s\n", str);

            if(strncmp(str, "1234567890ABCDEF", 16) != 0) {
                print_error("bigint_to_string failed");
            } else {
                print_success("bigint_to_string passed");
            }


            memory_free((void*)str);
        } else {
            bigint_destroy(bigint);
            print_error("bigint_to_string failed");
            return -1;
        }

        bigint_set_int64(bigint, -0x1234567890ABCDEFLL);

        str = bigint_to_str(bigint);

        if (str) {
            printf("negative bigint_to_string: %s\n", str);

            if(strncmp(str, "-1234567890ABCDEF", 17) != 0) {
                print_error("negative bigint_to_string failed");
            } else {
                print_success("negative bigint_to_string passed");
            }


            memory_free((void*)str);
            bigint_destroy(bigint);
        } else {
            bigint_destroy(bigint);
            print_error("negative bigint_to_string failed");
            return -1;
        }
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_set_str(void) {
    bigint_t* bigint = bigint_create();

    if (bigint) {
        bigint_destroy(bigint);
    } else {
        print_error("bigint_create failed");
        return -1;
    }

    bigint = bigint_create();

    if(bigint) {
        bigint_set_str(bigint, "1234567890ABCDEF1234567890ABCDEF");

        const char_t* str = bigint_to_str(bigint);

        if (str) {
            printf("bigint_to_string: %s\n", str);

            if(strncmp(str, "1234567890ABCDEF1234567890ABCDEF", 32) != 0) {
                print_error("bigint_to_string failed");
            } else {
                print_success("bigint_to_string passed");
            }

            memory_free((void*)str);

        } else {
            bigint_destroy(bigint);
            print_error("bigint_to_string failed");
            return -1;
        }

        bigint_set_str(bigint, "-1234567890ABCDEF1234567890ABCDEF");

        str = bigint_to_str(bigint);

        if (str) {
            printf("negative bigint_to_string: %s\n", str);

            if(strncmp(str, "-1234567890ABCDEF1234567890ABCDEF", 33) != 0) {
                print_error("negative bigint_to_string failed");
            } else {
                print_success("negative bigint_to_string passed");
            }

            memory_free((void*)str);
            bigint_destroy(bigint);
        } else {
            bigint_destroy(bigint);
            print_error("negative bigint_to_string failed");
            return -1;
        }

    } else {
        print_error("bigint_create failed");
        return -1;
    }

    return 0;
}

static int32_t bigint_test_shl_shr(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();

    if(bigint_1 && bigint_2) {
        bigint_set_str(bigint_1, "1234");

        if(bigint_shl(bigint_2, bigint_1, 4) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shl: %s\n", str);

            if(strncmp(str, "12340", 5) != 0) {
                print_error("bigint_shl failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        if(bigint_shr(bigint_2, bigint_1, 4) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shr: %s\n", str);

            if(strncmp(str, "123", 3) != 0) {
                print_error("bigint_shr failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        if(bigint_shl(bigint_2, bigint_1, 0) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shl: %s\n", str);

            if(strncmp(str, "1234", 4) != 0) {
                print_error("bigint_shl failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        if(bigint_shr(bigint_2, bigint_1, 0) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shr: %s\n", str);

            if(strncmp(str, "1234", 4) != 0) {
                print_error("bigint_shr failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        if(bigint_shl(bigint_2, bigint_1, 64) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shl: %s\n", str);

            if(strncmp(str, "12340000000000000000", 20) != 0) {
                print_error("bigint_shl failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        if(bigint_shr(bigint_2, bigint_1, 64) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shr: %s\n", str);

            if(strncmp(str, "0", 1) != 0) {
                print_error("bigint_shr failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_shr(bigint_2, bigint_1, 64) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shr: %s\n", str);

            if(strncmp(str, "1234567890ABCDEF", 16) != 0 || strlen(str) != 16) {
                print_error("bigint_shr failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1");

        if(bigint_shl(bigint_2, bigint_1, 64) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shl: %s\n", str);

            if(strncmp(str, "10000000000000000", 17) != 0 || strlen(str) != 17) {
                print_error("bigint_shl failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        bigint_set_str(bigint_1, "800000000000");

        if(bigint_shl(bigint_2, bigint_1, 1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_shl: %s\n", str);

            if(strncmp(str, "1000000000000", 13) != 0 || strlen(str) != 13) {
                print_error("bigint_shl failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_shr_one(bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr_one failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_shr_one: %s\n", str);

            if(strncmp(str, "91A2B3C4855E6F7891A2B3C4855E6F7", 31) != 0 || strlen(str) != 31) {
                print_error("bigint_shr_one failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr_one passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr_one failed");
            return -1;
        }

        bigint_set_str(bigint_1, "867890ABCDEF1234567890AB");

        if(bigint_shl_one(bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl_one failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_shl_one: %s\n", str);

            if(strncmp(str, "10CF121579BDE2468ACF12156", 25) != 0 || strlen(str) != 25) {
                print_error("bigint_shl_one failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl_one passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl_one failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");

        if(bigint_shl_one(bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl_one failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_shl_one: %s\n", str);

            if(strncmp(str, "-2468", 5) != 0 || strlen(str) != 5) {
                print_error("bigint_shl_one failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl_one passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl_one failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");

        if(bigint_shl(bigint_1, bigint_1, 43) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_shl: %s\n", str);

            if(strncmp(str, "-91A00000000000", 15) != 0 || strlen(str) != 15) {
                print_error("bigint_shl failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shl passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shl failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");

        if(bigint_shr_one(bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr_one failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_shr_one: %s\n", str);

            if(strncmp(str, "-91A", 4) != 0 || strlen(str) != 4) {
                print_error("bigint_shr_one failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr_one passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr_one failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-91A00000000000");

        if(bigint_shr(bigint_1, bigint_1, 43) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_shr: %s\n", str);

            if(strncmp(str, "-1234", 5) != 0 || strlen(str) != 5) {
                print_error("bigint_shr failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_shr passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_shr failed");
            return -1;
        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);

    return 0;
}

static int32_t bigint_test_not(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();

    if(bigint_1 && bigint_2) {
        bigint_set_str(bigint_1, "1234");

        if(bigint_not(bigint_2, bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_not failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_2);

        if (str) {
            printf("bigint_not: %s\n", str);

            if(strncmp(str, "FFFFFFFFEDCB", 12) != 0) {
                print_error("bigint_not failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                return -1;
            } else {
                print_success("bigint_not passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_not failed");
            return -1;
        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);

    return 0;
}

static int32_t bigint_test_and(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "1230", 4) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "1234567890ABCDEF1234567890ABCDEF", 32) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;

        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "1234567890ABCDEF1234567890ABCDEF", 32) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");
        bigint_set_str(bigint_2, "-9876");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "-9A78", 5) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "-1234567890BBFDFF7AB5FFFDFFBBFE00", 33) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "1234567890A9C9A90224543010A9C9AA", 32) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF123456");

        if(bigint_and(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_and: %s\n", str);

            if(strncmp(str, "1030106881A9856F103010", 22) != 0) {
                print_error("bigint_and failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_and passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_and failed");
            return -1;
        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_or(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "9876");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "9A76", 4) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "-9876");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "-8842", 5) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");
        bigint_set_str(bigint_2, "9876");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "-202", 4) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");
        bigint_set_str(bigint_2, "-9876");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "-1032", 5) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "1234567890ABCDEF1234567890ABCDEF", 32) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;

        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "1234567891ABCDEF1234567890ABCDEF", 32) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "-204461010024880020445", 22) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "-1030106881A9856F103011", 23) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF123456");

        if(bigint_or(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_or: %s\n", str);

            if(strncmp(str, "-1234567890A9C9A90224543010A9C9A9", 33) != 0) {
                print_error("bigint_or failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_or passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_or failed");
            return -1;
        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_xor(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_xor(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_xor: %s\n", str);

            if(strncmp(str, "444C", 4) != 0) {
                print_error("bigint_xor failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_xor passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_xor(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_xor: %s\n", str);

            if(strncmp(str, "0", 1) != 0) {
                print_error("bigint_xor failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_xor passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;

        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        if(bigint_xor(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_xor: %s\n", str);

            if(strncmp(str, "10000000000000000000000", 23) != 0) {
                print_error("bigint_xor failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_xor passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_xor(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_xor: %s\n", str);

            if(strncmp(str, "1234567890B9F9B96AA5FDB57FB9F9BB", 32) != 0) {
                print_error("bigint_xor failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_xor passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_xor(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_xor: %s\n", str);

            if(strncmp(str, "-1234567890B9F9B96AA5FDB57FB9F9BB", 33) != 0) {
                print_error("bigint_xor failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_xor passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF123456");

        if(bigint_xor(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_xor: %s\n", str);

            if(strncmp(str, "-1234567890B9F9B96AA5FDB57FB9F9B9", 33) != 0) {
                print_error("bigint_xor failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_xor passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_xor failed");
            return -1;
        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

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
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();

    if(bigint_1 && bigint_2) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "1234");

        if(bigint_cmp(bigint_1, bigint_2) != 0) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "5678");
        bigint_set_str(bigint_2, "1234");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-5678");
        bigint_set_str(bigint_2, "-1234");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");
        bigint_set_str(bigint_2, "-5678");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "-5678");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "5678");

        if(bigint_cmp(bigint_1, bigint_2) != 1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-5678");

        if(bigint_cmp(bigint_1, bigint_2) != -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            print_error("bigint_cmp failed");
            return -1;
        }

        print_success("bigint_cmp passed");
    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);

    return 0;
}

static int32_t bigint_test_add_sub(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_add(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_add: %s\n", str);

            if(strncmp(str, "68AC", 4) != 0) {
                print_error("bigint_add failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_add passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        if(bigint_sub(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_sub: %s\n", str);

            if(strncmp(str, "-4444", 5) != 0) {
                print_error("bigint_sub failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_sub passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_add(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_add: %s\n", str);

            if(strncmp(str, "2468ACF121579BDE2468ACF121579BDE", 32) != 0) {
                print_error("bigint_add failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_add passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        if(bigint_sub(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_sub: %s\n", str);

            if(strncmp(str, "0", 1) != 0) {
                print_error("bigint_sub failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_sub passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234567891ABCDEF1234567890ABCDEF");

        if(bigint_sub(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_sub: %s\n", str);

            if(strncmp(str, "-10000000000000000000000", 24) != 0) {
                print_error("bigint_sub failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_sub passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF1234567890ABCDEF");

        if(bigint_add(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_add: %s\n", str);

            if(strncmp(str, "-10000000000000000000000", 24) != 0) {
                print_error("bigint_add failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_add passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "-1234567891ABCDEF123456");

        if(bigint_add(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_add: %s\n", str);

            if(strncmp(str, "-1234567890BE02458AC602467FBE0245", 33) != 0) {
                print_error("bigint_add failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_add passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        bigint_set_str(bigint_1, "91A000000000");
        bigint_set_str(bigint_2, "1234567890ABCDEF1234567890ABCDEF");

        if(bigint_sub(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_sub: %s\n", str);

            if(strncmp(str, "-1234567890ABCDEF1233C4D890ABCDEF", 24) != 0) {
                print_error("bigint_sub failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_sub passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_sub failed");
            return -1;
        }

        bigint_set_str(bigint_1, "-1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_add(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_add: %s\n", str);

            if(strncmp(str, "4444", 4) != 0) {
                print_error("bigint_add failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_add passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_add failed");
            return -1;
        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_mul(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "5678");

        if(bigint_mul(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mul: %s\n", str);

            if(strncmp(str, "6260060", 7) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        bigint_set_str(bigint_1, "3");
        bigint_set_str(bigint_2, "-5");

        if(bigint_mul(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mul: %s\n", str);

            if(strncmp(str, "-F", 2) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234");

        if(bigint_mul(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mul: %s\n", str);

            if(strncmp(str, "14B60B60AA97760A3D760B60AA97760A28C", 35) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;

        }

        bigint_set_str(bigint_2, "FEDCBA9876543210");

        if(bigint_mul(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mul: %s\n", str);

            if(strncmp(str, "121FA00ACD77D742358D290922D96432236D88FE55618CF0", 48) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;

        }

        bigint_set_str(bigint_2, "-FEDCBA9876543210");

        if(bigint_mul(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mul: %s\n", str);

            if(strncmp(str, "-121FA00ACD77D742358D290922D96432236D88FE55618CF0", 49) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;

        }

        bigint_set_str(bigint_1, "480795EF8E54BE25D4B2");

        if(bigint_mul(bigint_1, bigint_1, bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_mul square on self: %s\n", str);

            if(strncmp(str, "144444904AFA2F71646AF4F5A6613A1F642B4BC4", 40) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;

        }

        bigint_set_str(bigint_1, "480795EF8E54BE25D4B275AC532E410C48D115DC755780CE2C0327E4581BF3");

        if(bigint_mul(bigint_1, bigint_1, bigint_1) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;
        }

        str = bigint_to_str(bigint_1);

        if (str) {
            printf("bigint_mul square on self: %s\n", str);

            if(strncmp(str, "144444904AFA2F71646B372D8E6E9EAE180F1E88D62D1CA9436B8B99710477CB646E30BC3F0ABFD28821F694C79003E8A62EB8775DD61EB54626121D28A9", 124) != 0) {
                print_error("bigint_mul failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mul passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mul failed");
            return -1;

        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_pow(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "3");
        bigint_set_str(bigint_2, "2");

        if(bigint_pow(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_pow failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_pow: %s\n", str);

            if(strncmp(str, "9", 1) != 0) {
                print_error("bigint_pow failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_pow passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_pow failed");
            return -1;
        }

        bigint_set_str(bigint_1, "12");
        bigint_set_str(bigint_2, "34");

        if(bigint_pow(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_pow failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_pow: %s\n", str);

            if(strncmp(str, "1C9040830AA8880352DFDF4C48E4FBA82690BE45210000000000000", 55) != 0) {
                print_error("bigint_pow failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_pow passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_pow failed");
            return -1;

        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_div(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "56");

        if(bigint_div(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_div: %s\n", str);

            if(strncmp(str, "36", 2) != 0) {
                print_error("bigint_div failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_div passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890AB");
        bigint_set_str(bigint_2, "56");

        if(bigint_div(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_div: %s\n", str);

            if(strncmp(str, "3630A22567", 2) != 0) {
                print_error("bigint_div failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_div passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234");

        if(bigint_div(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_div: %s\n", str);

            if(strncmp(str, "10004C01602D88D768FF32BC2824B", 29) != 0) {
                print_error("bigint_div failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_div passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;

        }

        bigint_set_str(bigint_2, "FEDCBA9876543210");

        if(bigint_div(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_div: %s\n", str);

            if(strncmp(str, "124924923F07FFFE", 16) != 0) {
                print_error("bigint_div failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_div passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;

        }

        bigint_set_str(bigint_2, "-FEDCBA9876543210");

        if(bigint_div(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_div: %s\n", str);

            if(strncmp(str, "-124924923F07FFFF", 17) != 0) {
                print_error("bigint_div failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_div passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;

        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");

        if(bigint_div(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_div: %s\n", str);

            if(strncmp(str, "124924923F07FFFE", 16) != 0) {
                print_error("bigint_div failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_div passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_div failed");
            return -1;

        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_mod(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "56");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "10", 2) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890AB");
        bigint_set_str(bigint_2, "56");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "11", 2) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "10B3", 4) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;

        }

        bigint_set_str(bigint_2, "FEDCBA9876543210");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "FC6C9395FCD4320F", 16) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;

        }

        bigint_set_str(bigint_2, "-FEDCBA9876543210");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "-270270279800001", 16) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;

        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "-FC6C9395FCD4320F", 17) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;

        }

        bigint_set_str(bigint_2, "FEDCBA9876543210");

        if(bigint_mod(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_mod: %s\n", str);

            if(strncmp(str, "270270279800001", 15) != 0) {
                print_error("bigint_mod failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_mod passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_mod failed");
            return -1;

        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_gcd(void) {
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();

    if(bigint_1 && bigint_2 && bigint_3) {
        bigint_set_str(bigint_1, "1234");
        bigint_set_str(bigint_2, "56");

        if(bigint_gcd(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        const char_t* str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_gcd: %s\n", str);

            if(strncmp(str, "2", 1) != 0) {
                print_error("bigint_gcd failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_gcd passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890AB");
        bigint_set_str(bigint_2, "56");

        if(bigint_gcd(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_gcd: %s\n", str);

            if(strncmp(str, "1", 1) != 0) {
                print_error("bigint_gcd failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_gcd passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        bigint_set_str(bigint_1, "1234567890ABCDEF1234567890ABCDEF");
        bigint_set_str(bigint_2, "1234");

        if(bigint_gcd(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_gcd: %s\n", str);

            if(strncmp(str, "5", 1) != 0) {
                print_error("bigint_gcd failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_gcd passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;

        }

        bigint_set_str(bigint_2, "FEDCBA9876543210");

        if(bigint_gcd(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_gcd: %s\n", str);

            if(strncmp(str, "F", 1) != 0) {
                print_error("bigint_gcd failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_gcd passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;

        }

        bigint_set_str(bigint_2, "-FEDCBA9876543210");

        if(bigint_gcd(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_gcd: %s\n", str);

            if(strncmp(str, "F", 1) != 0) {
                print_error("bigint_gcd failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_gcd passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;

        }

        bigint_set_str(bigint_1, "-1234567890ABCDEF1234567890ABCDEF");

        if(bigint_gcd(bigint_3, bigint_1, bigint_2) == -1) {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;
        }

        str = bigint_to_str(bigint_3);

        if (str) {
            printf("bigint_gcd: %s\n", str);

            if(strncmp(str, "F", 1) != 0) {
                print_error("bigint_gcd failed");
                memory_free((void*)str);
                bigint_destroy(bigint_1);
                bigint_destroy(bigint_2);
                bigint_destroy(bigint_3);
                return -1;
            } else {
                print_success("bigint_gcd passed");
            }

            memory_free((void*)str);
        } else {
            bigint_destroy(bigint_1);
            bigint_destroy(bigint_2);
            bigint_destroy(bigint_3);
            print_error("bigint_gcd failed");
            return -1;

        }

    } else {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);

    return 0;
}

static int32_t bigint_test_mul_mod(void){
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();
    bigint_t* bigint_4 = bigint_create();

    const char_t* str = NULL;

    if(!bigint_1 || !bigint_2 || !bigint_3 || !bigint_4) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_set_str(bigint_1, "41929F51");
    bigint_set_str(bigint_2, "4DAA51D9");
    bigint_set_str(bigint_3, "9B54A3B3");

    if(bigint_mul_mod(bigint_4, bigint_1, bigint_2, bigint_3) == -1) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_mul_mod failed");
        return -1;
    }

    str = bigint_to_str(bigint_4);

    if(!str) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_to_str failed");
        return -1;
    }

    printf("bigint_mul_mod: %s\n", str);

    if(strncmp(str, "2CE10231", 8) != 0) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        memory_free((void*)str);
        print_error("bigint_mul_mod failed");
        return -1;
    }

    memory_free((void*)str);

    print_success("bigint_mul_mod passed");

    bigint_set_str(bigint_1, "480795EF8E54BE25D4B275AC532E410C48D115DC755780CE2C0327E4581BF3");
    bigint_set_str(bigint_2, "480795EF8E54BE25D4B275AC532E410C48D115DC755780CE2C0327E4581BF3");
    bigint_set_str(bigint_3, "9B54A3A2FDB8D8A969870D87E3C05C21DA5F74CD413596E2213FBC7BFA800E3B");

    if(bigint_mul_mod(bigint_4, bigint_1, bigint_2, bigint_3) == -1) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_mul_mod failed");
        return -1;
    }

    str = bigint_to_str(bigint_4);

    if(!str) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_to_str failed");
        return -1;
    }

    printf("bigint_mul_mod: %s\n", str);

    if(strncmp(str, "8FD619722A77625BA23AD86CEF48FAB83EC2E83B905B4A475E5BD88383296653", 64) != 0) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        memory_free((void*)str);
        print_error("bigint_mul_mod failed");
        return -1;
    }

    memory_free((void*)str);

    print_success("bigint_mul_mod passed");

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);
    bigint_destroy(bigint_4);

    return 0;
}

static int32_t bigint_test_pow_mod(void){
    bigint_t* bigint_1 = bigint_create();
    bigint_t* bigint_2 = bigint_create();
    bigint_t* bigint_3 = bigint_create();
    bigint_t* bigint_4 = bigint_create();

    const char_t* str = NULL;

    if(!bigint_1 || !bigint_2 || !bigint_3 || !bigint_4) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_set_str(bigint_1, "41929F51");
    bigint_set_str(bigint_2, "4DAA51D9");
    bigint_set_str(bigint_3, "9B54A3B3");

    if(bigint_pow_mod(bigint_4, bigint_1, bigint_2, bigint_3) == -1) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_pow_mod failed");
        return -1;
    }

    str = bigint_to_str(bigint_4);

    if(!str) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_to_str failed");
        return -1;
    }

    printf("bigint_pow_mod: %s\n", str);

    if(strncmp(str, "1", 1) != 0) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        memory_free((void*)str);
        print_error("bigint_pow_mod failed");
        return -1;
    }

    memory_free((void*)str);

    print_success("bigint_pow_mod passed");

    bigint_set_str(bigint_1, "4FA20A442C5B6AC2CDC7B9A0F1E996F6F819B68C8289F8132BC722D04B85DCB0");
    bigint_set_str(bigint_2, "4DAA51D17EDC6C54B4C386C3F1E02E10ED2FBA66A09ACB71109FDE3DFD40071D");
    bigint_set_str(bigint_3, "9B54A3A2FDB8D8A969870D87E3C05C21DA5F74CD413596E2213FBC7BFA800E3B");

    if(bigint_pow_mod(bigint_4, bigint_1, bigint_2, bigint_3) == -1) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_pow_mod failed");
        return -1;
    }

    str = bigint_to_str(bigint_4);

    if(!str) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        print_error("bigint_to_str failed");
        return -1;
    }

    printf("bigint_pow_mod: %s\n", str);

    if(strncmp(str, "1", 1) != 0) {
        bigint_destroy(bigint_1);
        bigint_destroy(bigint_2);
        bigint_destroy(bigint_3);
        bigint_destroy(bigint_4);
        memory_free((void*)str);
        print_error("bigint_pow_mod failed");
        return -1;
    }

    memory_free((void*)str);

    print_success("bigint_pow_mod passed");

    bigint_destroy(bigint_1);
    bigint_destroy(bigint_2);
    bigint_destroy(bigint_3);
    bigint_destroy(bigint_4);

    return 0;
}

static int32_t bigint_test_prime(void) {
    bigint_t* bigint_1 = bigint_create();

    if(bigint_1) {
        bigint_set_str(bigint_1, "1234");

        if(bigint_is_prime(bigint_1)) {
            bigint_destroy(bigint_1);
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");

        bigint_set_str(bigint_1, "3D");

        if(!bigint_is_prime(bigint_1)) {
            bigint_destroy(bigint_1);
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");

        bigint_set_str(bigint_1, "8D6BEBFBC797F75551AAFC5679E488C9A42576BBB377C1F3");

        if(!bigint_is_prime(bigint_1)) {
            bigint_destroy(bigint_1);
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");

        bigint_set_str(bigint_1, "9B54A3A2FDB8D8A969870D87E3C05C21DA5F74CD413596E2213FBC7BFA800E3B");

        if(!bigint_is_prime(bigint_1)) {
            bigint_destroy(bigint_1);
            print_error("bigint_is_prime failed");
            return -1;
        }

        print_success("bigint_is_prime passed");


        int32_t test_count = 5;
        int32_t bit_count = 32;

        while(test_count--) {
            bigint_destroy(bigint_1);

            bigint_1 = bigint_random_prime(bit_count);

            if(bigint_1) {
                print_success("bigint_random_prime passed");

                const char_t* str = bigint_to_str(bigint_1);
                printf("bigint_random_prime: %s (%i-bits)\n", str, bit_count);
                memory_free((void*)str);
            } else {
                bigint_destroy(bigint_1);
                print_error("bigint_random_prime failed");
                return -1;
            }

            bit_count <<= 1;
        }


        print_success("prime tests passed");
    } else {
        bigint_destroy(bigint_1);
        print_error("bigint_create failed");
        return -1;
    }

    bigint_destroy(bigint_1);

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

    result = bigint_test_mul_mod();

    if(result != 0) {
        return result;
    }

    result = bigint_test_pow_mod();

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
