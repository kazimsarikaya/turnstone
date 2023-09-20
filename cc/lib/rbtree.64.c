/*
 * @file rbltree.64.c
 * @brief rbltree interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <rbtree.h>
#include <cpu/sync.h>
#include <linkedlist.h>

MODULE("turnstone.lib");

typedef enum rbtree_color_t {
    RBTREE_COLOR_RED,
    RBTREE_COLOR_BLACK,
}rbtree_color_t;

typedef struct rbtree_node_t rbtree_node_t;

struct rbtree_node_t {
    rbtree_node_t* parent;
    rbtree_node_t* left;
    rbtree_node_t* right;
    rbtree_color_t color;
    const void*    key;
    const void*    data;
};

boolean_t      rbtree_node_is_on_left(rbtree_node_t* rbn);
boolean_t      rbtree_node_has_red_child(rbtree_node_t* rbn);
rbtree_node_t* rbtree_node_uncle(rbtree_node_t* rbn);
rbtree_node_t* rbtree_node_sibling(rbtree_node_t* rbn);
void           rbtree_node_move_down(rbtree_node_t* rbn, rbtree_node_t* new_parent);
rbtree_node_t* rbtree_node_new(memory_heap_t* heap, const void* key, const void* data);
void           rbtree_node_swap_colors(rbtree_node_t* rbn1, rbtree_node_t* rbn2);
void           rbtree_node_swap_values(rbtree_node_t* rbn1, rbtree_node_t* rbn2);
rbtree_node_t* rbtree_node_successor(rbtree_node_t* x);
rbtree_node_t* rbtree_node_bst_replace(rbtree_node_t* x);


rbtree_node_t* rbtree_node_new(memory_heap_t* heap, const void* key, const void* data) {
    rbtree_node_t* res = memory_malloc_ext(heap, sizeof(rbtree_node_t), 0);

    if(!res) {
        return NULL;
    }

    res->color = RBTREE_COLOR_RED;
    res->key = key;
    res->data = data;

    return res;
}

boolean_t rbtree_node_is_on_left(rbtree_node_t* rbn) {
    if(!rbn->parent) {
        return false;
    }

    return rbn == rbn->parent->left;
}

boolean_t rbtree_node_has_red_child(rbtree_node_t* rbn) {
    return (rbn->left && rbn->left->color == RBTREE_COLOR_RED) || (rbn->right && rbn->right->color == RBTREE_COLOR_RED);
}

rbtree_node_t* rbtree_node_uncle(rbtree_node_t* rbn) {
    if(rbn->parent == NULL || rbn->parent->parent == NULL) {
        return NULL;
    }

    if(rbtree_node_is_on_left(rbn->parent)) {
        return rbn->parent->parent->right;
    }

    return rbn->parent->parent->left;
}

rbtree_node_t* rbtree_node_sibling(rbtree_node_t* rbn) {
    if(!rbn || rbn->parent == NULL) {
        return NULL;
    }

    if(rbtree_node_is_on_left(rbn)) {
        return rbn->parent->right;
    }

    return rbn->parent->left;
}

void rbtree_node_move_down(rbtree_node_t* rbn, rbtree_node_t* new_parent) {
    if(rbn->parent) {
        if(rbtree_node_is_on_left(rbn)) {
            rbn->parent->left = new_parent;
        } else {
            rbn->parent->right = new_parent;
        }
    }

    new_parent->parent = rbn->parent;
    rbn->parent = new_parent;
}
void rbtree_node_swap_colors(rbtree_node_t* rbn1, rbtree_node_t* rbn2) {
    rbtree_color_t temp;
    temp = rbn1->color;
    rbn1->color = rbn2->color;
    rbn2->color = temp;
}

void rbtree_node_swap_values(rbtree_node_t* rbn1, rbtree_node_t* rbn2) {
    const void* temp;
    temp = rbn1->key;
    rbn1->key = rbn2->key;
    rbn2->key = temp;

    temp = rbn1->data;
    rbn1->data = rbn2->data;
    rbn2->data = temp;
}

rbtree_node_t* rbtree_node_successor(rbtree_node_t* x) {
    rbtree_node_t* temp = x;

    while(temp->left != NULL) {
        temp = temp->left;
    }

    return temp;
}

rbtree_node_t* rbtree_node_bst_replace(rbtree_node_t* x) {
    if(x->left != NULL && x->right != NULL) {
        return rbtree_node_successor(x->right);
    }

    if(x->left == NULL && x->right == NULL) {
        return NULL;
    }

    if(x->left != NULL) {
        return x->left;
    } else {
        return x->right;
    }
}

typedef struct rbtree_t rbtree_t;

struct rbtree_t {
    memory_heap_t* heap;
    lock_t         lock;
    uint64_t       size;
    rbtree_node_t* root;
};

void          rbtree_left_rotate(rbtree_t* rbt, rbtree_node_t* rbn);
void          rbtree_right_rotate(rbtree_t* rbt, rbtree_node_t* rbn);
void          rbtree_fix_redred(rbtree_t* rbt, rbtree_node_t* rbn);
void          rbtree_fix_blackblack(rbtree_t* rbt, rbtree_node_t* rbn);
void          rbtree_delete_node(rbtree_t* rbt, rbtree_node_t* v);
boolean_t     rbtree_search_node(rbtree_t* rbt, const void* key, index_key_comparator_f cmp, rbtree_node_t** res);
uint64_t      rbtree_size(index_t* idx);
int8_t        rbtree_insert(index_t* idx, const void* key, const void* data, void** removed_data);
int8_t        rbtree_delete(index_t* idx, const void* key, void** removed_data);
iterator_t*   rbtree_search(index_t* idx, const void* key1, const void* key2, const index_key_search_criteria_t criteria);
iterator_t*   rbtree_create_iterator(index_t* idx);
boolean_t     rbtree_contains(index_t* idx, const void* key);
linkedlist_t* rbtree_inorder(index_t* idx, const void* key1, const void* key2, index_key_search_criteria_t criteria);

void rbtree_left_rotate(rbtree_t* rbt, rbtree_node_t* rbn) {
    rbtree_node_t* new_parent = rbn->right;

    if(rbn == rbt->root) {
        rbt->root = new_parent;
    }

    rbtree_node_move_down(rbn, new_parent);

    rbn->right = new_parent->left;

    if(new_parent->left) {
        new_parent->left->parent = rbn;
    }

    new_parent->left = rbn;
}

void rbtree_right_rotate(rbtree_t* rbt, rbtree_node_t* rbn) {
    rbtree_node_t* new_parent = rbn->left;

    if(rbn == rbt->root) {
        rbt->root = new_parent;
    }

    rbtree_node_move_down(rbn, new_parent);

    rbn->left = new_parent->right;

    if(new_parent->right) {
        new_parent->right->parent = rbn;
    }

    new_parent->right = rbn;
}

void rbtree_fix_redred(rbtree_t* rbt, rbtree_node_t* rbn) {
    while(true) {
        if(rbn == rbt->root) {
            rbn->color = RBTREE_COLOR_BLACK;

            break;
        }

        rbtree_node_t* parent = rbn->parent;
        rbtree_node_t* grandparent = parent->parent;
        rbtree_node_t* uncle = rbtree_node_uncle(rbn);

        if(parent->color != RBTREE_COLOR_BLACK) {
            if(uncle != NULL && uncle->color == RBTREE_COLOR_RED) {
                parent->color = RBTREE_COLOR_BLACK;
                uncle->color = RBTREE_COLOR_BLACK;
                grandparent->color = RBTREE_COLOR_RED;

                rbn = grandparent;
            } else {
                if (rbtree_node_is_on_left(parent)) {
                    if(rbtree_node_is_on_left(rbn)) {
                        rbtree_node_swap_colors(parent, grandparent);
                    } else {
                        rbtree_left_rotate(rbt, parent);
                        rbtree_node_swap_colors(rbn, grandparent);
                    }

                    rbtree_right_rotate(rbt, grandparent);
                } else {
                    if(rbtree_node_is_on_left(rbn)) {
                        rbtree_right_rotate(rbt, parent);
                        rbtree_node_swap_colors(rbn, grandparent);
                    } else {
                        rbtree_node_swap_colors(parent, grandparent);
                    }

                    rbtree_left_rotate(rbt, grandparent);
                }

                break;
            }
        } else {
            break;
        }
    }
}

void rbtree_fix_blackblack(rbtree_t* rbt, rbtree_node_t* rbn) {
    while(true) {
        if (rbn == rbt->root) {
            break;
        }

        rbtree_node_t* sibling = rbtree_node_sibling(rbn);
        rbtree_node_t* parent = rbn->parent;

        if(!sibling) {
            rbn = parent;
        } else {
            if(sibling->color == RBTREE_COLOR_RED) {
                parent->color = RBTREE_COLOR_RED;
                sibling->color = RBTREE_COLOR_BLACK;

                if(rbtree_node_is_on_left(sibling)) {
                    rbtree_right_rotate(rbt, parent);
                } else {
                    rbtree_left_rotate(rbt, parent);
                }
            } else {
                if(rbtree_node_has_red_child(sibling)) {
                    if(sibling->left && sibling->left->color == RBTREE_COLOR_RED) {
                        if(rbtree_node_is_on_left(sibling)) {
                            sibling->left->color = sibling->color;
                            sibling->color = parent->color;
                            rbtree_right_rotate(rbt, parent);
                        } else {
                            sibling->left->color = parent->color;
                            rbtree_right_rotate(rbt, sibling);
                            rbtree_left_rotate(rbt, parent);
                        }
                    } else {
                        if(rbtree_node_is_on_left(sibling)) {
                            sibling->right->color = parent->color;
                            rbtree_left_rotate(rbt, sibling);
                            rbtree_right_rotate(rbt, parent);
                        } else {
                            sibling->right->color = sibling->color;
                            sibling->color = parent->color;
                            rbtree_left_rotate(rbt, parent);
                        }
                    }

                    parent->color = RBTREE_COLOR_BLACK;

                    break;
                } else {
                    sibling->color = RBTREE_COLOR_RED;

                    if(parent->color == RBTREE_COLOR_BLACK) {
                        rbn = parent;
                    } else {
                        parent->color = RBTREE_COLOR_BLACK;

                        break;
                    }
                }
            }
        }
    }
}

void rbtree_delete_node(rbtree_t* rbt, rbtree_node_t* v) {
    while(true) {
        rbtree_node_t* u = rbtree_node_bst_replace(v);

        boolean_t uvBlack = ((u == NULL || ( u && u->color == RBTREE_COLOR_BLACK)) && (v->color == RBTREE_COLOR_BLACK));
        rbtree_node_t* parent = v->parent;

        if(u == NULL) {
            if(v == rbt->root) {
                rbt->root = NULL;
            } else {
                if (uvBlack) {
                    rbtree_fix_blackblack(rbt, v);
                } else {
                    rbtree_node_t* v_sibling = rbtree_node_sibling(v);

                    if (v_sibling) {
                        v_sibling->color = RBTREE_COLOR_RED;
                    }
                }

                if(rbtree_node_is_on_left(v)) {
                    parent->left = NULL;
                } else {
                    if(parent) {
                        parent->right = NULL;
                    }
                }
            }

            memory_free_ext(rbt->heap, v);

            break;
        }

        if(v->left == NULL || v->right == NULL) {
            if (v == rbt->root) {
                v->key = u->key;
                v->data = u->data;
                v->left = NULL;
                v->right = NULL;
                memory_free_ext(rbt->heap, u);
            } else {
                if(rbtree_node_is_on_left(v)) {
                    parent->left = u;
                } else {
                    parent->right = u;
                }

                memory_free_ext(rbt->heap, v);
                u->parent = parent;

                if(uvBlack) {
                    rbtree_fix_blackblack(rbt, u);
                } else {
                    u->color = RBTREE_COLOR_BLACK;
                }
            }

            break;
        }

        rbtree_node_swap_values(u, v);

        v = u;
    }
}

boolean_t rbtree_search_node(rbtree_t* rbt, const void* key, index_key_comparator_f cmp, rbtree_node_t** res) {
    rbtree_node_t* temp = rbt->root;

    boolean_t found = false;

    while(temp != NULL) {
        int8_t c_res = cmp(key, temp->key);

        if(c_res == -1) {
            if(temp->left == NULL) {
                break;
            } else {
                temp = temp->left;

            }
        } else if (c_res == 0) {
            found = true;
            break;
        } else {
            if(temp->right == NULL) {
                break;
            } else {
                temp = temp->right;
            }
        }
    }

    if(res) {
        *res = temp;
    }

    return found;
}

uint64_t rbtree_size(index_t* idx) {
    if(!idx) {
        return 0;
    }

    rbtree_t* rbt = (rbtree_t*)idx->metadata;
    return rbt->size;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t rbtree_insert(index_t* idx, const void* key, const void* data, void** removed_data) {
    if(!idx) {
        return -1;
    }

    rbtree_t* rbt = idx->metadata;

    lock_acquire(rbt->lock);

    if(!rbt->root) {
        rbt->root = rbtree_node_new(idx->heap, key, data);

        if(!rbt->root) {
            lock_release(rbt->lock);

            return -1;
        }

        rbt->root->color = RBTREE_COLOR_BLACK;

        rbt->size++;

        lock_release(rbt->lock);

        return 0;
    }

    rbtree_node_t* fn = NULL;

    if(rbtree_search_node(rbt, key, idx->comparator, &fn)) {
        if(removed_data) {
            *removed_data = (void*)fn->data;
        }

        fn->key = key;
        fn->data = data;

        lock_release(rbt->lock);

        return 0;
    }

    rbtree_node_t* new_node = rbtree_node_new(idx->heap, key, data);

    if(!new_node) {
        lock_release(rbt->lock);

        return -1;
    }

    new_node->parent = fn;

    if(idx->comparator(key, fn->key) == -1) {
        fn->left = new_node;
    } else {
        fn->right = new_node;
    }

    rbtree_fix_redred(rbt, new_node);

    rbt->size++;

    lock_release(rbt->lock);

    return 0;
}
#pragma GCC diagnostic pop

int8_t rbtree_delete(index_t* idx, const void* key, void** removed_data) {
    if(!idx) {
        return -1;
    }

    rbtree_t* rbt = idx->metadata;

    lock_acquire(rbt->lock);

    rbtree_node_t* fn = NULL;

    if(rbtree_search_node(rbt, key, idx->comparator, &fn)) {
        if(removed_data) {
            *removed_data = (void*)fn->data;
        }

        rbtree_delete_node(rbt, fn);
        rbt->size--;
    }

    lock_release(rbt->lock);

    return 0;
}

iterator_t* rbtree_create_iterator(index_t* idx) {
    return rbtree_search(idx, NULL, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_NULL);
}

boolean_t rbtree_contains(index_t* idx, const void* key) {
    if(!idx) {
        return false;
    }

    rbtree_t* rbt = idx->metadata;

    lock_acquire(rbt->lock);

    boolean_t res = rbtree_search_node(rbt, key, idx->comparator, NULL);

    lock_release(rbt->lock);

    return res;
}

linkedlist_t* rbtree_inorder(index_t* idx, const void* key1, const void* key2, index_key_search_criteria_t criteria) {
    if(!idx) {
        return NULL;
    }


    rbtree_t* rbt = idx->metadata;

    linkedlist_t* stack = linkedlist_create_stack_with_heap(idx->heap);

    if(!stack) {
        return NULL;
    }

    linkedlist_t* results = linkedlist_create_list_with_heap(idx->heap);

    if(!results) {
        linkedlist_destroy(stack);

        return NULL;
    }

    lock_acquire(rbt->lock);

    rbtree_node_t* current = rbt->root;


    while(current != NULL || linkedlist_size(stack)) {
        while(current != NULL) {

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_NULL) {
                linkedlist_stack_push(stack, current);
                current = current->left;

                continue;
            }

            int8_t c_res_1 = idx->comparator(current->key, key1);

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_LESS) {
                if(c_res_1 == -1) {
                    linkedlist_stack_push(stack, current);
                }
            }

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_LESSOREQUAL) {
                if(c_res_1 <= 0) {
                    linkedlist_stack_push(stack, current);
                }
            }

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL) {
                if(c_res_1 == 0) {
                    linkedlist_stack_push(stack, current);
                } else if(c_res_1 == -1) {
                    break;
                }
            }

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_EQUALORGREATER) {
                if(c_res_1 >= 0) {
                    linkedlist_stack_push(stack, current);
                } else  {
                    if(current->right) {
                        current = current->right;

                        continue;
                    }
                }
            }

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_GREATER) {
                if(c_res_1 > 0) {
                    linkedlist_stack_push(stack, current);
                } else  {
                    if(current->right) {
                        current = current->right;

                        continue;
                    }
                }
            }

            if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_BETWEEN) {
                int8_t c_res_2 = idx->comparator(current->key, key2);

                if(c_res_1 >= 0 && c_res_2 <= 0) {
                    linkedlist_stack_push(stack, current);
                } else if(c_res_1 == -1) {
                    if(current->right) {
                        current = current->right;

                        continue;
                    }

                    break;
                }
            }

            current = current->left;
        }

        if(linkedlist_size(stack)) {
            current = (rbtree_node_t*)linkedlist_stack_pop(stack);
            linkedlist_list_insert(results, current);
        }

        if(current) {
            current = current->right;
        }

    }

    lock_release(rbt->lock);

    linkedlist_destroy(stack);

    return results;
}


index_t* rbtree_create_index_with_heap(memory_heap_t* heap, index_key_comparator_f comparator) {
    rbtree_t* rbt = memory_malloc_ext(heap, sizeof(rbtree_t), 0);

    if(!rbt) {
        return NULL;
    }

    index_t* idx = memory_malloc_ext(heap, sizeof(index_t), 0);

    if(!idx) {
        memory_free(rbt);

        return NULL;
    }

    rbt->heap = heap;
    rbt->lock = lock_create_with_heap(heap);

    idx->comparator = comparator;
    idx->heap = heap;
    idx->metadata = rbt;
    idx->size = rbtree_size;
    idx->insert = rbtree_insert;
    idx->delete = rbtree_delete;
    idx->search = rbtree_search;
    idx->create_iterator = rbtree_create_iterator;
    idx->contains = rbtree_contains;

    return idx;
}

int8_t rbtree_destroy_index(index_t* idx) {
    if(!idx) {
        return 0;
    }

    rbtree_t* rbt = idx->metadata;

    if(!rbt) {
        return -1;
    }

    rbtree_node_t* rbn = rbt->root;

    if(!rbn) {
        lock_destroy(rbt->lock);
        memory_free_ext(idx->heap, rbt);
        memory_free_ext(idx->heap, idx);

        return 0;
    }

    while(true) {
        if(rbn->left) {
            rbn = rbn->left;
        } else {
            if(rbn->right) {
                rbn = rbn->right;
            } else {
                rbtree_node_t* parent = rbn->parent;

                if(parent == NULL) {
                    memory_free_ext(idx->heap, rbn);

                    break;
                }

                if(rbn == parent->left) {
                    parent->left = NULL;
                } else {
                    parent->right = NULL;
                }

                memory_free_ext(idx->heap, rbn);

                rbn = parent;
            }
        }
    }

    lock_destroy(rbt->lock);
    memory_free_ext(idx->heap, rbt);
    memory_free_ext(idx->heap, idx);

    return 0;
}

typedef struct rbtree_iterator_t {
    memory_heap_t* heap;
    uint64_t       size;
    size_t         current_index;
    linkedlist_t*  items;
} rbtree_iterator_t;

int8_t      rbtree_iterator_destroy(iterator_t * iterator);
int8_t      rbtree_iterator_end_of_index(iterator_t * iterator);
iterator_t* rbtree_iterator_next (iterator_t * iterator);
const void* rbtree_iterator_get_key(iterator_t * iterator);
const void* rbtree_iterator_get_data(iterator_t* iterator);

iterator_t* rbtree_search(index_t * idx, const void* key1, const void* key2, const index_key_search_criteria_t criteria){
    if(idx == NULL) {
        return NULL;
    }

    rbtree_t* tree = (rbtree_t*) idx->metadata;

    if(tree == NULL) {
        return NULL;
    }

    rbtree_iterator_t* iter = memory_malloc_ext(idx->heap, sizeof(rbtree_iterator_t), 0x0);

    if(iter == NULL) {
        return NULL;
    }

    iter->heap = idx->heap;
    iter->items = rbtree_inorder(idx, key1, key2, criteria);
    iter->size = linkedlist_size(iter->items);

    if(!iter->items) {
        memory_free_ext(idx->heap, iter);

        return NULL;
    }

    iterator_t* iterator = memory_malloc_ext(idx->heap, sizeof(iterator_t), 0x0);

    if(iterator == NULL) {
        linkedlist_destroy(iter->items);
        memory_free_ext(idx->heap, iter);

        return NULL;
    }

    iterator->metadata = iter;
    iterator->destroy = &rbtree_iterator_destroy;
    iterator->next = &rbtree_iterator_next;
    iterator->end_of_iterator = &rbtree_iterator_end_of_index;
    iterator->get_item = &rbtree_iterator_get_data;
    iterator->delete_item = NULL;
    iterator->get_extra_data = rbtree_iterator_get_key;

    return iterator;
}



int8_t rbtree_iterator_destroy(iterator_t * iterator){
    rbtree_iterator_t* iter = (rbtree_iterator_t*) iterator->metadata;
    memory_heap_t* heap = iter->heap;

    linkedlist_destroy(iter->items);
    memory_free_ext(heap, iter);
    memory_free_ext(heap, iterator);

    return 0;
}

int8_t rbtree_iterator_end_of_index(iterator_t * iterator) {
    rbtree_iterator_t* iter = (rbtree_iterator_t*) iterator->metadata;

    return iter->current_index == iter->size?0:1;
}

iterator_t* rbtree_iterator_next (iterator_t * iterator){
    rbtree_iterator_t* iter = (rbtree_iterator_t*) iterator->metadata;

    iter->current_index++;

    if(iter->current_index >= iter->size) {
        iter->current_index = iter->size;
    }

    return iterator;
}

const void* rbtree_iterator_get_key(iterator_t * iterator) {
    rbtree_iterator_t* iter = (rbtree_iterator_t*) iterator->metadata;

    if(iter->current_index == iter->size) {
        return NULL;
    }

    rbtree_node_t* cn = (rbtree_node_t*)linkedlist_get_data_at_position(iter->items, iter->current_index);

    return cn->key;
}

const void* rbtree_iterator_get_data(iterator_t* iterator) {
    rbtree_iterator_t* iter = (rbtree_iterator_t*) iterator->metadata;

    if(iter->current_index == iter->size) {
        return NULL;
    }

    rbtree_node_t* cn = (rbtree_node_t*)linkedlist_get_data_at_position(iter->items, iter->current_index);

    return cn->data;
}


