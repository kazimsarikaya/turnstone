/**
 * @file hashmap.64.c
 * @brief hashmap interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hashmap.h>
#include <memory.h>

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
    uint64_t               segment_capacity;
    uint64_t               total_capacity;
    uint64_t               total_size;
    hashmap_key_generator  hkg;
    hashmap_key_comparator hkc;
    hashmap_segment_t*     segments;
};

uint64_t hashmap_default_kg(const void* key);
int8_t   hashmap_default_kc(const void* item1, const void* item2);

uint64_t hashmap_default_kg(const void* key) {
    return (uint64_t)key;
}

int8_t   hashmap_default_kc(const void* item1, const void* item2) {
    uint64_t ti1 = (uint64_t)item1;
    uint64_t ti2 = (uint64_t)item2;

    return ti1 - ti2;
}

hashmap_t*  hashmap_new_with_hkg_with_hkc(uint64_t capacity, hashmap_key_generator hkg, hashmap_key_comparator hkc) {
    if(!capacity) {
        return NULL;
    }

    hashmap_t* hm = memory_malloc(sizeof(hashmap_t));

    if(!hm) {
        return NULL;
    }

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

    uint64_t h_key = hm->hkg(key) % hm->segment_capacity;

    hashmap_segment_t* seg = hm->segments;

    if(seg->items[h_key].exists) {
        if(hm->hkc(key, seg->items[h_key].key) == 0) {
            const void* old_item = seg->items[h_key].value;
            seg->items[h_key].key = key;
            seg->items[h_key].value = item;
            seg->items[h_key].exists = true;

            return old_item;
        }

        if(seg->size == hm->segment_capacity) {
            seg = hashmap_segment_next_new(hm, seg);

            if(!seg) {
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

                return old_item;
            }

            seg = hashmap_segment_next_new(hm, seg);

            if(!seg) {
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

    return NULL;
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
        if(seg->items[h_key].key) {
            if(hm->hkc(key, seg->items[h_key].key) == 0) {
                seg->items[h_key].key = NULL;
                seg->items[h_key].value = NULL;
                seg->items[h_key].exists = false;
                seg->size--;
                hm->total_size--;

                return true;
            }

            uint64_t t_h_key = (h_key + 1) % hm->segment_capacity;

            if(seg->items[t_h_key].key) {
                if(hm->hkc(key, seg->items[t_h_key].key) == 0) {
                    seg->items[t_h_key].key = NULL;
                    seg->items[t_h_key].value = NULL;
                    seg->items[h_key].exists = false;
                    seg->size--;
                    hm->total_size--;

                    return true;
                }
            }
        }

        seg = seg->next;
    }

    return true;
}

