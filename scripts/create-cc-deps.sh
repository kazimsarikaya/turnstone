#!/bin/sh

CC="$1"
SOURCES="$2"

for c_src in ${SOURCES}; do
  output_dir_name="output/$(dirname $c_src)"
  ${CC} $c_src |sed -E 's%^([^ :]+):%'${output_dir_name}'/\1:%g'
  echo
done
