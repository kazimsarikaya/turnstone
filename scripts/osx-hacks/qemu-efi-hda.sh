#!/bin/sh

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"

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
  -M q35 -m 1g -smp cpus=2 -name osdev-hda-boot \
  -drive if=pflash,readonly=off,format=raw,unit=0,file=${CURRENTDIR}/edk2-x86_64-code.fd \
  -drive if=pflash,readonly=off,format=raw,unit=1,file=${CURRENTDIR}/edk2-i386-vars.fd \
  -drive index=0,media=disk,format=raw,file=${OUTPUTDIR}/qemu-hda,werror=report,rerror=report \
  -monitor stdio \
  -net none \
  -serial file:${BASEDIR}/tmp/qemu-video.log