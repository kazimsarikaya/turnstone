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

int main(void);

int main(void) {
    string * s = new string("Hello, World!");

    assert(s != NULL && "Failed to allocate memory\n");

    printf("String: %p\n", s);

    s->print();

    printf("Length: %d\n", s->length());

    delete s;

    return 0;
}
