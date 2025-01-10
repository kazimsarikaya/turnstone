#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CURRENTDIR=`dirname $0`
BASEDIR="${CURRENTDIR}/../../"
OUTPUTDIR="${BASEDIR}/output"
ASSETSDIR="${BASEDIR}/assets"
OBJCOPY="objcopy"
CCOUTPUTDIR="${OUTPUTDIR}/cc"

ASSETSDIR=$(readlink -f ${ASSETSDIR})

OUTPUT_FILE=$1
#remove .data.o at end
INPUT_FILE=${OUTPUT_FILE%.data.o}
#remove output/ at start
INPUT_FILE=${INPUT_FILE#output/}

echo "OUTPUT_FILE: ${OUTPUT_FILE}"
echo "INPUT_FILE: ${INPUT_FILE}"

set -x

asset=${INPUT_FILE}
asset_rel=${asset#${ASSETSDIR}/}
asset_under=${asset//\//_}
asset_under=${asset_under//./_}

module_name=$(dirname ${asset_rel}|sed 's/\//./g')

file_name_base=$(basename ${asset})
file_name=${file_name_base%.*}
file_ext=${file_name_base##*.}


echo "Copying asset: ${asset} ${module_name}"


echo -n ${module_name} > ${OUTPUTDIR}/module_name_${file_name}.txt

${OBJCOPY} -O elf64-x86-64 -B i386 -I binary \
    --rename-section .data=.rodata.${file_name} \
    --redefine-sym _binary_${asset_under}_start=${file_name}_data_start \
    --redefine-sym _binary_${asset_under}_end=${file_name}_data_end \
    --redefine-sym _binary_${asset_under}_size=${file_name}_data_size \
    --add-section .___module___=${OUTPUTDIR}/module_name_${file_name}.txt \
    --add-symbol ___module___=.___module___:${module_name} \
    ${INPUT_FILE} ${OUTPUT_FILE}

rm -f ${OUTPUTDIR}/module_name_${file_name}.txt

