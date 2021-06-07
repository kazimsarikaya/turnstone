#!/bin/sh
CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"

sudo qemu-system-x86_64 \
  -M q35 -m 1g -smp cpus=2 -name osdev-hda-boot \
  -drive index=0,media=disk,format=raw,file=${OUTPUTDIR}/qemu-hda \
  -net nic,model=virtio,macaddr=54:54:00:55:55:55 \
  -net tap,script=${CURRENTDIR}/tap-up.sh,downscript=${CURRENTDIR}/tap-down.sh  \
  -monitor stdio \
  -serial file:${BASEDIR}/tmp/qemu-video.log
