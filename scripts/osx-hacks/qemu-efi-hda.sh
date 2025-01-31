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

DEBUG=$1
PREVENTSHUTDOWN=""

if [[ "${DEBUG}x" == "debugx" ]]; then
  ACCEL="tcg -s -S"
  PREVENTSHUTDOWN="--no-shutdown --no-reboot"
fi

if [[ "${DEBUG}x" == "pausedx" ]]; then
  ACCEL="${ACCEL} -S"
fi

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

NUMCPUS=4 
RAMSIZE=4

SERIALS=""

for i in `seq 0 $((NUMCPUS-1))`; do
  SERIALS="${SERIALS} -serial file:${BASEDIR}/tmp/qemu-serial${i}.log"
done

VIRTIO_SERIAL_ENABLED="yes"
VIRTIO_SERIAL=""

if [[ "${VIRTIO_SERIAL_ENABLED}x" == "yesx" ]]; then
    VIRTIO_SERIAL="${VIRTIO_SERIAL} -device virtio-serial,id=vser0,packed=on,ioeventfd=on,max_ports=4,vectors=64"
    VIRTIO_SERIAL="${VIRTIO_SERIAL} -device virtserialport,name=com.turnstoneos.clipboard.0,chardev=vdagent0"
    VIRTIO_SERIAL="${VIRTIO_SERIAL} -chardev socket,id=vdagent0,port=4444,host=localhost,server=off,reconnect=5"
fi

TRACE_OPTS="guest_errors,trace:virtio_console_*"

qemu-system-x86_64 \
  -nodefaults -no-user-config $PREVENTSHUTDOWN \
  -M q35 -m ${RAMSIZE}g -smp cpus=4 -name osdev-hda-efi-boot \
  -cpu max \
  -accel $ACCEL -d "${TRACE_OPTS}" \
  -drive if=pflash,readonly=on,format=raw,unit=0,file=${CURRENTDIR}/edk2-x86_64-code.fd \
  -drive if=pflash,readonly=off,format=raw,unit=1,file=${CURRENTDIR}/edk2-i386-vars.fd \
  -drive id=system,if=none,format=raw,file=${OUTPUTDIR}/qemu-hda,werror=report,rerror=report \
  -device ide-hd,drive=system,bootindex=1 \
  -drive id=cache,if=none,format=raw,file=${OUTPUTDIR}/qemu-nvme-cache,werror=report,rerror=report \
  -device nvme,drive=cache,serial=qn0001,id=nvme0,logical_block_size=4096,physical_block_size=4096 \
  -monitor stdio \
  -device virtio-vga-gl,id=gpu0 \
  -device virtio-net,netdev=t0,id=nic0,host_mtu=1500 \
  -netdev $NETDEV \
  -device virtio-keyboard,id=kbd \
  -device virtio-mouse,id=mouse \
  -device virtio-tablet,id=tablet \
  -device edu,id=edu,dma_mask=0xFFFFFFFFFFFFFFFF \
  $VIRTIO_SERIAL \
  $SERIALS \
  -debugcon file:${BASEDIR}/tmp/qemu-acpi-debug.log -global isa-debugcon.iobase=0x402 \
  -display sdl,gl=on,show-cursor=on 
