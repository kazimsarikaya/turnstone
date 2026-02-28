#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
BASEDIR=`readlink -f ${BASEDIR}`
OUTPUTDIR="${BASEDIR}/build"
TPM_ASSETS_DIR="${BASEDIR}/assets-gen/turnstone/kernel/hw/tpm"
HTTP_ASSETS_DIR="${BASEDIR}/assets-gen/turnstone/kernel/network/http"

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

if [ ! -f $OUTPUTDIR/edk2-x86_64-code.fd ]; then
  cp $UEFIBIOSCODESRC $OUTPUTDIR/edk2-x86_64-code.fd
fi

if [ ! -f $OUTPUTDIR/edk2-i386-vars.fd ]; then
  cp $UEFIBIOSVARSSRC $OUTPUTDIR/edk2-i386-vars.fd
fi

if [ ! -f ${OUTPUTDIR}/qemu-nvme-cache ]; then
  dd if=/dev/zero of=${OUTPUTDIR}/qemu-nvme-cache bs=1 count=0 seek=$((1024*1024*1024)) >/dev/null 2>&1
fi

if [ ! -f ${OUTPUTDIR}/qemu-usb-bot ]; then
  dd if=/dev/zero of=${OUTPUTDIR}/qemu-usb-bot bs=1 count=0 seek=$((1024*1024*1024)) >/dev/null 2>&1
fi

if [ ! -f ${OUTPUTDIR}/qemu-usb-uas ]; then
  dd if=/dev/zero of=${OUTPUTDIR}/qemu-usb-uas bs=1 count=0 seek=$((1024*1024*1024)) >/dev/null 2>&1
fi

# check swtpm is installed
if ! command -v swtpm &> /dev/null; then
    echo "swtpm could not be found. Please install swtpm to use TPM features."
    exit 1
fi

# check tpm2_startup is installed
if ! command -v tpm2_startup &> /dev/null; then
    echo "tpm2_startup could not be found. Please install tpm2-tools to use TPM features."
    exit 1
fi

TPM_DIR="${OUTPUTDIR}/tpm"

kill -9 $(cat ${TPM_DIR}/swtpm.pid 2>/dev/null) 2>/dev/null || true
rm -f ${TPM_DIR}/swtpm.pid ${TPM_DIR}/swtpm ${TPM_DIR}/swtpm.ctrl
truncate -s 0 ${TPM_DIR}/swtpm.log || true

export TPM2TOOLS_TCTI="swtpm:path=${TPM_DIR}/swtpm"

if [ ! -d ${TPM_DIR} ]; then
  mkdir -p ${TPM_DIR}/config ${TPM_DIR}/swtpm-localca ${TPM_DIR}/swtpm-state ${TPM_ASSETS_DIR} ${HTTP_ASSETS_DIR}
  cat > ${TPM_DIR}/config/swtpm_setup.conf <<EOF
# Program invoked for creating certificates
create_certs_tool= /usr/bin/swtpm_localca
create_certs_tool_config = ${TPM_DIR}/config/swtpm-localca.conf
create_certs_tool_options = ${TPM_DIR}/config/swtpm-localca.options
# Comma-separated list (no spaces) of PCR banks to activate by default
active_pcr_banks = sha384
rsa_keysize = 2048
profile = {"Name": "default-v1"}
# profile_file =
local_profiles_dir = /etc/swtpm/profiles/
EOF
  cat > ${TPM_DIR}/config/swtpm-localca.conf <<EOF
statedir = ${TPM_DIR}/swtpm-localca
signingkey = ${TPM_DIR}/swtpm-localca/signkey.pem
issuercert = ${TPM_DIR}/swtpm-localca/issuercert.pem
certserial = ${TPM_DIR}/swtpm-localca/certserial
EOF
  cat > ${TPM_DIR}/config/swtpm-localca.options <<EOF
--platform-manufacturer Fedora
--platform-version 2.1
--platform-model QEMU
EOF

  swtpm_setup --tpm2 \
      --tpm-state ${TPM_DIR}/swtpm-state  \
      --ecc \
      --create-ek-cert \
      --create-platform-cert \
      --lock-nvram \
      --config ${TPM_DIR}/config/swtpm_setup.conf

  swtpm socket \
      --tpmstate dir=${TPM_DIR}/swtpm-state \
      --ctrl type=unixio,path=${TPM_DIR}/swtpm.ctrl \
      --server type=unixio,path=${TPM_DIR}/swtpm \
      --tpm2 \
      --log file=${TPM_DIR}/swtpm.log,level=20 \
      --flags not-need-init \
      --pid file=${TPM_DIR}/swtpm.pid \
      --daemon

  while ! [ -S ${TPM_DIR}/swtpm ]; do
    sleep 0.1
  done

  tpm2_startup -c

  tpm2_createprimary -C o -G ecc384:aes256cfb -g sha384 -c ${TPM_DIR}/primary.ctx 
  tpm2_evictcontrol -C o -c ${TPM_DIR}/primary.ctx | awk '{print $2}'|xxd -r -p|xxd -p -c1|tac|xxd -r -p > ${TPM_ASSETS_DIR}/tpm2_primary_handle
  tpm2_import -C ${TPM_DIR}/primary.ctx -G ecc384 -g sha384 \
      -i ${OUTPUTDIR}/ca.key -u ${TPM_DIR}/ca.pub \
      -r ${TPM_DIR}/ca.priv --autoflush
  cp ${TPM_DIR}/ca.pub ${HTTP_ASSETS_DIR}/http_ca_pub
  cp ${TPM_DIR}/ca.priv ${HTTP_ASSETS_DIR}/http_ca_priv
  openssl pkey -in ${OUTPUTDIR}/ca.key -pubout -out ${HTTP_ASSETS_DIR}/http_ca_pem_pub
  make -C ${BASEDIR} qemu
