/**
 * @file hashmap.64.c
 * @brief hashmap interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hashmap.h>
#include <memory.h>
#include <cpu/sync.h>
#include <xxhash.h>
#include <strings.h>

/*! module name */
MODULE("turnstone.lib.hashmap");

/**
 * @struct hashmap_item_t
 * @brief hashmap item
 */
typedef struct hashmap_item_t {
    boolean_t   exists; ///< item exists
    const void* key; ///< item key
    const void* value; ///< item value
} hashmap_item_t; ///< hashmap item

/**
 * @struct hashmap_segment_t
 * @brief hashmap segment
 */
typedef struct hashmap_segment_t {
    uint64_t                  size; ///< segment size
    struct hashmap_segment_t* next; ///< next segment
    hashmap_item_t*           items; ///< items at segment
} hashmap_segment_t; ///< hashmap segment

/**
 * @struct hashmap_t
 * @brief hashmap
 */
struct hashmap_t {
    memory_heap_t*           heap; ///< heap
    uint64_t                 segment_capacity; ///< segment capacity
    uint64_t                 total_capacity; ///< total capacity
    uint64_t                 total_size; ///< total size
    hashmap_key_generator_f  hkg; ///< key generator
    hashmap_key_comparator_f hkc; ///< key comparator
    hashmap_segment_t*       segments; ///< segments
    lock_t*                  lock; ///< lock
}; ///< hashmap

/**
 * @brief default key generator
 * @param[in] key key
 * @return key hash xxhash64
 */
uint64_t hashmap_default_kg(const void* key);

/**
 * @brief default key comparator
 * @param[in] item1 item1
 * @param[in] item2 item2
 * @return -1 if item1 < item2, 0 if item1 == item2, 1 if item1 > item2
 */
int8_t hashmap_default_kc(const void* item1, const void* item2);

/**
 * @brief string key generator
 * @param[in] key key
 * @return key hash xxhash64
 */
uint64_t hashmap_string_kg(const void * key);

/**
 * @brief string key comparator
 * @param[in] item1 item1
 * @param[in] item2 item2
 * @return -1 if item1 < item2, 0 if item1 == item2, 1 if item1 > item2
 */
int8_t hashmap_string_kc(const void* item1, const void* item2);

uint64_t hashmap_default_kg(const void* key) {
    return (uint64_t)key;
}

uint64_t hashmap_string_kg(const void * key) {
    char_t* str_key = (char_t*)key;

    return xxhash64_hash(str_key, strlen(str_key));
}

int8_t hashmap_default_kc(const void* item1, const void* item2) {
    uint64_t ti1 = (uint64_t)item1;
    uint64_t ti2 = (uint64_t)item2;

    if(ti1 < ti2) {
        return -1;
    }

    if(ti1 > ti2) {
        return 1;
    }

    return 0;
}

int8_t hashmap_string_kc(const void* item1, const void* item2) {
    char_t* ti1 = (char_t*)item1;
    char_t* ti2 = (char_t*)item2;

    return strcmp(ti1, ti2);
}

hashmap_t*  hashmap_new_with_hkg_with_hkc(memory_heap_t* heap, uint64_t capacity, hashmap_key_generator_f hkg, hashmap_key_comparator_f hkc) {
    if(!capacity) {
        return NULL;
    }

    heap = memory_get_heap(heap);

    hashmap_t* hm = memory_malloc_ext(heap, sizeof(hashmap_t), 0);

    if(!hm) {
        return NULL;
    }

    hm->heap = heap;

    hm->lock = lock_create_with_heap(heap);

    hm->total_capacity = capacity;
    hm->segment_capacity = capacity;

    hm->hkg = hkg?hkg:hashmap_default_kg;
    hm->hkc = hkc?hkc:hashmap_default_kc;

    hm->segments = memory_malloc_ext(heap, sizeof(hashmap_segment_t), 0);

    if(!hm->segments) {
        memory_free_ext(heap, hm);

        return NULL;
    }

    hm->segments->items = memory_malloc_ext(heap, sizeof(hashmap_item_t) * hm->segment_capacity, 0);

    if(!hm->segments->items) {
        memory_free_ext(heap, hm->segments);
        memory_free_ext(heap, hm);

        return NULL;
    }

    return hm;
}

