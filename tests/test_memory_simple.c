/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"

int main(){
    FILE* fp;


    fp = fopen( "tmp/mem0.dump", "w" );
    fwrite(mem_area, 1, RAMSIZE, fp );
    fclose(fp);

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

    fp = fopen( "tmp/mem2.dump", "w" );
    fwrite(mem_area, 1, RAMSIZE, fp );
    fclose(fp);

    print_success("OK");

    printf("testing aligned malloc\n");

    uint8_t* items[10];

    for(int i = 0; i < 10; i++) {
        items[i] = memory_malloc_ext(NULL, 0x1000, 0x1000);
        printf("0x%08lx\n", items[i] );
        memory_free_ext(NULL, items[i]);
    }


    for(int i = 0; i < 10; i++) {
        items[i] = memory_malloc_ext(NULL, 0x1000, 0x100);
        printf("0x%08lx\n", items[i] );
        memory_free_ext(NULL, items[i]);
    }

    memory_get_heap_stat(&stat);
    printf("mc 0x%lx fc 0x%lx ts 0x%lx fs 0x%lx 0x%lx\n", stat.malloc_count, stat.free_count, stat.total_size, stat.free_size, stat.total_size - stat.free_size);

    fp = fopen( "tmp/mem3.dump", "w" );
    fwrite(mem_area, 1, RAMSIZE, fp );
    fclose(fp);
    return 0;
}
