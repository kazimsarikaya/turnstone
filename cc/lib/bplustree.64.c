/**
 * @file bplustree.64.c
 * @brief b+ tree implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <bplustree.h>
#include <memory.h>
#include <indexer.h>
#include <linkedlist.h>
#include <utils.h>

MODULE("turnstone.lib");

/**
 * @struct bplustree_node_internal_t
 * @brief internal tree node
 *
 * keys are sorted list.
 * both datas and childs are can not be setted at same time.
 *
 * TODO: make datas and childs as union an insert node type enum
 */
typedef struct bplustree_node_internal_t {
    linkedlist_t*                     keys;       ///< keys at node (it is a sorted list)
    struct bplustree_node_internal_t* next; ///< next node at tree same tree depth
    struct bplustree_node_internal_t* previous;       ///< previous node at tree same tree depth
    struct bplustree_node_internal_t* parent;       ///< parent node
    linkedlist_t*                     datas;       ///< data values if it is leaf node
    linkedlist_t*                     childs;       ///< child nodes if it is internal node
    boolean_t                         data_as_bucket; ///< if true data is bucket
}bplustree_node_internal_t; ///< short hand for struct

/**
 * @struct bplustree_internal_t
 * @brief internal tree struct. used as metadata of index.
 */
typedef struct bplustree_internal_t {
    bplustree_node_internal_t* root; ///< root node
    uint64_t                   max_key_count;       ///< maximum key count for each node
    boolean_t                  unique;       ///< if key present replace data
    uint64_t                   size; ///< element count
} bplustree_internal_t; ///< short hand for struct

/**
 * @struct bplustree_iterator_internal_t
 * @brief internal iterator struct
 */
typedef struct bplustree_iterator_internal_t {
    memory_heap_t*                   heap;  ///< the heap used at iteration
    const bplustree_node_internal_t* current_node;  ///< the current leaf node
    size_t                           current_index;  ///< index at first leaf node
    size_t                           current_bucket_index; ///< index at bucket
    int8_t                           end_of_iter;  ///< end of iter flag
    index_key_search_criteria_t      criteria; ///< search criteria
    const void*                      key1;  ///< search key for all type
    const void*                      key2;  ///< search key for between
    index_key_comparator_f           comparator;  ///< key comparator
} bplustree_iterator_internal_t; ///< short hand for struct

/*! b+ tree insert implementation. see also index_t insert method*/
int8_t bplustree_insert(index_t* idx, const void* key, const void* data, void** removed_data);
/*! b+ tree delete implementation. see also index_t delete method*/
int8_t bplustree_delete(index_t* idx, const void* key, void** deleted_data);
/*! b+ tree contains implementation. see also index_t contains method*/
boolean_t bplustree_contains(index_t* idx, const void* key);
/*! b+ tree search implementation. see also index_t search method*/
iterator_t* bplustree_search(index_t* idx, const void* key1, const void* key2, const index_key_search_criteria_t criteria);
/*! b+ tree size implementation.*/
uint64_t bplustree_size(index_t* idx);

/**
 * @brief splits node into two half with min key constraint.
 * @param[in]  idx         b+ tree index
 * @param[in]  node        source node
 * @param[out]  ptr_par_key the middle key of partition (right node min key)
 * @return             new node (right one)
 */
bplustree_node_internal_t* bplustree_split_node(index_t* idx, bplustree_node_internal_t* node, void** ptr_par_key);

/**
 * @brief finds left leaf min key
 * @param[in] node node to start search
 * @return min key.
 */
const void* bplustree_get_min_key(const bplustree_node_internal_t* node);
/**
 * @brief toss root, so level of tree become smaller.
 * @param[in]  idx b+ tree index
 * @return    0 if succeed.
 */
int8_t bplustree_toss_root(index_t* idx);

/**
 * @brief creates b+ tree iterator
 * @param[in]  idx source of b+ tree
 * @return iterator
 *
 * the iterator travels only leaf nodes.
 */
iterator_t* bplustree_iterator_create(index_t* idx);

/**
 * @brief destroys the iterator
 * @param[in]  iterator iterator to destroy
 * @return  0 if succeed
 */
int8_t bplustree_iterator_destroy(iterator_t* iterator);

/**
 * @brief returns 0 at and of tree
 * @param[in]  iterator iterator to check
 * @return   0 if end of tree.
 */
int8_t bplustree_iterator_end_of_index(iterator_t* iterator);

/**
 * @brief fetches next key/value
 * @param[in]  iterator iterator to travel
 * @return   itself
 */
iterator_t* bplustree_iterator_next(iterator_t* iterator);

/**
 * @brief returns current key at iterator.
 * @param[in] iterator iterator to get key
 * @return the key.
 */
const void* bplustree_iterator_get_key(iterator_t* iterator);

/**
 * @brief returns current data at iterator.
 * @param[in] iterator iterator to get data
 * @return the data.
 */
const void* bplustree_iterator_get_data(iterator_t* iterator);

