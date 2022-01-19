#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"

sudo sh -c "> ${BASEDIR}/tmp/qemu-video-test.log; > ${BASEDIR}/tmp/qemu-test.log"

sudo qemu-system-x86_64 \
  -M q35 -m 1g -smp cpus=2 -name osdev-hda-test \
  -drive index=0,media=disk,format=raw,file=${OUTPUTDIR}/qemu-test-hda \
  -net nic,model=virtio,macaddr=54:54:00:55:55:55 \
  -net tap,script=${CURRENTDIR}/tap-up.sh,downscript=${CURRENTDIR}/tap-down.sh  \
  -serial file:${BASEDIR}/tmp/qemu-video-test.log \
  -serial file:${BASEDIR}/tmp/qemu-test.log \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -nographic

exit_code=$?

exit_code=$(($exit_code>>1))

echo

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

if [[ $exit_code -eq 0 ]]; then
  echo "${GREEN}All tests passed${NC}"
  exit 0
else
  echo "${RED}Some tests failed${NC}"
  exit 1
fi
