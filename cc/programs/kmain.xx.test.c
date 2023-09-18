/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tests.h>
#include <logging.h>

extern size_t __test_functions_array_start;
extern size_t __test_functions_array_end;
extern size_t __test_functions_names;

int8_t kmain_test(size_t entry_point) {
    UNUSED(entry_point);

    video_clear_screen();

    uint32_t failed_count = 0;

    size_t t_f_start_addr = (size_t)&__test_functions_array_start;
    size_t t_f_end_addr =  (size_t)&__test_functions_array_end;

    test_func* t_f_start = (test_func*)t_f_start_addr;
    char_t** t_f_names = (char_t**)&__test_functions_names;

    test_init_output();

    size_t t_f_count = (t_f_end_addr - t_f_start_addr) / sizeof(size_t);

    for(size_t test_no = 0; test_no < t_f_count; test_no++) {
        test_print("TEST");

        test_print(t_f_names[test_no]);

        test_func t_f = t_f_start[test_no];

        if(t_f(test_no) == 0) {
            test_print(" PASS\n");
        } else {
            test_print(" FAIL\n");
            failed_count++;
        }
    }

    test_exit_qemu(failed_count);

    return 0;
}
