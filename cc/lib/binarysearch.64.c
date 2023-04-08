/**
 * @file binarsearch.64.h
 * @brief binarysearch interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <binarysearch.h>

void* binarsearch(void* list, uint64_t size, uint64_t item_size, void* key, binarsearch_comparator_f cmp) {
    uint8_t* tmp_list = list;

    uint64_t first = 0;
    uint64_t last = size - 1;

    void* first_item = tmp_list + (first * item_size);
    void* last_item = tmp_list + (last * item_size);

    while (cmp(first_item, last_item) < 1) {
        uint64_t middle = (first + last) / 2;
        void* middle_item = tmp_list + (middle * item_size);

        if(cmp(middle_item, key) == -1) {
            first = middle + 1;
            first_item = tmp_list + (first * item_size);
        }
        else if(cmp(middle_item, key) == 1) {
            last = middle - 1;
            last_item = tmp_list + (last * item_size);
        }
        else {
            return middle_item;
        }
    }

    return NULL;
}
