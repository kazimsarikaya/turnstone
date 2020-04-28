#!/bin/bash

for var in "$@"
do
  _sources=$(gcc -I../includes -MM ${var}.c | tr -d '\\\n'|cut -d':' -f2|sed 's/\.\.\/includes\///g'|sed 's/\.h//g')
  for _s in $_sources;
  do
    _s=$(echo $_s|tr -d ' ')
    for _f in $(find ../cc -name "$_s*.c");
    do
      _f=$(echo $_f|sed 's-\.\./cc/--g'|sed 's-\.c-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
      echo -e "../output/$var.bin: ../output/tests/$_f\n"
    done
  done
  echo
done
