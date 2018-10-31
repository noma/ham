# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham_lib LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

# interface target for ham
add_library(ham_interface INTERFACE)
target_compile_features(ham_interface INTERFACE )
target_link_libraries(ham_interface INTERFACE noma_bmt boost_library)
target_include_directories(ham_interface INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../../include)

set(HAM_LIB_SRC
	net/communicator.cpp
	net/communicator_mpi.cpp
	net/communicator_mpi_rma_dynamic.cpp
	offload/runtime.cpp
	offload/offload.cpp
	util/cpu_affinity.cpp)

# TCP
add_library(ham_offload_tcp # SHARED if BUILD_SHARED_LIBS = TRUE
		net/communicator.cpp
		net/communicator_tcp.cpp
		offload/runtime.cpp
		offload/offload.cpp
		offload/main.cpp
		util/cpu_affinity.cpp)
target_compile_definitions(ham_offload_tcp PUBLIC -DHAM_COMM_TCP=1)
target_link_libraries(ham_offload_tcp PUBLIC ham_interface boost_library pthread)

set_target_properties(ham_offload_tcp PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO)

if (MPI_FOUND)
	add_library(ham_offload_mpi # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main.cpp
	            net/communicator_mpi.cpp)
	target_compile_definitions(ham_offload_mpi PUBLIC -DHAM_COMM_MPI=1)
	target_link_libraries(ham_offload_mpi PUBLIC ham_interface mpi_library)

	add_library(ham_offload_mpi_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main_explicit.cpp
	            net/communicator_mpi.cpp)
	target_compile_definitions(ham_offload_mpi_explicit PUBLIC -DHAM_COMM_MPI=1 -DHAM_EXPLICIT=1)
	target_link_libraries(ham_offload_mpi_explicit PUBLIC ham_interface mpi_library)

	add_library(ham_offload_mpi_rma_dynamic # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main.cpp
	            net/communicator_mpi_rma_dynamic.cpp)
	target_compile_definitions(ham_offload_mpi_rma_dynamic PUBLIC -DHAM_COMM_MPI_RMA_DYNAMIC=1)
	target_link_libraries(ham_offload_mpi_rma_dynamic PUBLIC ham_interface mpi_library)

	set_target_properties(ham_offload_mpi ham_offload_mpi_explicit ham_offload_mpi_rma_dynamic PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO)
endif ()

if (SCIF_FOUND)
	add_library(ham_offload_scif # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main.cpp
	            net/communicator_scif.cpp)
	target_compile_definitions(ham_offload_scif PUBLIC -DHAM_COMM_SCIF=1)
	target_link_libraries(ham_offload_scif_explicit PUBLIC ham_interface scif_library)

	add_library(ham_offload_scif_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main_explicit.cpp
	            net/communicator_scif.cpp)
	target_compile_definitions(ham_offload_scif_explicit PUBLIC -DHAM_COMM_SCIF=1 -DHAM_EXPLICIT=1)
	target_link_libraries(ham_offload_scif_explicit PUBLIC ham_interface scif_library)

	set_target_properties(ham_offload_scif ham_offload_scif_explicit PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO)
endif ()
