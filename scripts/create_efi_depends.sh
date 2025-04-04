#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

TOSDBIMG=$1 

shift

DEPEND_OBJS=""

for _param in $@; do
  _sources=$(gcc -I../includes -MM $_param | tr -d '\\\n'|cut -d':' -f2|sed 's/\.\.\/includes\///g'|sed 's/\.h//g')

  for _s in $_sources; do
    _s=$(basename $_s|tr -d ' ')

    for _f in $(find ../cc -name "$_s*.c"|grep -v video|grep -v graphics); do
      _f=$(echo $_f|sed 's-\.\./cc/--g'|sed 's-\.c-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
      DEPEND_OBJS="$DEPEND_OBJS\n$_f"
    done

    for _f in $(find ../cc -name "$_s*.cpp"|grep -v video|grep -v graphics); do
      _f=$(echo $_f|sed 's-\.\./cc/--g'|sed 's-\.cpp-\.o-g'|sed 's-\.xx\.o-\.xx_64\.o-g')
      DEPEND_OBJS="$DEPEND_OBJS\n$_f"
    done

  done

done

DEPEND_OBJS=$(echo -e $DEPEND_OBJS|sort|uniq)

for _f in $DEPEND_OBJS; do
  _s=$(basename $_f|sed 's/\..*$//g')
  _hs=$(find ../includes -name "$_s*.h")

  if [ -z "$_hs" ]; then
    echo 
  else 
    echo -e ../build/efi/$_f: $(find ../includes -name "$_s*.h") 
    echo 
  fi
 
  echo -e "$TOSDBIMG: \$(EFIOUTPUT)/$_f"
  echo -e "EFIOBJS += \$(EFIOUTPUT)/$_f"
  echo
done
