Start Qemu on OSX {#qemuonosx}
=========================

### Setup networking

tap kext needs Tunnelblick should be installed

for creating use these commands:

```
sudo ifconfig bridge0 create
sudo ifconfig bridge0 192.168.122.1 192.168.122.255
sudo kextload  /Applications/Tunnelblick.app/Contents//Resources/tap-notarized.kext
```

for destroying use these commands:

```
sudo ifconfig bridge0 down
sudo ifconfig bridge0 destroy
sudo kextunload  /Applications/Tunnelblick.app/Contents//Resources/tap-notarized.kext
```


### Start/Stop dnsmasq

For start stop dnsmasq (dhcp+tftp):

```
sudo port load dnsmasq
sudo port unload dnsmasq
```

**dnsmasq conf** is at ```/opt/local/etc/dnsmasq.conf``` and content is:

```
interface=bridge0
enable-tftp
dhcp-range=192.168.122.100,192.168.122.200
tftp-root=/opt/local/var/tftpboot
#dhcp-boot=pxelinux.0
dhcp-match=set:ipxe,175
dhcp-boot=tag:!ipxe,undionly.kpxe
dhcp-boot=tftp://192.168.122.1/pxelinux.cfg/ipxe.cfg
```

inside **tftp-root**:

```
/opt/local/var/tftpboot
├── osdev-kernel
├── pxelinux.cfg
│   └── ipxe.cfg
└── undionly.kpxe

1 directory, 3 files
```

HobbyOS supports only undionly pxe boot. **ipxe.cfg** content is:

```
#!ipxe

kernel tftp://192.168.122.1/osdev-kernel
boot
```

### Start qemu

sudo qemu-system-x86_64 -net nic,model=virtio,macaddr=54:54:00:55:55:55 -net tap,script=tap-up,downscript=tap-down -M q35 -m 1g -monitor stdio

**tap-up script**
```
#!/bin/sh
TAPDEV="$1"
BRIDGEDEV="bridge0"
ifconfig $BRIDGEDEV addm $TAPDEV
```
**tap-down script**
```
#!/bin/sh
TAPDEV="$1"
BRIDGEDEV="bridge0"
ifconfig $BRIDGEDEV deletem $TAPDEV
```
