#!/bin/sh
CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"

dd if=/dev/zero of=${OUTPUTDIR}/qemu-hda bs=1 count=0 seek=1g
dd if=${OUTPUTDIR}/kernel of=${OUTPUTDIR}/qemu-hda conv=notrunc

sudo qemu-system-x86_64 \
  -M q35 -m 1g -smp cpus=2 -name osdev-hda-boot \
  -drive index=0,media=disk,format=raw,file=${OUTPUTDIR}/qemu-hda \
  -net nic,model=virtio,macaddr=54:54:00:55:55:55 \
  -net tap,script=${CURRENTDIR}/tap-up.sh,downscript=${CURRENTDIR}/tap-down.sh  \
  -monitor stdio \
  -serial file:${BASEDIR}/tmp/qemu-video.log
