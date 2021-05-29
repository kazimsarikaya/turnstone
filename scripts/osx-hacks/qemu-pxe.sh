#!/bin/sh
CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"
TFTPROOT="/opt/local/var/tftpboot"

sudo cp "${OUTPUTDIR}/kernel" "{TFTPROOT}/osdev-kernel"

sudo qemu-system-x86_64 \
  -M q35 -m 1g -smp cpus=2 -name osdev-pxe-boot \
  -net nic,model=virtio,macaddr=54:54:00:55:55:55 \
  -net tap,script=${CURRENTDIR}/tap-up.sh,downscript=${CURRENTDIR}/tap-down.sh  \
  -monitor stdio
