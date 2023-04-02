#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

for var in "$@"
do
  _depends=$(gcc -I../includes -MM ${var}.c | tr -d '\\\n'|cut -d':' -f2|sed 's/\.\.\/includes\///g')
  _sources=$(echo $_depends|sed 's/\.h//g')
  for _s in $_sources;
  do
    _s=$(basename $_s|tr -d ' ')
    for _f in $(find ../cc -name "$_s*.c"|grep -v video);
    do
      _f=$(echo $_f|sed 's-\.\./cc/--g'|sed 's-\.c-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
      echo -e "../output/$var.bin: ../output/cc-local/$_f"
    done
  done

  _headers=$(echo $_depends|tr ' ' '\n'|grep -v ^$|grep -v ".c$"|sort|uniq)
  for _h in $_headers;
  do
   _f=$(find ../includes|grep $_h|tr -d '\n')
   if [[ "${_f}x" != "x" ]]; then
     echo -e "../output/cc-local/$var.o: $_f"
   fi
  done
  echo
done
