/**
 * @file network_info.64.c
 * @brief Network info implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <network/network_info.h>
#include <network/network_utils.h>
#include <utils.h>
#include <memory.h>
#include <logging.h>

MODULE("turnstone.lib.network");

extern memory_heap_t* network_packet_heap;
map_t* network_info_map = NULL;

static uint64_t network_info_mke(const void* key) {
    uint64_t x = 0;
    memory_memcopy(key, &x, sizeof(network_mac_address_t));

    return x;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t network_register_network_info(network_mac_address_t mac, uint16_t mtu, list_t* return_queue,
                                     boolean_t has_hw_vlan_support, boolean_t is_vlan_tagged, uint16_t vlan_id) {
    if(!mac) {
        PRINTLOG(NETWORK, LOG_ERROR, "mac address is null");
        return -1;
    }

    if(!return_queue) {
        PRINTLOG(NETWORK, LOG_ERROR, "return queue is null");
        return -1;
    }

    if(!network_info_map) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info map is not initialized");
        return -1;
    }

    if(map_get(network_info_map, mac)) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info already exists for mac %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2],
                 mac[3], mac[4], mac[5]);
        return -1;
    }

    memory_heap_t* heap = map_get_heap(network_info_map);

    network_info_t* new_ni = memory_malloc_ext(heap, sizeof(network_info_t), 0);

    if(!new_ni) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to allocate network info for mac %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2],
                 mac[3], mac[4], mac[5]);
        return -1;
    }

    memory_memcopy(mac, new_ni->mac, sizeof(network_mac_address_t));
    new_ni->mtu                 = mtu;
    new_ni->return_queue        = return_queue;
    new_ni->has_hw_vlan_support = has_hw_vlan_support;
    new_ni->is_vlan_tagged      = is_vlan_tagged;
    new_ni->vlan_id             = vlan_id;

    map_insert(network_info_map, new_ni->mac, new_ni);

    PRINTLOG(NETWORK, LOG_INFO, "network info registered for mac %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    return 0;
}
#pragma GCC diagnostic pop

int8_t network_unregister_network_info(network_info_t* ni) {
    if(!ni) {
        return -1;
    }

    if(!network_info_map) {
        return -1;
    }

    network_info_t* existing_ni = (network_info_t*)map_get(network_info_map, ni->mac);

    if(!existing_ni) {
        return -1;
    }

    map_delete(network_info_map, ni->mac);

    memory_heap_t* heap = map_get_heap(network_info_map);

    memory_free_ext(heap, existing_ni);

    PRINTLOG(NETWORK, LOG_INFO, "network info unregistered for mac %02x:%02x:%02x:%02x:%02x:%02x",
             ni->mac[0], ni->mac[1], ni->mac[2],
             ni->mac[3], ni->mac[4], ni->mac[5]);

    return 0;
}

network_info_t* network_get_network_info(const network_mac_address_t mac) {
    if(!mac) {
        PRINTLOG(NETWORK, LOG_ERROR, "mac address is null");
        return NULL;
    }

    if(!network_info_map) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info map is not initialized");
        return NULL;
    }

    return (network_info_t*)map_get(network_info_map, mac);
}

network_info_t* network_get_network_info_of_owned_ipv4_address(network_ipv4_address_t ipv4_address) {
    if(!network_info_map) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info map is not initialized");
        return NULL;
    }

    iterator_t* it = map_create_iterator(network_info_map);
    if(it) {
        while(!it->end_of_iterator(it)) {
            network_info_t* ni = (network_info_t*)it->get_item(it);
            if(ni) {
                if(network_ipv4_is_address_eq(ni->ipv4_address, ipv4_address)) {
                    it->destroy(it);
                    return ni;
                }
            }
            it = it->next(it);
        }
        it->destroy(it);
    }

    return NULL;
}

network_info_t* network_get_network_info_by_ipv4_address(network_ipv4_address_t ipv4_address) {
    if(!network_info_map) {
        PRINTLOG(NETWORK, LOG_ERROR, "network info map is not initialized");
        return NULL;
    }

    network_info_t* last_network_info = NULL;

    iterator_t* it = map_create_iterator(network_info_map);
    if(it) {
        while(!it->end_of_iterator(it)) {
            network_info_t* ni = (network_info_t*)it->get_item(it);
            if(ni) {
                last_network_info = ni;
                if(network_ipv4_is_address_in_same_subnet(ipv4_address, ni->ipv4_address, ni->ipv4_subnetmask)) {
                    it->destroy(it);
                    return ni;
                }
            }
            it = it->next(it);
        }
        it->destroy(it);
    }

    // currently, we return the last network info as default gateway's network info,
    // but we may want to change this behavior in the future.
    return last_network_info;
}

int8_t network_info_init(void) {
    if(!network_info_map) {
        network_info_map = map_new_with_heap(network_packet_heap, &network_info_mke);
    }

    if(!network_info_map) {
        PRINTLOG(NETWORK, LOG_ERROR, "failed to create network info map");
        return -1;
    }

    return 0;
}
