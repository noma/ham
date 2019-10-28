# Copyright (c) 2019 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham_exe LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

# add HAM project as subdirectory, assuming a copy of the repository's ham-directory has been copied into the project
option(HAM_BUILD_EXECUTABLES "" OFF) # override option, to disable building executables of the HAM sub-project
add_subdirectory(ham)

# hello_world builds for different backends:

# MPI Backend
if (MPI_FOUND)
	add_executable(hello_world_mpi hello_world.cpp)
	target_link_libraries(hello_world_mpi ham_offload_mpi)
endif ()

# SCIF Backend (Intel Xeon Phi accelerator)
if (SCIF_FOUND)
	add_executable(hello_world_scif hello_world.cpp)
	target_link_libraries(hello_world_scif ham_offload_scif)
endif ()

# There are two back-ends for the NEC VE, where VEO+VEDMA is the faster one and VEO-only acts as a reference.
# NEC requires two builds, one for the Vector Engine (VE) and one for the Vector Host (VH),
# i.e. we need to create two build folders with CMake

# NEC VEO-only backend
if (VEO_FOUND)
	if (HAM_NEC_COMPILER_DETECTED)
		# Vector Engine (VE) libraries
		add_library(hello_world_veo_ve hello_world.cpp)
		SET(HAMLIB_VE -Wl,--whole-archive ham_offload_veo_ve -Wl,--no-whole-archive)
		target_link_libraries(hello_world_veo_ve ${HAMLIB_VE})
		set_property(TARGET hello_world_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		add_custom_command(TARGET hello_world_veo_ve
		                   POST_BUILD
		                   COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_hello_world_veo ./libhello_world_veo_ve.a ./ham/libham_offload_veo_ve.a)
	else ()
		# Vector Host (VH) executables
		add_executable(hello_world_veo_vh hello_world.cpp)
		target_link_libraries(hello_world_veo_vh ham_offload_veo_vh)
	endif ()
endif ()

# NEC VEO+VEDMA backend
if (VEO_FOUND)
	if (HAM_NEC_COMPILER_DETECTED)
		# Vector Engine (VE) libraries
		add_library(hello_world_vedma_ve hello_world.cpp)
		SET(HAMLIB_VE -Wl,--whole-archive ham_offload_vedma_ve -Wl,--no-whole-archive)
		target_link_libraries(hello_world_vedma_ve ${HAMLIB_VE})
		set_property(TARGET hello_world_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		add_custom_command(TARGET hello_world_vedma_ve
		                   POST_BUILD
		                   COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_hello_world_vedma ./libhello_world_vedma_ve.a ./ham/libham_offload_vedma_ve.a ${MK_VEORUN_STATIC_LIBS})
	else ()
		# Vector Host (VH) executables
		add_executable(hello_world_vedma_vh hello_world.cpp)
		target_link_libraries(hello_world_vedma_vh ham_offload_vedma_vh)
	endif ()
endif ()
