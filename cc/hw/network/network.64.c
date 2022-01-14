#include <driver/network.h>
#include <driver/network_virtio.h>
#include <pci.h>
#include <linkedlist.h>
#include <memory.h>
#include <video.h>


int8_t network_init() {
	PRINTLOG(NETWORK, LOG_INFO, "network devices starting", 0);
	int8_t errors = 0;

	linkedlist_t netdevs = PCI_CONTEXT->network_controllers;

	iterator_t* iter = linkedlist_iterator_create(netdevs);

	while(iter->end_of_iterator(iter) != 0) {
		pci_dev_t* netdev = iter->get_item(iter);

		pci_common_header_t* pci_netdev = netdev->pci_header;



		if(pci_netdev->vendor_id == NETWORK_DEVICE_VENDOR_ID_VIRTIO && (pci_netdev->device_id == NETWORK_DEVICE_DEVICE_ID_VIRTNET1 || pci_netdev->device_id == NETWORK_DEVICE_DEVICE_ID_VIRTNET2)) {
			errors += network_virtio_init(netdev);
		} else {
			PRINTLOG(NETWORK, LOG_ERROR, "unknown net device vendor 0x%04x device 0x%04x", pci_netdev->vendor_id, pci_netdev->device_id);
			errors += -1;
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);


	PRINTLOG(NETWORK, LOG_INFO, "network devices started", 0);

	return errors;
}
