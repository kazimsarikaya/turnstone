/**
 * @file helloworld_cpp.64.cpp
 * @brief Hello World test program.
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <helloworld.h>
#include <memory.h>
#include <strings.h>
#include <assert.h>
#include <stdbufs.h>

/*! module name */
MODULE("turnstone.user.programs.helloworld_cpp");

class string {
private:
char * str;
int    len;

public:
string(const char * s) {
    len = strlen(s);
    str = new char[len + 1];
    assert(str != NULL && "Failed to allocate memory\n");
    printf("Allocated memory %p\n", str);
    strcopy(s, str);
}
~string() {
    delete[] str;
}

int length() {
    return len;
}

void print() {
    printf("%s\n", str);
}

};


void hello_world_cpp_test(void) {
    string * s = new string(hello_world());

    assert(s != NULL && "Failed to allocate memory\n");

    printf("String: %p\n", s);

    s->print();

    printf("Length: %d\n", s->length());

    delete s;
}
