/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"

int main(void);

int main(void){
    memory_heap_stat_t stat;

    memory_get_heap_stat(&stat);
    printf("before malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    char* data1 = memory_malloc(sizeof(char) * 31);

    memory_get_heap_stat(&stat);
    printf("after malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    if(data1 == NULL) {
        print_error("Cannot malloc");
        return -1;
    }

    printf("data1: %p\n", data1);
    memory_memcopy("data1", data1, 5);
    print_success("data1 ok");


    memory_get_heap_stat(&stat);
    printf("before malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    char* data2 = memory_malloc(sizeof(char) * 117);

    memory_get_heap_stat(&stat);
    printf("after malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    if(data2 == NULL) {
        print_error("Cannot malloc");
        return -1;
    }

    printf("data2: %p\n", data2);
    memory_memcopy("data2", data2, 5);
    print_success("data2 ok");


    memory_get_heap_stat(&stat);
    printf("before malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    char* data3 = memory_malloc(sizeof(char) * 87);

    memory_get_heap_stat(&stat);
    printf("after malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    if(data3 == NULL) {
        print_error("Cannot malloc");
        return -1;
    }

    printf("data3: %p\n", data3);
    memory_memcopy("data3", data3, 5);
    print_success("data3 ok");

    memory_get_heap_stat(&stat);
    printf("before free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    memory_free(data2);

    memory_get_heap_stat(&stat);
    printf("after free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);


    memory_get_heap_stat(&stat);
    printf("before malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    char* data4 = memory_malloc(sizeof(char) * 29);

    memory_get_heap_stat(&stat);
    printf("after malloc mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    if(data4 == NULL) {
        print_error("Cannot malloc");
        return -1;
    }

    printf("data4: %p\n", data4);
    memory_memcopy("data4", data4, 5);
    print_success("data4 ok");


    memory_get_heap_stat(&stat);
    printf("before free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    memory_free(data1);

    memory_get_heap_stat(&stat);
    printf("after free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);


    memory_get_heap_stat(&stat);
    printf("before free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    memory_free(data3);

    memory_get_heap_stat(&stat);
    printf("after free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);


    memory_get_heap_stat(&stat);
    printf("before free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    memory_free(data4);

    memory_get_heap_stat(&stat);
    printf("after free mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);



    memory_get_heap_stat(&stat);
    printf("mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    print_success("OK");

    printf("testing aligned malloc\n");

    uint8_t** items = memory_malloc(sizeof(uint8_t*) * 10);

    if(items == NULL) {
        print_error("Cannot malloc");
        return -1;
    }

    for(int i = 0; i < 10; i++) {
        uint8_t* test = memory_malloc_ext(NULL, 0x1000, 0x1000);

        if(test == NULL) {
            print_error("Cannot malloc");
            return -1;
        }

        items[i] = test;
        test[0] = 0x55;
        printf("0x1000 aligned address %p %x\n", test, test[0]);
        memory_free_ext(NULL, test);
    }


    for(int i = 0; i < 10; i++) {
        uint8_t* test = memory_malloc_ext(NULL, 0x1000, 0x100);
        items[i] = test;
        printf("0x100 aligned address %p\n", items[i]);
        memory_free_ext(NULL, items[i]);
    }

    memory_free(items);

    memory_get_heap_stat(&stat);
    printf("mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    return 0;
}
