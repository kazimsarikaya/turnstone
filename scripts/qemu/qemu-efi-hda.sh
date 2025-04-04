#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/build"

ACCEL="kvm"
UEFIBIOSCODESRC="/usr/share/OVMF/OVMF_CODE.fd"
UEFIBIOSVARSSRC="/usr/share/OVMF/OVMF_VARS.fd"
NETDEV="bridge,id=t0"

if [ ! -c /dev/kvm ]; then
    echo "KVM is not available. Please load the kvm module."
    exit 1
fi

DEBUG=$1
PREVENTSHUTDOWN=""

if [[ "${DEBUG}x" == "debugx" ]]; then
  ACCEL="${ACCEL} -s -S"
  PREVENTSHUTDOWN="--no-shutdown --no-reboot"
fi

if [[ "${DEBUG}x" == "preventx" ]]; then
  PREVENTSHUTDOWN="--no-shutdown --no-reboot"
fi

if [[ "${DEBUG}x" == "pausedx" ]]; then
  ACCEL="${ACCEL} -S"
fi

if [ ! -f $CURRENTDIR/edk2-x86_64-code.fd ]; then
  cp $UEFIBIOSCODESRC $OUTPUTDIR/edk2-x86_64-code.fd
fi

if [ ! -f $CURRENTDIR/edk2-i386-vars.fd ]; then
  cp $UEFIBIOSVARSSRC $OUTPUTDIR/edk2-i386-vars.fd
fi

if [ ! -f ${OUTPUTDIR}/qemu-nvme-cache ]; then
  dd if=/dev/zero of=${OUTPUTDIR}/qemu-nvme-cache bs=1 count=0 seek=$((1024*1024*1024)) >/dev/null 2>&1
fi

NUMCPUS=4
RAMSIZE=8

SERIALS=""

for i in `seq 0 $((NUMCPUS-1))`; do
  SERIALS="${SERIALS} -serial file:${BASEDIR}/tmp/qemu-serial${i}.log"
done

TRACE_OPTS="guest_errors,mmu"

# if trace_opts is not empty, then enable tracing (prefix with -d)

if [[ "${TRACE_OPTS}x" != "x" ]]; then
  TRACE_OPTS="-d ${TRACE_OPTS}"
fi

/home/kazim/.local/qemu/bin/qemu-system-x86_64 \
  -nodefaults -no-user-config $PREVENTSHUTDOWN \
  -M q35,kernel-irqchip=split -m ${RAMSIZE}g -smp cpus=${NUMCPUS} -name osdev-hda-efi-boot \
  -cpu host,topoext=on,x2apic=on \
  -accel $ACCEL ${TRACE_OPTS} \
  -drive if=pflash,readonly=on,format=raw,unit=0,file=${OUTPUTDIR}/edk2-x86_64-code.fd \
  -drive if=pflash,readonly=off,format=raw,unit=1,file=${OUTPUTDIR}/edk2-i386-vars.fd \
  -drive id=system,if=none,format=raw,file=${OUTPUTDIR}/qemu-hda,werror=report,rerror=report \
  -device ide-hd,drive=system,bootindex=1 \
  -drive id=cache,if=none,format=raw,file=${OUTPUTDIR}/qemu-nvme-cache,werror=report,rerror=report \
  -device nvme,drive=cache,serial=qn0001,id=nvme0,logical_block_size=4096,physical_block_size=4096 \
  -monitor stdio \
  -device VGA,id=gpu0,vgamem_mb=256 \
  -device igb,netdev=t0,id=nic0 \
  -netdev $NETDEV \
  -device usb-ehci,id=ehci \
  -device usb-tablet,bus=ehci.0 \
  -device usb-kbd,bus=ehci.0 \
  -device edu,id=edu,dma_mask=0xFFFFFFFFFFFFFFFF \
  -device amd-iommu,id=amdiommu,device-iotlb=on,intremap=on,xtsup=on,pt=on \
  $SERIALS \
  -debugcon file:${BASEDIR}/tmp/qemu-acpi-debug.log -global isa-debugcon.iobase=0x402 \
  -display sdl,gl=on,show-cursor=off
