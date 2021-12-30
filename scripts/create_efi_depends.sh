#!/usr/bin/env bash

APP=$1

shift

DEPEND_OBJS=""

for _param in $@;
do
  _sources=$(gcc -I../includes -MM $_param | tr -d '\\\n'|cut -d':' -f2|sed 's/\.\.\/includes\///g'|sed 's/\.h//g')
  for _s in $_sources;
  do
    _s=$(echo $_s|tr -d ' ')
    for _f in $(find ../cc -name "$_s*.c"|grep -v video);
    do
      _f=$(echo $_f|sed 's-\.\./cc/--g'|sed 's-\.c-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
      DEPEND_OBJS="$DEPEND_OBJS\n$_f"
    done
  done
  echo
done

DEPEND_OBJS=$(echo -e $DEPEND_OBJS|sort|uniq)

for _f in $DEPEND_OBJS;
do
  echo -e "$APP: ../output/efi/$_f"
  echo -e "EFIOBJS += \$(EFIOUTPUT)/$_f"
done
