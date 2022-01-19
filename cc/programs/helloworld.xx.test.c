/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tests.h>
#include <helloworld.h>
#include <strings.h>


TEST_FUNC(helloworld, group1, helloworld) {
	UNUSED(test_no);

	char_t* res = hello_world();

	if(strcmp(res, "Hello World\n") == 0) {
		return 0;
	}

	return -1;
}

TEST_FUNC(helloworld, group2, helloworld) {
	UNUSED(test_no);

	char_t* res = hello_world();

	if(strcmp(res, "Hello World\n") == 0) {
		return 0;
	}

	return -1;
}
