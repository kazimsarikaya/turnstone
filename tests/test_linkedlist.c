#include "testsetup.h"
#include <linkedlist.h>

int8_t int_comparator(const void* i, const void* j){
	int* t_i = (int*)i;
	int* t_j = (int*)j;
	if(*t_i < *t_j) {
		return -1;
	}
	if(*t_i > *t_j) {
		return 1;
	}
	return 0;
}


int main(){
	setup_ram();
	int t = 9, x = 13, y = 16, z = 7;
	int* i;
	FILE* fp;

	printf("%p %p %p %p\n", &t, &x, &y, &z);

	linkedlist_t list = linkedlist_create_list();
	print_success("Create list: OK");
	linkedlist_list_insert(list, &t);
	linkedlist_list_insert(list, &y);
	linkedlist_list_insert(list, &z);
	linkedlist_insert_at_position(list, &x, 1);
	printf("list size: %i\n", linkedlist_size(list));
	linkedlist_iterator_t iter = linkedlist_iterator_create(list);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	size_t pos;
	if(linkedlist_get_position(list, &y, &pos) == 0 ) {
		printf("position of y (%i) is: %i\n", y, pos );
	} else {
		print_error("y not found");
	}
	i = (int*)linkedlist_get_data_at_position(list, 1);
	if(i == NULL) {
		print_error("get data at postion error");
		return -1;
	}
	printf("item at pos 1 is: %i\n", *i);
	i = (int*)linkedlist_delete_at_position(list, 1);
	if(i == NULL) {
		print_error("delete at postion error");
		return -1;
	}
	printf("deleted item from pos 1 is: %i\n", *i);
	iter = linkedlist_iterator_create(list);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	linkedlist_destroy(list);
	print_success("Destroy list: OK");

	linkedlist_t queue = linkedlist_create_queue();
	print_success("Create queue: OK");
	linkedlist_queue_push(queue, &x);
	printf("item inserted: %i\n", x);
	linkedlist_queue_push(queue, &y);
	printf("item inserted: %i\n", y);
	linkedlist_queue_push(queue, &z);
	printf("item inserted: %i\n", z);
	print_success("Populate queue: OK");
	iter = linkedlist_iterator_create(queue);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	printf("poping from queue\n");
	i = (int*)linkedlist_queue_pop(queue);
	printf("item: %i\n", *i);
	i = (int*)linkedlist_queue_pop(queue);
	printf("item: %i\n", *i);
	i = (int*)linkedlist_queue_pop(queue);
	printf("item: %i\n", *i);
	linkedlist_destroy(queue);
	print_success("Destroy queue: OK");

	linkedlist_t stack = linkedlist_create_stack();
	print_success("Create stack: OK");
	linkedlist_stack_push(stack, &x);
	printf("item inserted: %i\n", x);
	linkedlist_stack_push(stack, &y);
	printf("item inserted: %i\n", y);
	linkedlist_stack_push(stack, &z);
	printf("item inserted: %i\n", z);
	print_success("Populate stack: OK");
	iter = linkedlist_iterator_create(stack);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	printf("poping from stack\n");
	i = (int*)linkedlist_stack_pop(stack);
	printf("item: %i\n", *i);
	i = (int*)linkedlist_stack_pop(stack);
	printf("item: %i\n", *i);
	i = (int*)linkedlist_stack_pop(stack);
	printf("item: %i\n", *i);
	linkedlist_destroy(stack);
	print_success("Destroy stack: OK");

	linkedlist_t sortedlist = linkedlist_create_sortedlist(int_comparator);
	print_success("Create sortedlist: OK");
	pos = 0;
	pos = linkedlist_sortedlist_insert(sortedlist, &t);
	printf("%i inserted at %i\n", t, pos);
	iter = linkedlist_iterator_create(sortedlist);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	pos = linkedlist_sortedlist_insert(sortedlist, &y);
	printf("%i inserted at %i\n", y, pos);
	iter = linkedlist_iterator_create(sortedlist);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	pos = linkedlist_sortedlist_insert(sortedlist, &z);
	printf("%i inserted at %i\n", z, pos);
	iter = linkedlist_iterator_create(sortedlist);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	pos = linkedlist_sortedlist_insert(sortedlist, &x);
	printf("%i inserted at %i\n", x, pos);
	print_success("Populate sortedlist: OK");
	printf("list size: %i\n", linkedlist_size(sortedlist));
	iter = linkedlist_iterator_create(sortedlist);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);

	printf("Test deletion\n");
	int* j = (int*)linkedlist_sortedlist_delete(sortedlist, &y);
	printf("%i deleted, deleted value: %i\n", y, *j);
	printf("list size: %i\n", linkedlist_size(sortedlist));
	iter = linkedlist_iterator_create(sortedlist);
	while(linkedlist_iterator_end_of_list(iter) != 0) {
		i = (int*)linkedlist_iterator_get_item(iter);
		printf("item: %i\n", *i);
		iter = linkedlist_iterator_next(iter);
	}
	linkedlist_iterator_destroy(iter);
	print_success("Deletion succeed");

	linkedlist_destroy(sortedlist);
	print_success("Destroy sortedlist: OK");



	print_success("All tests are finished");

	fp = fopen( "tmp/mem1.dump", "w" );
	fwrite(mem_area, 1, RAMSIZE, fp );

	fclose(fp);

	return 0;
}
