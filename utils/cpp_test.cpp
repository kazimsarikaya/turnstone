/**
 * @file 3.cpp
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include <strings.h>
#include <assert.h>
#include <cppruntime/cppstring.hpp>
#include <cppruntime/cppmemview.hpp>
#include <helloworld.h>

int main(void);

int main(void) {
    hello_world_cpp_test();
    return 0;
}
