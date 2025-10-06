#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

set -eux

CURRENTDIR=`dirname $0`
CURRENTDIR="`readlink -f ${CURRENTDIR}`"
BASEDIR="${CURRENTDIR}/../../"
BASEDIR="`readlink -f ${BASEDIR}`"
OUTPUTDIR="${BASEDIR}/build"
CCGENDIR="${OUTPUTDIR}/cc-gen"
INCLUDESGENDIR="${OUTPUTDIR}/includes-gen"
ASSETSGENDIR="${OUTPUTDIR}/assets-gen"
PYENVDIR="${OUTPUTDIR}/pyenv"

PYFILE="${CURRENTDIR}/../assets-data/build_font_atlas.py"
PYFILE="`readlink -f ${PYFILE}`"

make -C "${BASEDIR}" "build/deflate.bin"

if ! python3 --version >/dev/null 2>&1; then
  echo "Python3 is not installed. Please install Python3."
  exit 1
fi

if [ ! -f $PYFILE ]; then
  echo "File ${PYFILE} not found!"
  exit 1
fi

if [ ! -f ${PYENVDIR}/pyvenv.cfg ]; then
    rm -rf ${PYENVDIR}
    python3 -m venv ${PYENVDIR}
fi

source ${PYENVDIR}/bin/activate

echo $PATH

pip install --upgrade pip
pip install freetype-py numpy scipy Pillow

python3 "${PYFILE}" --base_dir "${BASEDIR}" --invert_png
