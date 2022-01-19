#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CC="$1"
SOURCES="$2"

for c_src in ${SOURCES}; do
  output_dir_name="output/$(dirname $c_src)"
  ${CC} $c_src |sed -E 's%^([^ :]+):%'${output_dir_name}'/\1:%g'
  echo
done
