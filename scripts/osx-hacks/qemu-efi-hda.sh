#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"

UEFIBIOSCODE=""
UEFIBIOSVARS=""
ACCEL="$([[ -c /dev/kvm ]] && echo kvm || echo tcg)"
NETDEV=""

if [ `uname -s` == "Linux" ]; then
  UEFIBIOSCODE="/usr/share/OVMF/OVMF_CODE.fd"  
  UEFIBIOSVARS="/usr/share/OVMF/OVMF_VARS.fd"
  NETDEV="bridge,id=t0"
  #NETDEV="tap,id=t0,ifname=tap0,script=no,downscript=no"
fi

if [ `uname -s` == "Darwin" ]; then
  UEFIBIOSCODE="/opt/local/share/qemu/edk2-x86_64-code.fd"
  UEFIBIOSVARS="/opt/local/share/qemu/edk2-i386-vars.fd"
  NETDEV="socket,id=t0,udp=127.0.0.1:16384,localaddr=127.0.0.1:16385"
fi

if [ ! -f $CURRENTDIR/edk2-x86_64-code.fd ]; then
  cp $UEFIBIOSCODE $CURRENTDIR/edk2-x86_64-code.fd
fi

if [ ! -f $CURRENTDIR/edk2-i386-vars.fd ]; then
  cp $UEFIBIOSVARS $CURRENTDIR/edk2-i386-vars.fd
fi

if [ ! -f ${OUTPUTDIR}/qemu-nvme-cache ]; then
  dd if=/dev/zero of=${OUTPUTDIR}/qemu-nvme-cache bs=1 count=0 seek=$((1024*1024*1024)) >/dev/null 2>&1
fi

qemu-system-x86_64 \
  -nodefaults -no-user-config \
  -M q35 -m 1g -smp cpus=2 -name osdev-hda-boot \
  -cpu max \
  -accel $ACCEL \
  -drive if=pflash,readonly=on,format=raw,unit=0,file=${CURRENTDIR}/edk2-x86_64-code.fd \
  -drive if=pflash,readonly=off,format=raw,unit=1,file=${CURRENTDIR}/edk2-i386-vars.fd \
  -drive index=0,media=disk,format=raw,file=${OUTPUTDIR}/qemu-hda,werror=report,rerror=report \
  -drive id=cache,if=none,format=raw,file=${OUTPUTDIR}/qemu-nvme-cache,werror=report,rerror=report \
  -device nvme,drive=cache,serial=qn0001,id=nvme0 \
  -monitor stdio \
  -device vmware-svga,id=gpu0 \
  -device virtio-net,netdev=t0,id=nic0,host_mtu=1500 \
  -netdev $NETDEV \
  -device virtio-keyboard,id=kbd \
  -serial file:${BASEDIR}/tmp/qemu-video.log \
  -debugcon file:${BASEDIR}/tmp/qemu-acpi-debug.log -global isa-debugcon.iobase=0x402 \
  -display gtk
