#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

for var in "$@"
do
   also_cpp=0 
   if [[ ! -f ${var}.c ]]; then
        if [[ -f ${var}.cpp ]]; then
            also_cpp=1
            file=${var}.cpp
        else
            echo "File ${var}.c not found"
            exit 1
        fi
   else 
       file=${var}.c
   fi

  _depends=$(gcc -Iincludes -Iincludes-local -MM ${file} | tr -d '\\\n'|cut -d':' -f2|sed 's/includes\///g')
  _sources=$(echo $_depends|sed -E 's/\.(h|hpp)//g')
  for _s in $_sources;
  do
    _s=$(basename $_s|tr -d ' ')
    for _f in $(find cc -name "$_s*.c"|grep -v video| grep -v '\.test\.c');
    do
      _f=$(echo $_f|sed 's-cc/--g'|sed 's-\.c-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
      echo -e "build/$(basename $var).bin: build/cc-local/$_f"
    done

    if [[ $also_cpp -eq 1 ]]; then
      for _f in $(find cc -name "$_s*.cpp"|grep -v video);
      do
        _f=$(echo $_f|sed 's-cc/--g'|sed 's-\.cpp-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
        echo -e "build/$(basename $var).bin: build/cc-local/$_f"
      done
    fi
  done

  _headers=$(echo $_depends|tr ' ' '\n'|grep -v ^$|grep -v ".c$"|sort|uniq)
  for _h in $_headers;
  do
   _f=$(find includes|grep $_h|tr '\n' ' ')
   if [[ "${_f}x" != "x" ]]; then
       echo -e "build/cc-local/$(basename $var).o: $_f"
   fi
  done
  echo
done
