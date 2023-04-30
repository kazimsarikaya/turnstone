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

MODULE("turnstone.lib");

typedef struct hashmap_item_t {
    boolean_t   exists;
    const void* key;
    const void* value;
} hashmap_item_t;

typedef struct hashmap_segment_t {
    uint64_t                  size;
    struct hashmap_segment_t* next;
    hashmap_item_t*           items;
} hashmap_segment_t;

struct hashmap_t {
    uint64_t                 segment_capacity;
    uint64_t                 total_capacity;
    uint64_t                 total_size;
    hashmap_key_generator_f  hkg;
    hashmap_key_comparator_f hkc;
    hashmap_segment_t*       segments;
    lock_t                   lock;
};

uint64_t hashmap_default_kg(const void* key);
int8_t   hashmap_default_kc(const void* item1, const void* item2);
uint64_t hashmap_string_kg(const void * key);
int8_t   hashmap_string_kc(const void* item1, const void* item2);

uint64_t hashmap_default_kg(const void* key) {
    return (uint64_t)key;
}

uint64_t hashmap_string_kg(const void * key) {
    char_t* str_key = (char_t*)key;

    return xxhash32_hash(str_key, strlen(str_key));
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

hashmap_t*  hashmap_new_with_hkg_with_hkc(uint64_t capacity, hashmap_key_generator_f hkg, hashmap_key_comparator_f hkc) {
    if(!capacity) {
        return NULL;
    }

    hashmap_t* hm = memory_malloc(sizeof(hashmap_t));

    if(!hm) {
        return NULL;
    }

    hm->lock = lock_create();

    hm->total_capacity = capacity;
    hm->segment_capacity = capacity;

    hm->hkg = hkg?hkg:hashmap_default_kg;
    hm->hkc = hkc?hkc:hashmap_default_kc;

    hm->segments = memory_malloc(sizeof(hashmap_segment_t));

    if(!hm->segments) {
        memory_free(hm);

        return NULL;
    }

    hm->segments->items = memory_malloc(sizeof(hashmap_item_t) * hm->segment_capacity);

    if(!hm->segments->items) {
        memory_free(hm->segments);
        memory_free(hm);

        return NULL;
    }

    return hm;
}

hashmap_t* hashmap_string(uint64_t capacity) {
    return hashmap_new_with_hkg_with_hkc(capacity, hashmap_string_kg, hashmap_string_kc);
}

boolean_t   hashmap_destroy(hashmap_t* hm) {
    if(!hm) {
        return false;
    }

    hashmap_segment_t* seg = hm->segments;

    while(seg) {
        hashmap_segment_t* t_seg = seg->next;

        memory_free(seg->items);
        memory_free(seg);

        seg = t_seg;
    }

    lock_destroy(hm->lock);
    memory_free(hm);

    return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static hashmap_segment_t* hashmap_segment_next_new(hashmap_t* hm, hashmap_segment_t* seg) {
    seg->next = memory_malloc(sizeof(hashmap_segment_t));

    if(!seg->next) {
        return NULL;
    }

    seg->next->items = memory_malloc(sizeof(hashmap_item_t) * hm->segment_capacity);

    if(!seg->next->items) {
        memory_free(seg->next);

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

    if(seg->items[h_key].exists) {
        if(hm->hkc(key, seg->items[h_key].key) == 0) {
            const void* old_item = seg->items[h_key].value;

            seg->items[h_key].key = key;
            seg->items[h_key].value = item;
            seg->items[h_key].exists = true;

            lock_release(hm->lock);

            return old_item;
        }

        if(seg->size == hm->segment_capacity) {
            seg = hashmap_segment_next_new(hm, seg);

            if(!seg) {
                lock_release(hm->lock);

                return NULL;
            }
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

            seg = hashmap_segment_next_new(hm, seg);

            if(!seg) {
                lock_release(hm->lock);

                return NULL;
            }
        } else {
            h_key = t_h_key;
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
                    seg->items[h_key].exists = false;
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

typedef struct hashmap_iterator_metadata_t {
    hashmap_segment_t* current_segment;
    uint64_t           segment_capacity;
    uint64_t           current_index;
    boolean_t          started;
} hashmap_iterator_metadata_t;

const void* hashmap_iterator_get_item(iterator_t* iter);
const void* hashmap_iterator_get_extra_data(iterator_t* iter);
iterator_t* hashmap_iterator_next(iterator_t* iter);
int8_t      hashmap_iterator_destroy(iterator_t* iter);
int8_t      hashmap_iterator_end_of_iterator(iterator_t* iter);

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
        if(iter_md->started) {
            iter_md->current_index++;
        }else {
            iter_md->started = true;
        }

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
    memory_free(iter->metadata);
    memory_free(iter);

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

    hashmap_iterator_metadata_t* iter_md = memory_malloc(sizeof(hashmap_iterator_metadata_t));

    if(!iter_md) {
        return NULL;
    }

    iter_md->current_segment = hm->segments;
    iter_md->segment_capacity = hm->segment_capacity;

    iterator_t* iter = memory_malloc(sizeof(iterator_t));

    if(!iter) {
        memory_free(iter_md);

        return NULL;
    }

    iter->metadata = iter_md;
    iter->get_item = hashmap_iterator_get_item;
    iter->end_of_iterator = hashmap_iterator_end_of_iterator;
    iter->destroy = hashmap_iterator_destroy;
    iter->get_extra_data = hashmap_iterator_get_extra_data;
    iter->next = hashmap_iterator_next;

    return iter->next(iter);
}
