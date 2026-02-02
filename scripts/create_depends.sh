#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

BUILDDIR="build"
SRCDIR="cc"
DESTDIR=$1
DEPFILE=$2
OBJFILE=${DEPFILE%.dep}

# Optional override for BINFILE
if [ -n "$3" ]; then
    BINFILE="$3"
else
    BINFILE=$(basename "$OBJFILE" .o).bin
fi

HEADERS=$(sed 's/\\$//' "$DEPFILE" | tr -s ' ' '\n' \
    | grep -E '^\S+\.h$' \
    | grep -v '^includes-local/' \
    | sed 's|.*/||; s|\.h$||' \
    | paste -sd ' ' -)

echo $HEADERS >&2

DEPSRC=`for h in $HEADERS; do
    # find matching .c or .cpp files under cc/
    find cc/ -type f \( -name "$h*.c" -o -name "$h*.cpp" \)
done`

DEPOBJ=$(for h in $HEADERS; do
    find "$SRCDIR"/ -type f \( -name "$h*.c" -o -name "$h*.cpp" \)
done | sed -e "s|^$SRCDIR|$BUILDDIR/$DESTDIR|" \
          -e "s|\.xx\.|.xx_64.|g" \
          -e "s|\.c$|.o|" \
          -e "s|\.cpp$|.o|")

# Optional: flatten into single line for Makefile usage
DEPOBJ_LINE=$(echo $DEPOBJ | tr '\n' ' ')

# Produce Makefile-style rule
echo "$BUILDDIR/$BINFILE: $OBJFILE $DEPOBJ_LINE"
