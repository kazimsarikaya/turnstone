#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"
CDROM=/Users/kazim/Downloads/alpine-extended-3.14.1-x86_64.iso

UEFIBIOSCODE=""
UEFIBIOSVARS=""

if [ `uname -s` == "Linux" ]; then
  UEFIBIOSCODE="/usr/share/edk2/ovmf/OVMF_CODE.fd"
  UEFIBIOSVARS="/usr/share/edk2/ovmf/OVMF_VARS.fd"
fi

if [ `uname -s` == "Darwin" ]; then
  UEFIBIOSCODE="/opt/local/share/qemu/edk2-x86_64-code.fd"
  UEFIBIOSVARS="/opt/local/share/qemu/edk2-i386-vars.fd"
fi

if [ ! -f $CURRENTDIR/edk2-x86_64-code.fd ]; then
  cp $UEFIBIOSCODE $CURRENTDIR/edk2-x86_64-code.fd
fi

if [ ! -f $CURRENTDIR/edk2-i386-vars.fd ]; then
  cp $UEFIBIOSVARS $CURRENTDIR/edk2-i386-vars.fd
fi

qemu-system-x86_64 \
  -M q35 -m 1g -smp cpus=2 -name osdev-efi-cdrom-net \
  -drive if=pflash,readonly=off,format=raw,unit=0,file=${CURRENTDIR}/edk2-x86_64-code.fd \
  -drive if=pflash,readonly=off,format=raw,unit=1,file=${CURRENTDIR}/edk2-i386-vars.fd \
  -drive index=0,media=cdrom,format=raw,file=${CDROM},werror=report,rerror=report \
  -monitor stdio \
  -device virtio-net,netdev=t0,id=nic0 \
  -netdev socket,id=t0,udp=127.0.0.1:16384,localaddr=127.0.0.1:16385  \
  -serial file:${BASEDIR}/tmp/qemu-video.log \
  -debugcon file:${BASEDIR}/tmp/qemu-acpi-debug.log -global isa-debugcon.iobase=0x402