index_t* bplustree_create_index_with_heap_and_unique(memory_heap_t* heap, uint64_t max_key_count,
                                                     index_key_comparator_f comparator, boolean_t unique){
    if(max_key_count < 2) {
        return NULL;
    }
    bplustree_internal_t* tree = memory_malloc_ext(heap, sizeof(bplustree_internal_t), 0x0);

    if(tree == NULL) {
        return NULL;
    }

    tree->root = NULL;
    tree->max_key_count = max_key_count;
    tree->unique = unique;

    index_t* idx = memory_malloc_ext(heap, sizeof(index_t), 0x0);

    if(idx == NULL) {
        memory_free_ext(heap, tree);

        return NULL;
    }

    idx->heap = heap;
    idx->metadata = tree;
    idx->comparator = comparator;
    idx->insert = &bplustree_insert;
    idx->delete = &bplustree_delete;
    idx->contains = &bplustree_contains;
    idx->search = &bplustree_search;
    idx->create_iterator = &bplustree_iterator_create;
    idx->size = &bplustree_size;
    return idx;
}

int8_t bplustree_destroy_index(index_t* idx){
    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;

    if(tree->root != NULL) {
        const bplustree_node_internal_t* node = tree->root;
        const bplustree_node_internal_t* tmp_node;

        while(node != NULL) {
            if(node->childs != NULL) {
                if(linkedlist_size(node->childs) > 0) {
                    node = linkedlist_get_data_at_position(node->childs, 0);
                } else {
                    linkedlist_destroy(node->keys);
                    linkedlist_destroy(node->childs);
                    tmp_node = node->parent;

                    if(tmp_node != NULL) {
                        linkedlist_delete_at_position(tmp_node->childs, 0);
                    }

                    memory_free_ext(idx->heap, (void*)node);
                    node = tmp_node;
                }
            } else {
                linkedlist_destroy(node->keys);

                if(!tree->unique) {
                    iterator_t* iterator = linkedlist_iterator_create(node->datas);

                    while(iterator->end_of_iterator(iterator) != 0) {
                        linkedlist_t bucket = (linkedlist_t)iterator->get_item(iterator);

                        linkedlist_destroy(bucket);

                        iterator = iterator->next(iterator);
                    }

                    iterator->destroy(iterator);
                }

                linkedlist_destroy(node->datas);

                tmp_node = node->parent;

                if(tmp_node != NULL) {
                    linkedlist_delete_at_position(tmp_node->childs, 0);
                }

                memory_free_ext(idx->heap, (void*)node);
                node = tmp_node;
            }
        }
    }

    memory_free_ext(idx->heap, tree);
    memory_free_ext(idx->heap, idx);

    return 0;
}

