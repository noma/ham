#!/bin/bash

# Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# This script will build and install Boost including Boost.Build.
# DOWNLOAD_PATH is expected to contain a boost archive in tar.bz2 format.
# Set the variables below to your needs. Assuming the defaults, this script
# will perform the following steps:
# - unpack boost_1_56_0.tar.bz2 inside ~/Downloads
# - build Boost.Build and install it in:
#     ~/Software/boost_1_56_0
# - build Boost for the host and install it in:
#     ~/Software/boost_1_56_0
# - build Boost for the Xeon Phi and install it in:
#     ~/Software/boost_1_56_0_mic
# - create version independent symlinks
#     ~/Software/boost
#     ~/Software/boost_mic
# - append Boost specific exports to your .bashrc
# - create some build_boost*.log files in the current directory

# NOTES:
# If you want the Boost MPI library, you need to create user-config.jam in your
# home containing the line "using mpi ;".
# 

# References:
# http://www.boost.org/users/download/
# http://www.boost.org/more/getting_started/unix-variants.html#prepare-to-use-a-boost-library-binary
# https://software.intel.com/en-us/articles/building-the-boost-library-to-run-natively-on-intelr-xeon-phitm-coprocessor

DOWNLOAD_PATH=$HOME/Downloads
INSTALL_PATH=$HOME/Software
NO_MIC=false # set to true, to disable building Boost for Xeon Phi
BASHRC_FILE=$HOME/.bashrc # set to /dev/null to disable, or to any other file to manually merge the needed changes into your .bashrc 

BOOST_BUILD_OPTIONS="-j8" # concurrent build with up to 8 commands
BOOST_NAME=boost
BOOST_VERSION=1_56_0
BOOST_MIC_SUFFIX=mic
BOOST_ARCHIVE=${BOOST_NAME}_${BOOST_VERSION} # NOTE: without tar.bz2

##### end of user-configuration #####

# create paths from the configuration variables
BOOST_PATH=${BOOST_NAME}_${BOOST_VERSION}
BOOST_PATH_MIC=${BOOST_PATH}_${BOOST_MIC_SUFFIX}
BOOST_PATH_GENERIC=${BOOST_NAME}
BOOST_PATH_GENERIC_MIC=${BOOST_PATH_GENERIC}_mic
BOOST_INSTALL_PATH=$INSTALL_PATH/$BOOST_PATH
BOOST_INSTALL_PATH_MIC=$INSTALL_PATH/$BOOST_PATH_MIC

PWD=$(pwd)

BUILD_LOG_BB=$PWD/build_boost_build.log
BUILD_LOG=$PWD/build_boost.log
BUILD_LOG_MIC=$PWD/build_boost_mic.log

# Unpack boost
BOOST_ARCHIVE_FILE=${BOOST_ARCHIVE}.tar.bz2
if [ -a $DOWNLOAD_PATH/${BOOST_ARCHIVE_FILE} ]; then
	echo "Unpacking Boost ..."
	cd $DOWNLOAD_PATH
	tar xjf ${BOOST_ARCHIVE_FILE}
	cd ${BOOST_ARCHIVE}
else
	echo "Error: $DOWNLOAD_PATH/$BOOST_ARCHIVE_FILE does not exist."
fi

# Building Boost.Build
cd tools/build
echo "Building Boost.Build ..."
./bootstrap.sh > $BUILD_LOG_BB 2>&1
echo "Installing Boost.Build ..."
./b2 install --prefix=$BOOST_INSTALL_PATH >> $BUILD_LOG_BB 2>&1
PATH=$BOOST_INSTALL_PATH/bin:$PATH
cd ../..

# Build Boost for the Host (using the systems default compiler)
# no bootstrapping needed, since just installed Boost.Build
echo "Building and installing Boost into $BOOST_INSTALL_PATH ..."
b2 install --prefix=$BOOST_INSTALL_PATH $BOOST_BUILD_OPTIONS > $BUILD_LOG  2>&1

# Build Boost for the Xeon Phi (using the Intel compiler, which must be set up in the environment)
if [ $NO_MIC != 'true' ]; then
	echo "Building and installing Boost for Xeon Phi into $BOOST_INSTALL_PATH_MIC ..."
	b2 install --prefix=$BOOST_INSTALL_PATH_MIC $BOOST_BUILD_OPTIONS toolset=intel --disable-icu --without-iostreams cflags="-mmic" cxxflags="-mmic" linkflags="-mmic" > $BUILD_LOG_MIC 2>&1
fi

# create generic symlinks
echo "Creating symlinks ..."
cd $INSTALL_PATH
# if does not exist, or is a symlink, create/overwrite it
if [[ !(-a $BOOST_PATH_GENERIC) || -L $BOOST_PATH_GENERIC ]]; then
	ln -snf $BOOST_INSTALL_PATH $BOOST_PATH_GENERIC
else
	echo "Warning: $INSTALL_PATH/$BOOST_PATH_GENERIC already exists and is not a link. "
fi
# same for the MIC path
if [ $NO_MIC != 'true' ] && [[ !(-a $BOOST_PATH_GENERIC) || -L $BOOST_PATH_GENERIC ]]; then
	ln -snf $BOOST_INSTALL_PATH_MIC $BOOST_PATH_GENERIC_MIC
else
	echo "Warning: $INSTALL_PATH/$BOOST_PATH_GENERIC_MIC already exists and is not a link. "
fi

echo "Appending environment configuration to $BASHRC_FILE ..."
# add Boost.Build to PATH
echo -e '\n# Boost environment' >> $BASHRC_FILE
echo "export PATH=$INSTALL_PATH/$BOOST_PATH_GENERIC/bin:"'$PATH' >> $BASHRC_FILE

# export BOOST_ROOT
echo "export BOOST_ROOT=$INSTALL_PATH/$BOOST_PATH_GENERIC" >> $BASHRC_FILE

if [ $NO_MIC != 'true' ]; then
	echo "export MIC_BOOST_ROOT=$INSTALL_PATH/$BOOST_PATH_GENERIC_MIC" >> $BASHRC_FILE
fi

echo 'export LD_LIBRARY_PATH=$BOOST_ROOT/lib:$LD_LIBRARY_PATH' >> $BASHRC_FILE

echo "Done."
echo Please perform a \"source ~/.bashrc\", or re-login.

