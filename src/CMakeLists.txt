# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham_exe LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

add_subdirectory(ham)

### Benchmarks

message(${CMAKE_CXX_COMPILER_ID})

## Explicit targets (not built by default)
# Intel LEO offload directive benchmark, requires Intel compiler
if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	add_executable(benchmark_intel_leo EXCLUDE_FROM_ALL benchmark_intel_leo.cpp)
	target_link_libraries(benchmark_intel_leo ham_interface)
endif ()

if (MPI_FOUND)
	add_executable (benchmark_ham_offload_mpi benchmark_ham_offload.cpp)
	target_link_libraries (benchmark_ham_offload_mpi ham_offload_mpi)
endif()

if (SCIF_FOUND)
	add_executable (benchmark_ham_offload_scif benchmark_ham_offload.cpp)
	target_link_libraries (benchmark_ham_offload_scif ham_offload_scif)
endif()

### Examples/Tests

# some tests for the active msg layer
add_executable(active_msgs active_msgs.cpp)
target_link_libraries(active_msgs ham_interface)

if (MPI_FOUND)
	add_executable(ham_offload_test_mpi ham_offload.cpp)
	target_link_libraries(ham_offload_test_mpi ham_offload_mpi)

	add_executable(ham_offload_test_explicit_mpi ham_offload_explicit.cpp)
	target_link_libraries(ham_offload_test_explicit_mpi ham_offload_mpi_explicit)

	add_executable(inner_product_mpi inner_product.cpp)
	target_link_libraries(inner_product_mpi ham_offload_mpi)

	add_executable(test_data_transfer_mpi test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_mpi ham_offload_mpi)

	add_executable(test_argument_transfer_mpi test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_mpi ham_offload_mpi)
endif()

if (SCIF_FOUND)
	add_executable(ham_offload_test_scif ham_offload.cpp)
	target_link_libraries(ham_offload_test_scif ham_offload_scif)

	add_executable(ham_offload_test_explicit_scif ham_offload_explicit.cpp)
	target_link_libraries(ham_offload_test_explicit_scif ham_offload_scif_explicit)

	add_executable(inner_product_scif inner_product.cpp)
	target_link_libraries(inner_product_scif ham_offload_scif)

	add_executable(test_data_transfer_scif test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_scif ham_offload_scif)

	add_executable(test_argument_transfer_scif test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_scif ham_offload_scif)
endif()

if (VEO_FOUND)
	# Vector Host Executables
	add_executable(ham_offload_test_veo_vh ham_offload.cpp)
	target_link_libraries(ham_offload_test_veo_vh ham_offload_veo_vh)

	add_executable(ham_offload_test_explicit_veo_vh ham_offload_explicit.cpp)
	target_link_libraries(ham_offload_test_explicit_veo_vh ham_offload_veo_vh_explicit)

	add_executable(inner_product_veo_vh inner_product.cpp)
	target_link_libraries(inner_product_veo_vh ham_offload_veo_vh)

	add_executable(test_data_transfer_veo_vh test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_veo_vh ham_offload_veo_vh)

	add_executable(test_argument_transfer_veo_vh test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_veo_vh ham_offload_veo_vh)
	
	# Vector Engine Libraries
	add_library(ham_offload_test_veo_ve ham_offload.cpp)
	target_link_libraries(ham_offload_test_veo_ve ham_offload_veo_ve)

	add_library(ham_offload_test_explicit_veo_ve ham_offload_explicit.cpp)
	target_link_libraries(ham_offload_test_explicit_veo_ve ham_offload_veo_ve_explicit)

	# https://stackoverflow.com/questions/5693405/specifying-link-flags-for-only-one-static-lib-while-linking-executable
	# hams static lib contains symbols needed for VEO calls
	SET (HAMLIB -Wl,--whole-archive ham_offload_veo_ve -Wl,--no-whole-archive)
# NOTE: static vs. shared
#	add_library(inner_product_veo_ve SHARED inner_product.cpp) # for shared VEO lib
	add_library(inner_product_veo_ve inner_product.cpp) # for static VEO lib
	#target_link_libraries(inner_product_veo_ve ham_offload_veo_ve)
	target_link_libraries(inner_product_veo_ve ${HAMLIB})
	set_property(TARGET inner_product_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)

	add_library(test_data_transfer_veo_ve test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_veo_ve ham_offload_veo_ve)

	add_library(test_argument_transfer_veo_ve test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_veo_ve ham_offload_veo_ve)
	
endif()