else
  swtpm socket \
      --tpmstate dir=${TPM_DIR}/swtpm-state \
      --ctrl type=unixio,path=${TPM_DIR}/swtpm.ctrl \
      --server type=unixio,path=${TPM_DIR}/swtpm \
      --tpm2 \
      --log file=${TPM_DIR}/swtpm.log,level=20 \
      --flags not-need-init \
      --pid file=${TPM_DIR}/swtpm.pid \
      --daemon

  while ! [ -S ${TPM_DIR}/swtpm ]; do
    sleep 0.1
  done

  tpm2_startup -c
fi

NUMCPUS=4
RAMSIZE=8

MONITOR_XMAX=2048
MONITOR_YMAX=1152

# reserved vertical space (waybar etc.)
MONITOR_GAP=100

# get focused monitor resolution
read MON_W MON_H <<<"$(
  hyprctl monitors -j |
    jq -r '.[] | select(.focused==true) | "\(.width) \(.height)"'
)"

# usable area
AVAIL_W=$(( MON_W - MONITOR_GAP))
AVAIL_H=$(( MON_H - MONITOR_GAP))

# current desired aspect (should already be 16:9)
# but we calculate dynamically to be safe
DESIRED_W=$MONITOR_XMAX
DESIRED_H=$MONITOR_YMAX

# if it already fits, do nothing
if (( DESIRED_W <= AVAIL_W && DESIRED_H <= AVAIL_H )); then
    : # no-op
else
    # scale down while keeping aspect ratio
    if (( AVAIL_W * DESIRED_H >= AVAIL_H * DESIRED_W )); then
        # height limited
        MONITOR_YMAX=$AVAIL_H
        MONITOR_XMAX=$(( MONITOR_YMAX * DESIRED_W / DESIRED_H ))
    else
        # width limited
        MONITOR_XMAX=$AVAIL_W
        MONITOR_YMAX=$(( MONITOR_XMAX * DESIRED_H / DESIRED_W ))
    fi
fi

SERIALS=""

for i in `seq 0 $((NUMCPUS-1))`; do
  SERIALS="${SERIALS} -serial file:${BASEDIR}/tmp/qemu-serial${i}.log"
done

TRACE_OPTS="guest_errors,mmu"

# if trace_opts is not empty, then enable tracing (prefix with -d)

if [[ "${TRACE_OPTS}x" != "x" ]]; then
  TRACE_OPTS="-d ${TRACE_OPTS}"
fi

qemu-system-x86_64 \
  -nodefaults -no-user-config $PREVENTSHUTDOWN \
  -M q35,kernel-irqchip=split,smbios-entry-point-type=64 -m ${RAMSIZE}g -smp cpus=${NUMCPUS} -name osdev-hda-efi-boot \
  -cpu host,topoext=on,x2apic=on \
  -accel $ACCEL ${TRACE_OPTS} \
  -drive if=pflash,readonly=on,format=raw,unit=0,file=${OUTPUTDIR}/edk2-x86_64-code.fd \
  -drive if=pflash,readonly=off,format=raw,unit=1,file=${OUTPUTDIR}/edk2-i386-vars.fd \
  -drive id=system,if=none,format=raw,file=${OUTPUTDIR}/qemu-hda,werror=report,rerror=report \
  -drive id=cache,if=none,format=raw,file=${OUTPUTDIR}/qemu-nvme-cache,werror=report,rerror=report \
  -drive id=usbbot,if=none,format=raw,file=${OUTPUTDIR}/qemu-usb-bot,werror=report,rerror=report \
  -drive id=usbuas,if=none,format=raw,file=${OUTPUTDIR}/qemu-usb-uas,werror=report,rerror=report \
  -device ide-hd,drive=system,bootindex=1 \
  -device nvme,drive=cache,serial=qn0001,id=nvme0,logical_block_size=4096,physical_block_size=4096 \
  -device VGA,id=gpu0,vgamem_mb=256,xmax=${MONITOR_XMAX},ymax=${MONITOR_YMAX},xres=640,yres=480 \
  -device igb,netdev=t0,id=nic0 \
  -netdev $NETDEV \
  -device nec-usb-xhci,id=xhci0 \
  -device nec-usb-xhci,id=xhci1 \
  -device usb-hub,bus=xhci0.0,id=hub0,port=1 \
  -device usb-tablet,bus=xhci0.0,port=1.1 \
  -device usb-kbd,bus=xhci0.0,port=1.2 \
  -device usb-audio,bus=xhci0.0,port=1.3,buffer=1048576 \
  -device usb-storage,bus=xhci0.0,id=bot0,port=2,removable=on,drive=usbbot \
  -device usb-uas,bus=xhci0.0,id=uas0,port=3 \
  -device scsi-hd,bus=uas0.0,lun=0,removable=on,drive=usbuas \
  -device usb-host,hostbus=6,bus=xhci1.0,port=1,guest-reset=true,guest-resets-all=true,loglevel=4 \
  -device edu,id=edu,dma_mask=0xFFFFFFFFFFFFFFFF \
  -device amd-iommu,id=amdiommu,device-iotlb=on,intremap=on,xtsup=on,pt=on \
  $SERIALS \
  -chardev socket,id=chrtpm,path=${TPM_DIR}/swtpm.ctrl \
  -tpmdev emulator,id=tpm0,chardev=chrtpm \
  -device tpm-tis,tpmdev=tpm0 \
  -debugcon file:${BASEDIR}/tmp/qemu-acpi-debug.log -global isa-debugcon.iobase=0x402 \
  -monitor stdio \
  -audio pipewire \
  -display sdl,gl=on,show-cursor=off
