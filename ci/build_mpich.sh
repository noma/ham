#!/bin/bash

# Travis-CI helper to build MPIĆH for testing against newer compilers
# https://d-meiser.github.io/2016/01/10/mpi-travis.html

# NOTE: adapt URL to change to newer version
MPICH_URL=http://www.mpich.org/static/downloads/3.3.1/mpich-3.3.1.tar.gz

MPICH_FILE="${MPICH_URL##*/}" # remove prefix until last slash
MPICH_ORIG_DIR="${MPICH_FILE%.tar.gz}"
MPICH_SRC_DIR="${MPICH_FILE%.tar.gz}_${1}"
MPICH_INSTALL_DIR="mpich_${1}"

mkdir -p mpich
cd mpich

# check cache
if [ -f ${MPICH_INSTALL_DIR}/lib/libmpich.so ]; then
	echo "libmpich.so found -- nothing to build."
else
	echo "Downloading MPICH source..."
	wget ${MPICH_URL}
	tar xf ${MPICH_FILE}
	mv ${MPICH_ORIG_DIR} ${MPICH_SRC_DIR}
	rm ${MPICH_FILE}
	echo "Configuring and building MPICH..."
	cd ${MPICH_SRC_DIR}
	./configure \
		--prefix=$(pwd)/../${MPICH_INSTALL_DIR} \
		--enable-static=false \
		--enable-alloca=true \
		--disable-long-double \
		--enable-threads=multiple \
		--enable-fortran=no \
		--enable-fast=all \
		--enable-g=none \
		--enable-timing=none
	make -j4
	make install
	cd -
	rm -rf ${MPICH_SRC_DIR}
	echo "... done."
fi

ls -l ${MPICH_INSTALL_DIR}/bin

export PATH=$(pwd)/${MPICH_INSTALL_DIR}/bin:${PATH}
echo "PATH=${PATH}"

which mpicxx

ls -l ${MPICH_INSTALL_DIR}/lib

export LD_LIBRARY_PATH=$(pwd)/${MPICH_INSTALL_DIR}/lib:${LD_LIBRARY_PATH}
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"

cd ..