bplustree_node_internal_t* bplustree_split_node(index_t* idx, bplustree_node_internal_t* node, void** ptr_par_key) {
    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;
    bplustree_node_internal_t* new_node = memory_malloc_ext(idx->heap, sizeof(bplustree_node_internal_t), 0x0);

    if(new_node == NULL) {
        return NULL;
    }

    new_node->keys = linkedlist_create_sortedlist_with_heap(idx->heap, idx->comparator);

    if(node->childs != NULL) { // not leaf node, needs childs
        new_node->childs = linkedlist_create_list_with_heap(idx->heap);
    } else { // leaf node needs datas
        new_node->datas = linkedlist_create_list_with_heap(idx->heap);

        if(!tree->unique) {
            new_node->data_as_bucket = true;
        }
    }

    int64_t div_pos = tree->max_key_count / 2;

    if((tree->max_key_count % 2 ) != 0) {
        div_pos++;
    }

    int64_t child_pos = 1, child_idx = 0;
    int64_t data_pos = 0, data_idx = 0;
    const void* par_key = NULL;

    iterator_t* iter = linkedlist_iterator_create(node->keys);

    while(iter->end_of_iterator(iter) != 0) {
        if(div_pos == 0) {
            const void* cur = iter->delete_item(iter);

            if(par_key == NULL) {
                par_key = cur;

                if(node->childs == NULL) {
                    linkedlist_sortedlist_insert(new_node->keys, cur); // par key is only inserted at leaf nodes
                }
            } else {
                linkedlist_sortedlist_insert(new_node->keys, cur);
            }

            if(node->childs != NULL) { //split childs too
                bplustree_node_internal_t* tmp_child = (bplustree_node_internal_t*)linkedlist_delete_at_position(node->childs, child_pos);

                if(tmp_child != NULL) {
                    linkedlist_insert_at_position(new_node->childs, tmp_child, child_idx);
                    tmp_child->parent = new_node;
                }

                child_idx++;
            }

            if(node->childs == NULL) {
                const void* tmp_data = linkedlist_delete_at_position(node->datas, data_pos);
                linkedlist_insert_at_position(new_node->datas, tmp_data, data_idx);
                data_idx++;
            }
        } else {
            child_pos++;
            data_pos++;
            div_pos--;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    new_node->previous = node;
    new_node->next = node->next;

    if(node->next != NULL) {
        node->next->previous = new_node;
    }

    node->next = new_node;

    new_node->parent = node->parent; // set new nodes parent
    *ptr_par_key = (void*)par_key;
    return new_node;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t bplustree_insert(index_t* idx, const void* key, const void* data, void** removed_data){
    if(idx == NULL) {
        return -1;
    }

    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;
    if(tree->root == NULL) {
        tree->root = memory_malloc_ext(idx->heap, sizeof(bplustree_node_internal_t), 0x0);

        if(tree->root == NULL) {
            return -1;
        }


        tree->root->keys = linkedlist_create_sortedlist_with_heap(idx->heap, idx->comparator);

        if(tree->root->keys == NULL) {
            memory_free_ext(idx->heap, tree->root);

            return -1;
        }

        linkedlist_sortedlist_insert(tree->root->keys, key);
        tree->root->datas = linkedlist_create_list_with_heap(idx->heap);

        if(tree->root->datas == NULL) {
            memory_free_ext(idx->heap, tree->root->keys);
            memory_free_ext(idx->heap, tree->root);

            return -1;
        }

        tree->size++;

        if(tree->unique) {
            linkedlist_insert_at_position(tree->root->datas, data, 0);
        } else {
            linkedlist_t bucket = linkedlist_create_list_with_heap(idx->heap);

            if(!bucket) {
                memory_free_ext(idx->heap, tree->root->keys);
                memory_free_ext(idx->heap, tree->root->datas);
                memory_free_ext(idx->heap, tree->root);

                return -1;
            }

            linkedlist_insert_at_position(bucket, data, 0);
            linkedlist_insert_at_position(tree->root->datas, bucket, 0);
            tree->root->data_as_bucket = true;
        }

    } else {
        bplustree_node_internal_t* node = tree->root;
        int8_t inserted = 0;

        while(inserted == 0) {
            if(node->childs == NULL) { // leaf node
                size_t position;
                boolean_t key_found = false;

                if(linkedlist_get_position(node->keys, key, &position) == 0) {
                    key_found = true;
                }

                if(tree->unique && key_found) {
                    if(removed_data) {
                        *removed_data = (void*)linkedlist_get_data_at_position(node->datas, position);
                    }

                    key_found = false;

                    linkedlist_delete_at_position(node->keys, position);
                    linkedlist_delete_at_position(node->datas, position);
                    tree->size--;
                }

                size_t key_pos = 0;

                if(!key_found) {
                    key_pos = linkedlist_sortedlist_insert(node->keys, key);
                } else {
                    key_pos = position;
                }

                if(tree->unique) {
                    linkedlist_insert_at_position(node->datas, data, key_pos);
                } else {
                    if(key_found) {
                        linkedlist_t bucket = (linkedlist_t)linkedlist_get_data_at_position(node->datas, key_pos);
                        linkedlist_list_insert(bucket, data);
                    } else {
                        linkedlist_t bucket = linkedlist_create_list_with_heap(idx->heap);
                        linkedlist_list_insert(bucket, data);
                        linkedlist_insert_at_position(node->datas, bucket, key_pos);
                    }

                    node->data_as_bucket = true;
                }

                tree->size++;

                void* par_key;
                bplustree_node_internal_t* new_node;

                while(linkedlist_size(node->keys) > tree->max_key_count) { // node is full split it
                    new_node = bplustree_split_node(idx, (bplustree_node_internal_t*)node, &par_key);

                    if(new_node == NULL) {
                        return -1;
                    }

                    if(node->parent == NULL) { // root node
                        node->parent = memory_malloc_ext(idx->heap, sizeof(bplustree_node_internal_t), 0x0);

                        if(node->parent == NULL) {
                            memory_free_ext(idx->heap, new_node);

                            return -1;
                        }

                        new_node->parent = node->parent;
                        node->parent->keys = linkedlist_create_sortedlist_with_heap(idx->heap, idx->comparator);

                        if(node->parent->keys == NULL) {
                            memory_free_ext(idx->heap, new_node);
                            memory_free_ext(idx->heap, node->parent);
                            node->parent = NULL;

                            return -1;
                        }

                        linkedlist_sortedlist_insert(node->parent->keys, par_key);
                        node->parent->childs = linkedlist_create_list_with_heap(idx->heap);

                        if(node->parent->keys == NULL) {
                            memory_free_ext(idx->heap, (void*)new_node);
                            memory_free_ext(idx->heap, node->parent->keys);
                            memory_free_ext(idx->heap, (void*)node->parent);
                            node->parent = NULL;

                            return -1;
                        }

                        linkedlist_insert_at_position(node->parent->childs, node, 0);
                        linkedlist_insert_at_position(node->parent->childs, new_node, 1);
                        tree->root = node->parent;

                        break; // end of upward propogation
                    } else {
                        key_pos = linkedlist_sortedlist_insert(node->parent->keys, par_key);

                        if(linkedlist_insert_at_position(node->parent->childs, new_node, key_pos + 1) != key_pos + 1) {
                            return -2;
                        }

                        node = node->parent;
                    }
                }

                inserted = 1;
            } else { // internal node
                size_t pos = 0;
                iterator_t* iter = linkedlist_iterator_create(node->keys);

                while(iter->end_of_iterator(iter) != 0) {
                    const void* cur = iter->get_item(iter);
                    if(idx->comparator(cur, key) < 0) {
                        pos++;
                    } else {
                        break;
                    }
                    iter = iter->next(iter);
                }

                iter->destroy(iter);
                node = (bplustree_node_internal_t*)linkedlist_get_data_at_position(node->childs, pos);

                if( node == NULL) {
                    return -2; // child pos always exits.
                }
            }
        }
    }

    return 0;
}
#pragma GCC diagnostic pop

const void* bplustree_get_min_key(const bplustree_node_internal_t* node) {
    if(node == NULL) {
        return NULL;
    }
    while(node->childs != NULL) {
        node = linkedlist_get_data_at_position(node->childs, 0);
    }
    return linkedlist_get_data_at_position(node->keys, 0);
}

int8_t bplustree_toss_root(index_t* idx) {
    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;
    const bplustree_node_internal_t* root = tree->root;

    if(root->childs != NULL) {
        if(linkedlist_size(root->keys) <= 1) {
            bplustree_node_internal_t* left_child = (bplustree_node_internal_t*)linkedlist_get_data_at_position(root->childs, 0);
            bplustree_node_internal_t* right_child = (bplustree_node_internal_t*)linkedlist_get_data_at_position(root->childs, 1);

            if(right_child != NULL) {

                size_t key_size = linkedlist_size(left_child->keys) + linkedlist_size(right_child->keys);
                if(left_child->childs != NULL) {
                    key_size += linkedlist_size(root->keys);
                }
                if(key_size > tree->max_key_count) {
                    return 0;
                }



                linkedlist_delete_at_position(root->childs, 0);
                linkedlist_delete_at_position(root->childs, 0);
                const void* root_key = linkedlist_delete_at_position(root->keys, 0);


                if(root_key != NULL && left_child->childs != NULL) {
                    linkedlist_insert_at(left_child->keys, root_key, LINKEDLIST_INSERT_AT_TAIL, 0);
                }

                const void* tmp;
                while(linkedlist_size(right_child->keys) > 0) {
                    tmp = linkedlist_delete_at_position(right_child->keys, 0);
                    linkedlist_insert_at(left_child->keys, tmp, LINKEDLIST_INSERT_AT_TAIL, 0);
                }


                if(left_child->childs == NULL) {
                    while(linkedlist_size(right_child->datas) > 0) {
                        tmp = linkedlist_delete_at_position(right_child->datas, 0);
                        linkedlist_insert_at(left_child->datas, tmp, LINKEDLIST_INSERT_AT_TAIL, 0);
                    }
                } else {
                    while(linkedlist_size(right_child->childs) > 0) {
                        tmp = linkedlist_delete_at_position(right_child->childs, 0);
                        linkedlist_insert_at(left_child->childs, tmp, LINKEDLIST_INSERT_AT_TAIL, 0);
                        ((bplustree_node_internal_t*)tmp)->parent = left_child;
                    }
                }

                linkedlist_destroy(right_child->keys);
                if(left_child->childs == NULL) {
                    linkedlist_destroy(right_child->datas);
                } else {
                    linkedlist_destroy(right_child->childs);
                }
                memory_free_ext(idx->heap, (void*)right_child);

            }

            linkedlist_destroy(root->keys);
            linkedlist_destroy(root->childs);
            memory_free_ext(idx->heap, (void*)root);

            left_child->next = NULL;
            left_child->parent = NULL;

            tree->root = left_child;
        }
    }
    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t bplustree_delete(index_t* idx, const void* key, void** deleted_data){
    if(idx == NULL) {
        return -1;
    }

    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;

    if(tree->root == NULL) {
        return NULL;
    }

    size_t min_keys = (tree->max_key_count + 1) / 2;

    if(((tree->max_key_count + 1) % 2 ) != 0) {
        min_keys++;
    }

    min_keys--;
    bplustree_node_internal_t* node = tree->root;
    linkedlist_t path = linkedlist_create_stack_with_heap(idx->heap);

    if(path == NULL) {
        return -1;
    }

    int8_t found = -1;
    size_t delete_item_count = 0;

    while(node != NULL) {
        size_t* position = memory_malloc_ext(idx->heap, sizeof(size_t), 0x0);

        if(position == NULL) {
            linkedlist_destroy(path);

            return -1;
        }

        *position = 0;

        if(node->childs != NULL) { //internal node
            const void* cur = NULL;
            iterator_t* iter = linkedlist_iterator_create(node->keys);

            while(iter->end_of_iterator(iter) != 0) {
                cur = iter->get_item(iter);

                if(idx->comparator(cur, key) <= 0) {
                    (*position)++;
                } else {
                    break;
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);
            node = (bplustree_node_internal_t*)linkedlist_get_data_at_position(node->childs, *position);
            linkedlist_stack_push(path, position);
        } else { //leaf node
            if(linkedlist_get_position(node->keys, key, position) != 0) {
                memory_free_ext(idx->heap, position);

                break;
            } else {
                linkedlist_stack_push(path, position);

                if(deleted_data) {
                    *deleted_data = (void*)linkedlist_get_data_at_position(node->datas, *position);
                }

                if(node->data_as_bucket) {
                    delete_item_count = linkedlist_size(*deleted_data);
                } else {
                    delete_item_count = 1;
                }

                found = 0;

                break;
            }
        }
    }

    int8_t deleted = 0;

    if(found == 0) {
        size_t* position_at_node;
        size_t key_position;
        size_t parent_key_position;
        const void* tmp_key = NULL;
        position_at_node = (size_t*)linkedlist_get_data_at_position(path, 0);
        linkedlist_delete_at_position(node->datas, *position_at_node);   // remove data
                                                                         //
        while (linkedlist_size(path) > 0) {
            position_at_node = (size_t*)linkedlist_stack_pop(path);

            if(node->childs != NULL) {
                if(*position_at_node == 0) {
                    key_position = 0;
                } else {
                    key_position = (*position_at_node) - 1;
                }
            } else {
                key_position = *position_at_node;
            }

            tmp_key = linkedlist_get_data_at_position(node->keys, key_position);

            if(tmp_key != NULL && idx->comparator(key, tmp_key) == 0) {
                linkedlist_delete_at_position(node->keys, key_position);

                if(node->childs != NULL) {
                    tmp_key = bplustree_get_min_key(linkedlist_get_data_at_position(node->childs, *position_at_node));

                    if(tmp_key != NULL) {
                        linkedlist_insert_at_position(node->keys, tmp_key, key_position);
                    }
                }
            }

            if(tmp_key == NULL && node->parent == NULL) {
                bplustree_toss_root(idx);
                memory_free_ext(idx->heap, position_at_node);

                break;
            }

            if(node->parent != NULL && // not at parent
               (linkedlist_size(node->keys) < min_keys      // not satisfies key count
                || (node->childs != NULL && linkedlist_size(node->childs) < (min_keys + 1))      // not satisfies children count
               )) {

                linkedlist_insert_delete_at_t delete_from = LINKEDLIST_DELETE_AT_FINDBY, insert_at = LINKEDLIST_INSERT_AT_ANYWHERE;
                size_t* position_at_parent = (size_t*)linkedlist_get_data_at_position(path, 0);
                bplustree_node_internal_t* left_node = NULL;
                bplustree_node_internal_t* right_node = NULL;
                int8_t left_ok = -1, right_ok = -1;
                int8_t merge_with = 1;

                if(*position_at_parent != 0) { // first check left node
                    left_node = (bplustree_node_internal_t*)linkedlist_get_data_at_position(node->parent->childs, (*position_at_parent) - 1);

                    if(linkedlist_size(left_node->keys) > min_keys) { // left node has enough
                        delete_from = LINKEDLIST_DELETE_AT_TAIL;
                        insert_at = LINKEDLIST_INSERT_AT_HEAD;
                        left_ok = 0;
                    } else {
                        merge_with = -1;
                    }

                    parent_key_position = (*position_at_parent) - 1;
                } else {
                    parent_key_position = 0;
                }


                if(left_ok != 0) { // left node failed. check right_node
                    right_node = (bplustree_node_internal_t*)linkedlist_get_data_at_position(node->parent->childs, (*position_at_parent) + 1);

                    if(right_node != NULL && linkedlist_size(right_node->keys) > min_keys) {
                        delete_from = LINKEDLIST_DELETE_AT_HEAD;
                        insert_at = LINKEDLIST_INSERT_AT_TAIL;
                        right_ok = 0;
                    }
                }

                if( left_ok != 0 && right_ok != 0) {
                    //merge

                    bplustree_node_internal_t* src;
                    if(merge_with == -1 ) {
                        src = left_node;
                        delete_from = LINKEDLIST_DELETE_AT_TAIL;
                        insert_at = LINKEDLIST_INSERT_AT_HEAD;
                    } else {
                        src = right_node;
                        delete_from = LINKEDLIST_DELETE_AT_HEAD;
                        insert_at = LINKEDLIST_INSERT_AT_TAIL;
                    }

                    if(src == NULL) {
                        break;
                    }

                    const void* tmp_etc;

                    if(node->childs == NULL) { //leaf merge
                        while(linkedlist_size(src->keys) > 0) {
                            tmp_etc = linkedlist_delete_at(src->keys, NULL, delete_from, 0);
                            linkedlist_insert_at(node->keys, tmp_etc, insert_at, 0);
                            tmp_etc = linkedlist_delete_at(src->datas, NULL, delete_from, 0);
                            linkedlist_insert_at(node->datas, tmp_etc, insert_at, 0);
                        }

                        if(merge_with == -1) {
                            linkedlist_delete_at_position(node->parent->keys, (*position_at_parent) - 1);
                            linkedlist_delete_at_position(node->parent->childs, (*position_at_parent) - 1);

                            if(src->previous != NULL) {
                                src->previous->next = node;
                            }

                            node->previous = src->previous;
                            position_at_parent = (size_t*)linkedlist_stack_pop(path);
                            (*position_at_parent)--;
                            linkedlist_stack_push(path, position_at_parent);
                        } else {
                            linkedlist_delete_at_position(node->parent->keys, *position_at_parent);
                            linkedlist_delete_at_position(node->parent->childs, (*position_at_parent) + 1);
                            node->next = src->next;

                            if(src->next != NULL) {
                                src->next->previous = node;
                            }
                        }
                    } else { // internal merge

                        if(merge_with == -1) {
                            tmp_etc = linkedlist_get_data_at_position(node->keys, 0);
                        } else {
                            tmp_etc = linkedlist_get_data_at_position(node->keys, linkedlist_size(node->keys) - 1);
                        }

                        tmp_key = linkedlist_delete_at_position(node->parent->keys, parent_key_position);
                        uint8_t allow_get_parent_key = 0;

                        if(idx->comparator(key, tmp_key) == 0) { // not the key which is now deleting
                            allow_get_parent_key = 1;
                        }

                        if(tmp_etc != NULL && idx->comparator(tmp_etc, tmp_key) == 0) {
                            allow_get_parent_key = 1;
                        }

                        if(allow_get_parent_key == 0) {
                            linkedlist_insert_at(node->keys, tmp_key, insert_at, 0);
                        }


                        size_t child_count = linkedlist_size(node->childs) + linkedlist_size(src->childs);
                        size_t key_count = linkedlist_size(node->keys) + linkedlist_size(src->keys);

                        if(child_count - key_count == 2) {
                            const bplustree_node_internal_t* tmp_key_search_node;

                            if(merge_with == -1) {
                                tmp_key_search_node = linkedlist_get_data_at_position(node->childs, 0);
                            } else {
                                tmp_key_search_node = linkedlist_get_data_at_position(node->childs, 0);
                            }

                            tmp_key = bplustree_get_min_key(tmp_key_search_node);
                            linkedlist_insert_at(node->keys, tmp_key, insert_at, 0);
                        }

                        while(linkedlist_size(src->keys) > 0) {
                            tmp_etc = linkedlist_delete_at(src->keys, NULL, delete_from, 0);
                            linkedlist_insert_at(node->keys, tmp_etc, insert_at, 0);
                        }

                        while(linkedlist_size(src->childs) > 0) {
                            tmp_etc = linkedlist_delete_at(src->childs, NULL, delete_from, 0);
                            linkedlist_insert_at(node->childs, tmp_etc, insert_at, 0);
                            ((bplustree_node_internal_t*)tmp_etc)->parent = node;
                        }


                        if(merge_with == -1) {
                            linkedlist_delete_at_position(node->parent->childs, (*position_at_parent) - 1);

                            if(src->previous != NULL) {
                                src->previous->next = node;
                            }

                            node->previous = src->previous;
                            position_at_parent = (size_t*)linkedlist_stack_pop(path);
                            (*position_at_parent)--;
                            linkedlist_stack_push(path, position_at_parent);
                        } else {
                            linkedlist_delete_at_position(node->parent->childs, (*position_at_parent) + 1);
                            node->next = src->next;

                            if(src->next != NULL) {
                                src->next->previous = node;
                            }
                        }
                    }

                    linkedlist_destroy(src->keys);

                    if(src->childs == NULL) {
                        linkedlist_destroy(src->datas);
                    } else {
                        linkedlist_destroy(src->childs);
                    }

                    memory_free_ext(idx->heap, (void*)src);
                } else {
                    //borrow

                    bplustree_node_internal_t* src;

                    if(left_ok == 0) {
                        src = left_node;
                    } else {
                        src = right_node;
                    }

                    const void* tmp_etc;

                    if(node->childs == NULL) {
                        tmp_etc = linkedlist_delete_at(src->keys, NULL, delete_from, 0);
                        linkedlist_insert_at(node->keys, tmp_etc, insert_at, 0);
                        tmp_etc = linkedlist_delete_at(src->datas, NULL, delete_from, 0);
                        linkedlist_insert_at(node->datas, tmp_etc, insert_at, 0);
                    } else {
                        linkedlist_delete_at(src->keys, NULL, delete_from, 0);
                        tmp_etc = linkedlist_delete_at(src->childs, NULL, delete_from, 0);
                        linkedlist_insert_at(node->childs, tmp_etc, insert_at, 0);
                        ((bplustree_node_internal_t*)tmp_etc)->parent = node;
                        const bplustree_node_internal_t* tmp_key_child;

                        if(left_ok == 0) {
                            tmp_key_child = linkedlist_get_data_at_position(node->childs, 1);
                            tmp_key =  bplustree_get_min_key(tmp_key_child);
                            linkedlist_insert_at(node->keys, tmp_key, LINKEDLIST_INSERT_AT_HEAD, 0);
                        } else {
                            tmp_key_child = tmp_etc;
                            tmp_key =  bplustree_get_min_key(tmp_key_child);
                            linkedlist_insert_at(node->keys, tmp_key, LINKEDLIST_INSERT_AT_TAIL, 0);
                        }
                    }

                    if(left_ok == 0) {
                        tmp_key = bplustree_get_min_key(node);
                        linkedlist_delete_at_position(node->parent->keys, parent_key_position);
                        linkedlist_insert_at_position(node->parent->keys, tmp_key, parent_key_position);
                    } else {
                        tmp_key = bplustree_get_min_key(right_node);
                        size_t tmp_pos = (*position_at_parent) == 0 ? 0 : parent_key_position + 1;
                        linkedlist_delete_at_position(node->parent->keys, tmp_pos);
                        linkedlist_insert_at_position(node->parent->keys, tmp_key, tmp_pos);
                    }
                }
            }

            memory_free_ext(idx->heap, position_at_node); // we done with key, so free it
            node = node->parent; // go parent
        }

        bplustree_toss_root(idx);

        if(linkedlist_size(tree->root->keys) == 0) {
            linkedlist_destroy(tree->root->keys);
            linkedlist_destroy(tree->root->datas);
            memory_free_ext(idx->heap, (void*)tree->root);
            tree->root = NULL;
        }
    } else {
        deleted = -1;
    }

    if(deleted == 0) {
        tree->size -= delete_item_count;
    }

    linkedlist_destroy_with_data(path);
    return deleted;
}
#pragma GCC diagnostic pop

iterator_t* bplustree_search(index_t* idx, const void* key1, const void* key2, const index_key_search_criteria_t criteria){
    if(idx == NULL) {
        return NULL;
    }

    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;

    if(tree == NULL) {
        return NULL;
    }

    bplustree_iterator_internal_t* iter = memory_malloc_ext(idx->heap, sizeof(bplustree_iterator_internal_t), 0x0);

    if(iter == NULL) {
        return NULL;
    }

    iter->heap = idx->heap;
    iter->criteria = criteria;
    iter->key1 = key1;
    iter->key2 = key2;
    iter->comparator = idx->comparator;

    const bplustree_node_internal_t* node = tree->root;

    if(node != NULL) {
        while(1 == 1) {
            if(node->childs == NULL) {
                iter->current_node = node;
                iter->current_index = 0;

                if(criteria >= INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL) {
                    size_t key_count = linkedlist_size(iter->current_node->keys);

                    iterator_t* k_iter = linkedlist_iterator_create(iter->current_node->keys);

                    while(k_iter->end_of_iterator(k_iter) != 0) {
                        const void* key_at_pos = k_iter->get_item(k_iter);

                        int8_t c_res = iter->comparator(key1, key_at_pos);

                        if(criteria == INDEXER_KEY_COMPARATOR_CRITERIA_GREATER) {
                            if(c_res < 0) {
                                iter->end_of_iter = 1;
                                break;
                            }

                        } else {
                            if(c_res == 0) {
                                iter->end_of_iter = 1;
                                break;
                            } else if(c_res < 0)  {
                                iter->current_node = NULL;
                                iter->current_index = 0;
                                iter->end_of_iter = 0;
                                break;
                            }
                        }

                        iter->current_index++;

                        if(iter->current_index == key_count) {
                            iter->current_index = 0;
                            iter->current_node =  iter->current_node->next;

                            if(iter->current_node == NULL) {
                                iter->current_index = 0;
                                iter->end_of_iter = 0;
                                break;
                            }

                            k_iter->destroy(k_iter);
                            k_iter = linkedlist_iterator_create(iter->current_node->keys);

                            key_count = linkedlist_size(iter->current_node->keys);
                        } else {
                            k_iter = k_iter->next(k_iter);
                        }
                    }

                    k_iter->destroy(k_iter);

                } else {
                    iter->end_of_iter = 1;
                }

                break;
            } else { // internal node

                if(criteria < INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL) {
                    node = linkedlist_get_data_at_position(node->childs, 0);
                } else {
                    size_t key_count = linkedlist_size(node->keys);

                    iterator_t* k_iter = linkedlist_iterator_create(node->keys);
                    size_t i = 0;

                    while(k_iter->end_of_iterator(k_iter) != 0) { //search at leaf keys
                        const void* key_at_pos = k_iter->get_item(k_iter);

                        int8_t c_res = iter->comparator(key1, key_at_pos);

                        if(c_res < 0 || (!tree->unique && c_res == 0)) {
                            node = linkedlist_get_data_at_position(node->childs, i);
                            break; // break search at internal node
                        }

                        if(i + 1 != key_count) {
                            key_at_pos = linkedlist_get_data_at_position(node->keys, i + 1);

                            c_res = iter->comparator(key1, key_at_pos);

                            if(c_res < 0 || (!tree->unique && c_res == 0)) {
                                node = linkedlist_get_data_at_position(node->childs, i);
                                break; // break search at internal node
                            }
                        } else {
                            node = linkedlist_get_data_at_position(node->childs, i + 1);
                        }

                        i++;
                        k_iter = k_iter->next(k_iter);
                    }

                    k_iter->destroy(k_iter);
                }
            } //end of internal node
        }

    } else {
        iter->end_of_iter = 0;
    }

    iterator_t* iterator = memory_malloc_ext(idx->heap, sizeof(iterator_t), 0x0);

    if(iterator == NULL) {
        memory_free_ext(idx->heap, iter);

        return NULL;
    }

    iterator->metadata = iter;
    iterator->destroy = &bplustree_iterator_destroy;
    iterator->next = &bplustree_iterator_next;
    iterator->end_of_iterator = &bplustree_iterator_end_of_index;
    iterator->get_item = &bplustree_iterator_get_data;
    iterator->delete_item = NULL;
    iterator->get_extra_data = bplustree_iterator_get_key;

    return iterator;
}

iterator_t* bplustree_iterator_create(index_t* idx){
    return bplustree_search(idx, NULL, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_NULL);
}

int8_t bplustree_iterator_destroy(iterator_t* iterator){
    bplustree_iterator_internal_t* iter = (bplustree_iterator_internal_t*)iterator->metadata;
    memory_heap_t* heap = iter->heap;
    memory_free_ext(heap, iter);
    memory_free_ext(heap, iterator);
    return 0;
}

int8_t bplustree_iterator_end_of_index(iterator_t* iterator) {
    bplustree_iterator_internal_t* iter = (bplustree_iterator_internal_t*)iterator->metadata;
    return iter->end_of_iter;
}

iterator_t* bplustree_iterator_next(iterator_t* iterator){
    bplustree_iterator_internal_t* iter = (bplustree_iterator_internal_t*)iterator->metadata;


    if(iter->current_node->data_as_bucket) {
        iter->current_bucket_index++;

        linkedlist_t bucket = (linkedlist_t)linkedlist_get_data_at_position(iter->current_node->datas, iter->current_index);

        if(linkedlist_size(bucket) > iter->current_bucket_index) {
            return iterator;
        }

        iter->current_bucket_index = 0;
    }

    iter->current_index++;

    if(linkedlist_size(iter->current_node->keys) == iter->current_index) {
        if(iter->current_node->next != NULL) {
            iter->current_node = iter->current_node->next;
            iter->current_index = 0;
            iter->current_bucket_index = 0;
        } else {
            iter->current_node = NULL;
            iter->end_of_iter = 0;
        }
    }

    if(iter->current_node != NULL && iter->criteria != INDEXER_KEY_COMPARATOR_CRITERIA_NULL) {
        const void* key_at_pos = linkedlist_get_data_at_position(iter->current_node->keys, iter->current_index);

        if(iter->criteria == INDEXER_KEY_COMPARATOR_CRITERIA_LESS && iter->comparator(key_at_pos, iter->key1) >= 0) {
            iter->current_node = NULL;
            iter->end_of_iter = 0;
        } else if((iter->criteria == INDEXER_KEY_COMPARATOR_CRITERIA_LESSOREQUAL || iter->criteria == INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL) && iter->comparator(key_at_pos, iter->key1) > 0) {
            iter->current_node = NULL;
            iter->end_of_iter = 0;
        } else if(iter->criteria == INDEXER_KEY_COMPARATOR_CRITERIA_BETWEEN && iter->comparator(key_at_pos, iter->key2) > 0) {
            iter->current_node = NULL;
            iter->end_of_iter = 0;
        }
    }

    return iterator;
}

const void* bplustree_iterator_get_key(iterator_t* iterator) {
    bplustree_iterator_internal_t* iter = (bplustree_iterator_internal_t*)iterator->metadata;

    if(iter->end_of_iter == 0) {
        return NULL;
    }

    return linkedlist_get_data_at_position(iter->current_node->keys, iter->current_index);
}

const void* bplustree_iterator_get_data(iterator_t* iterator) {
    bplustree_iterator_internal_t* iter = (bplustree_iterator_internal_t*)iterator->metadata;

    if(iter->end_of_iter == 0) {
        return NULL;
    }

    if(iter->current_node->data_as_bucket) {
        linkedlist_t bucket = (linkedlist_t)linkedlist_get_data_at_position(iter->current_node->datas, iter->current_index);

        return linkedlist_get_data_at_position(bucket, iter->current_bucket_index);
    }

    return linkedlist_get_data_at_position(iter->current_node->datas, iter->current_index);
}

uint64_t bplustree_size(index_t* idx) {
    if(!idx || !idx->metadata) {
        return 0;
    }

    bplustree_internal_t* tree = idx->metadata;

    return tree->size;
}

boolean_t bplustree_contains(index_t* idx, const void* key){
    if(idx == NULL) {
        return false;
    }

    bplustree_internal_t* tree = (bplustree_internal_t*)idx->metadata;

    if(tree->root == NULL) {
        return false;
    }

    bplustree_node_internal_t* node = tree->root;

    while(node != NULL) {
        if(node->childs != NULL) { //internal node
            const void* cur = NULL;
            uint64_t position = 0;

            iterator_t* iter = linkedlist_iterator_create(node->keys);

            while(iter->end_of_iterator(iter) != 0) {
                cur = iter->get_item(iter);

                if(idx->comparator(cur, key) <= 0) {
                    position++;
                } else {
                    break;
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);
            node = (bplustree_node_internal_t*)linkedlist_get_data_at_position(node->childs, position);
        } else { //leaf node
            if(linkedlist_contains(node->keys, key) != 0) {
                return false;
            }

            return true;
        }
    }

    return false;
}
