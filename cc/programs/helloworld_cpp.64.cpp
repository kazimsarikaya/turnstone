/**
 * @file helloworld_cpp.64.cpp
 * @brief Hello World test program.
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <helloworld.h>
#include <cppruntime/cppstring.hpp>
#include <cppruntime/cppmemview.hpp>
#include <stdbufs.h>
#include <assert.h>

/*! module name */
MODULE("turnstone.user.programs.helloworld_cpp");

using namespace tosccp;

void hello_world_cpp_test(void) {
    string* s1 = new string("Hello, ");
    assert(s1 != NULL && "Failed to allocate memory\n");

    string* s2 = new string("World!");

    if(s2 == NULL) {
        delete s1;
    }

    assert(s2 != NULL && "Failed to allocate memory\n");

    string s = *s1 + *s2;

    printf("Length: %li String: %s first char: %c\n", s.size(), s.c_str(), s[0]);


    string* sub = s.substr(7, 5);

    delete s1;
    delete s2;

    printf("Substring: %s\n", sub->c_str());

    delete sub;

    string s3(64);

    for(int64_t i = 0; i < 16; i++) {
        s3[i] = 'A' + i;
    }

    printf("Length: %li String: %s\n", s3.size(), s3.c_str());

    MemoryView<uint8_t> mv((uint8_t*)s3.c_str(), s3.size());

    mv[63] = 'X';

    for(int64_t i = 0; i < mv.size(); i++) {
        if(i % 16 == 0) {
            printf("%04llx: ", i);
        }

        printf("%02x ", mv[i]);

        if(i % 16 == 15) {
            printf("\n");
        }
    }
    printf("\n");

    // mv[100] = 'Y'; // This should fail

}
