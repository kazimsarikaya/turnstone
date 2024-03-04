/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAM_SIZE 0x1000000 // 16 MB
#include "setup.h"
#include <assert.h>
#include <strings.h>

int main(void);

int main(void) {
    char_t* data = strdup("Hello, World!");
    assert(1 == 1 && "ok");
    assert(1 == 0 && "fail");

    printf("data: %s\n", data);
    memory_free(data);

    return 0;
}
