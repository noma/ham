#!/bin/bash

# Travis-CI helper to build MPIĆH for testing against newer compilers
# https://d-meiser.github.io/2016/01/10/mpi-travis.html

# NOTE: adapt URL to change to newer version
MPICH_URL=http://www.mpich.org/static/downloads/3.3.1/mpich-3.3.1.tar.gz

MPICH_FILE=${MPICH_URL##*/} # remove prefix until last slash
MPICH_DIR=${MPICH_FILE%.tar.gz}

# check cache
if [ -f mpich/lib/libmpich.so ]; then
	echo "libmpich.so found -- nothing to build."
else
	echo "Downloading MPICH source..."
	wget ${MPICH_URL}
	tar xf ${MPICH_FILE}
	rm ${MPICH_FILE}
	echo "Configuring and building MPICH..."
	cd ${MPICH_DIR}
	./configure \
		--prefix=`pwd`/../mpich \
		--enable-static=false \
		--enable-alloca=true \
		--disable-long-double \
		--enable-threads=single \
		--enable-fortran=no \
		--enable-fast=all \
		--enable-g=none \
		--enable-timing=none
	make -j4
	make install
	cd -
	rm -rf ${MPICH_DIR}
	echo "... done."
fi