hashmap_t* hashmap_string_with_heap(memory_heap_t* heap, uint64_t capacity) {
    return hashmap_new_with_hkg_with_hkc(heap, capacity, hashmap_string_kg, hashmap_string_kc);
}

boolean_t   hashmap_destroy(hashmap_t* hm) {
    if(!hm) {
        return false;
    }

    memory_heap_t* heap = hm->heap;

    hashmap_segment_t* seg = hm->segments;

    while(seg) {
        hashmap_segment_t* t_seg = seg->next;

        memory_free_ext(heap, seg->items);
        memory_free_ext(heap, seg);

        seg = t_seg;
    }

    lock_destroy(hm->lock);
    memory_free_ext(heap, hm);

    return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
/**
 * @brief get next new segment
 * @param[in] hm hashmap
 * @param[in] seg segment
 * @return next new segment
 */
static hashmap_segment_t* hashmap_segment_next_new(hashmap_t* hm, hashmap_segment_t* seg) {
    while(seg->next) {
        seg = seg->next;
    }

    seg->next = memory_malloc_ext(hm->heap, sizeof(hashmap_segment_t), 0);

    if(!seg->next) {
        return NULL;
    }

    seg->next->items = memory_malloc_ext(hm->heap, sizeof(hashmap_item_t) * hm->segment_capacity, 0);

    if(!seg->next->items) {
        memory_free_ext(hm->heap, seg->next);

        return NULL;
    }

    hm->total_capacity += hm->segment_capacity;

    seg = seg->next;

    return seg;
}
#pragma GCC diagnostic pop

const void* hashmap_put(hashmap_t* hm, const void* key, const void* item) {
    if(!hm) {
        return NULL;
    }

    lock_acquire(hm->lock);

    uint64_t h_key = hm->hkg(key) % hm->segment_capacity;

    hashmap_segment_t* seg = hm->segments;

    while(true) {
        if(seg->items[h_key].exists) {
            if(hm->hkc(key, seg->items[h_key].key) == 0) {
                const void* old_item = seg->items[h_key].value;

                seg->items[h_key].key = key;
                seg->items[h_key].value = item;
                seg->items[h_key].exists = true;

                lock_release(hm->lock);

                return old_item;
            }

            uint64_t t_h_key = (h_key + 1) % hm->segment_capacity;

            if(seg->items[t_h_key].exists) {
                if(hm->hkc(key, seg->items[t_h_key].key) == 0) {
                    const void* old_item = seg->items[t_h_key].value;

                    seg->items[t_h_key].key = key;
                    seg->items[t_h_key].value = item;
                    seg->items[t_h_key].exists = true;

                    lock_release(hm->lock);

                    return old_item;
                }
            } else {
                h_key = t_h_key;

                break;
            }

            if(seg->next) {
                seg = seg->next;
            } else {
                seg = hashmap_segment_next_new(hm, seg);

                if(!seg) {
                    lock_release(hm->lock);

                    return NULL;
                }

                break;
            }

        } else {
            break;
        }
    }

    seg->items[h_key].key = key;
    seg->items[h_key].value = item;
    seg->items[h_key].exists = true;
    seg->size++;
    hm->total_size++;

    lock_release(hm->lock);

    return NULL;
}

const void* hashmap_get_key(hashmap_t* hm, const void* key) {
    if(!hm) {
        return NULL;
    }

    const uint64_t h_key = hm->hkg(key) % hm->segment_capacity;

    hashmap_segment_t* seg = hm->segments;

    while(seg) {
        if(seg->items[h_key].exists) {
            if(hm->hkc(key, seg->items[h_key].key) == 0) {
                return seg->items[h_key].key;
            }

            uint64_t t_h_key = (h_key + 1) % hm->segment_capacity;

            if(seg->items[t_h_key].exists) {
                if(hm->hkc(key, seg->items[t_h_key].key) == 0) {
                    return seg->items[t_h_key].key;
                }
            }
        }

        seg = seg->next;
    }

    return NULL;
}

boolean_t hashmap_exists(hashmap_t* hm, const void* key) {
    if(!hm) {
        return false;
    }

    const uint64_t h_key = hm->hkg(key) % hm->segment_capacity;

    hashmap_segment_t* seg = hm->segments;

    while(seg) {
        if(seg->items[h_key].exists) {
            if(hm->hkc(key, seg->items[h_key].key) == 0) {
                return true;
            }

            uint64_t t_h_key = (h_key + 1) % hm->segment_capacity;

            if(seg->items[t_h_key].exists) {
                if(hm->hkc(key, seg->items[t_h_key].key) == 0) {
                    return true;
                }
            }
        }

        seg = seg->next;
    }

    return false;
}


const void* hashmap_get(hashmap_t* hm, const void* key) {
    if(!hm) {
        return NULL;
    }

    const uint64_t h_key = hm->hkg(key) % hm->segment_capacity;

    hashmap_segment_t* seg = hm->segments;

    while(seg) {
        if(seg->items[h_key].exists) {
            if(hm->hkc(key, seg->items[h_key].key) == 0) {
                return seg->items[h_key].value;
            }

            uint64_t t_h_key = (h_key + 1) % hm->segment_capacity;

            if(seg->items[t_h_key].exists) {
                if(hm->hkc(key, seg->items[t_h_key].key) == 0) {
                    return seg->items[t_h_key].value;
                }
            }
        }

        seg = seg->next;
    }

    return NULL;
}

boolean_t hashmap_delete(hashmap_t* hm, const void* key) {
    if(!hm) {
        return false;
    }

    lock_acquire(hm->lock);

    const uint64_t h_key = hm->hkg(key) % hm->segment_capacity;

    hashmap_segment_t* seg = hm->segments;

    while(seg) {
        if(seg->items[h_key].exists) {
            if(hm->hkc(key, seg->items[h_key].key) == 0) {
                seg->items[h_key].key = NULL;
                seg->items[h_key].value = NULL;
                seg->items[h_key].exists = false;
                seg->size--;
                hm->total_size--;

                lock_release(hm->lock);

                return true;
            }

            uint64_t t_h_key = (h_key + 1) % hm->segment_capacity;

            if(seg->items[t_h_key].exists) {
                if(hm->hkc(key, seg->items[t_h_key].key) == 0) {
                    seg->items[t_h_key].key = NULL;
                    seg->items[t_h_key].value = NULL;
                    seg->items[t_h_key].exists = false;
                    seg->size--;
                    hm->total_size--;

                    lock_release(hm->lock);

                    return true;
                }
            }
        }

        seg = seg->next;
    }

    lock_release(hm->lock);

    return true;
}


uint64_t hashmap_size(hashmap_t* hm) {
    if(!hm) {
        return 0;
    }

    return hm->total_size;
}

/**
 * @struct hashmap_iterator_metadata_t
 * @brief  Metadata for the hashmap iterator
 */
typedef struct hashmap_iterator_metadata_t {
    memory_heap_t*     heap; ///< Heap
    hashmap_segment_t* current_segment; ///< Current segment
    uint64_t           segment_capacity; ///< Segment capacity
    uint64_t           current_index; ///< Current index
} hashmap_iterator_metadata_t; ///< Typedef for hashmap iterator metadata

/**
 * @brief returns current item at iterator
 * @param[in] iter iterator
 * @return current item at iterator
 */
const void* hashmap_iterator_get_item(iterator_t* iter);

/**
 * @brief returns current key at iterator
 * @param[in] iter iterator
 * @return current key at iterator
 */
const void* hashmap_iterator_get_extra_data(iterator_t* iter);

/**
 * @brief advances iterator to next item
 * @param[in] iter iterator
 * @return iterator itself
 */
iterator_t* hashmap_iterator_next(iterator_t* iter);

/**
 * @brief destroys iterator
 * @param[in] iter iterator
 * @return 0 on success, -1 on failure
 */
int8_t hashmap_iterator_destroy(iterator_t* iter);

/**
 * @brief checks if iterator is at end
 * @param[in] iter iterator
 * @return 0 if iterator is at end, 1 otherwise
 */
int8_t hashmap_iterator_end_of_iterator(iterator_t* iter);

const void* hashmap_iterator_get_item(iterator_t* iter) {
    if(!iter) {
        return NULL;
    }

    hashmap_iterator_metadata_t* iter_md = iter->metadata;

    return iter_md->current_segment->items[iter_md->current_index].value;
}

const void* hashmap_iterator_get_extra_data(iterator_t* iter) {
    if(!iter) {
        return NULL;
    }

    hashmap_iterator_metadata_t* iter_md = iter->metadata;

    return iter_md->current_segment->items[iter_md->current_index].key;
}

iterator_t* hashmap_iterator_next(iterator_t* iter) {
    if(!iter) {
        return NULL;
    }

    hashmap_iterator_metadata_t* iter_md = iter->metadata;

    if(!iter_md->current_segment) {
        return iter;
    }

    while(true) {
        iter_md->current_index++;

        if(iter_md->current_index == iter_md->segment_capacity) {
            iter_md->current_index = 0;
            iter_md->current_segment = iter_md->current_segment->next;
        }

        if(!iter_md->current_segment) {
            break;
        }

        if(iter_md->current_segment->items[iter_md->current_index].exists) {
            break;
        }

    }

    return iter;
}

int8_t hashmap_iterator_destroy(iterator_t* iter) {
    if(!iter) {
        return -1;
    }

    memory_heap_t* heap = ((hashmap_iterator_metadata_t*)iter->metadata)->heap;

    memory_free_ext(heap, iter->metadata);
    memory_free_ext(heap, iter);

    return 0;
}

int8_t hashmap_iterator_end_of_iterator(iterator_t* iter) {
    if(!iter) {
        return NULL;
    }

    hashmap_iterator_metadata_t* iter_md = iter->metadata;

    return iter_md->current_segment == NULL?0:1;
}

iterator_t* hashmap_iterator_create(hashmap_t* hm) {
    if(!hm) {
        return NULL;
    }

    hashmap_iterator_metadata_t* iter_md = memory_malloc_ext(hm->heap, sizeof(hashmap_iterator_metadata_t), 0);

    if(!iter_md) {
        return NULL;
    }

    iter_md->heap = hm->heap;
    iter_md->current_segment = hm->segments;
    iter_md->segment_capacity = hm->segment_capacity;

    iterator_t* iter = memory_malloc_ext(hm->heap, sizeof(iterator_t), 0);

    if(!iter) {
        memory_free_ext(hm->heap, iter_md);

        return NULL;
    }

    iter->metadata = iter_md;
    iter->get_item = hashmap_iterator_get_item;
    iter->end_of_iterator = hashmap_iterator_end_of_iterator;
    iter->destroy = hashmap_iterator_destroy;
    iter->get_extra_data = hashmap_iterator_get_extra_data;
    iter->next = hashmap_iterator_next;

    if(hm->total_size == 0) {
        iter_md->current_segment = NULL;
    } else {
        while(iter_md->current_segment) {
            if(iter_md->current_segment->items[iter_md->current_index].exists) {
                break;
            }

            iter_md->current_index++;

            if(iter_md->current_index == iter_md->segment_capacity) {
                iter_md->current_segment = iter_md->current_segment->next;
                iter_md->current_segment = iter_md->current_segment->next;
            }
        }
    }

    return iter;
}
