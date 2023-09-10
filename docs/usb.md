# USB Devices

In this os we support two controller types [EHCI] and [XHCI].

Usb devices are interesting. Each controller has periodic and async transfer types. And handling of them and memory structures are different. Hence we need to encapsulate them. We detect version of each controller by pci progif field. Ehci has **20** and xhci has **30**. Then we create a controller structure for each device. Then init device. 

Each controller can have hubs and devices. We need to enumerate them. Also we need to init :). 

For each device it should have a driver. like keyboard driver or mass storage driver. Each driver should send related transfer to the controller. Hence **controller**, **device** and **driver** structures should be linked. Also controler struct should have port probe method. This method can be runned by interrupts. But don't forget that if an interrupt takes time, cpu will be blocked. Hence create a cpu task for port probing.






[EHCI]: https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf
[XHCI]: https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf
