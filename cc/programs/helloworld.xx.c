/**
 * @file helloworld.xx.c
 * @brief Hello World test program.
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <helloworld.h>

/*! module name */
MODULE("turnstone.user.programs.helloworld");


const char_t* hello_world(void){
    return "Hello, World\n";
}
