# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018-2019 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham_exe LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

add_subdirectory(ham)

if (HAM_BUILD_EXECUTABLES)

### Benchmarks

	## Explicit targets (not built by default)
	# Intel LEO offload directive benchmark, requires Intel compiler
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
		add_executable(benchmark_intel_leo EXCLUDE_FROM_ALL benchmark_intel_leo.cpp)
		target_link_libraries(benchmark_intel_leo ham_interface)
	endif ()

	if (MPI_FOUND)
		add_executable(benchmark_ham_offload_mpi benchmark_ham_offload.cpp)
		target_link_libraries(benchmark_ham_offload_mpi ham_offload_mpi)
	endif ()

	if (SCIF_FOUND)
		add_executable(benchmark_ham_offload_scif benchmark_ham_offload.cpp)
		target_link_libraries(benchmark_ham_offload_scif ham_offload_scif)
	endif ()

	if (VEO_FOUND)

		# non-HAM VEO and VEDMA benchmarks:

		if (NOT HAM_NEC_COMPILER_DETECTED)
			# Vector Host executables

			# VEO benchmark
			add_executable(benchmark_veo_vh benchmark_veo.cpp)
			target_link_libraries(benchmark_veo_vh veo_library cli11_library noma_bmt noma_debug)

			# VEDMA benchmark
			add_executable(benchmark_vedma_vh benchmark_vedma_vh.cpp)
			target_link_libraries(benchmark_vedma_vh veo_library cli11_library noma_bmt noma_debug)

			set_target_properties(benchmark_veo_vh benchmark_vedma_vh PROPERTIES
				CXX_STANDARD 11
				CXX_STANDARD_REQUIRED YES
				CXX_EXTENSIONS NO)
		else () 
			# Vector Engine libraries

			# VEO benchmark
			add_library(benchmark_veo_ve benchmark_veo_lib.cpp)
			set_property(TARGET benchmark_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(benchmark_veo_ve "" "")

			# VEDMA benchmark
			add_executable(benchmark_vedma_ve benchmark_vedma_ve.cpp)
			target_link_libraries(benchmark_vedma_ve cli11_library noma_bmt veos_system_libraries)

			set_target_properties(benchmark_vedma_ve PROPERTIES
				CXX_STANDARD 11
				CXX_STANDARD_REQUIRED YES
				CXX_EXTENSIONS YES) # NOTE: inline assembly requires g++11 extension
		endif ()

		# VEO-only Backend

		if (NOT HAM_NEC_COMPILER_DETECTED)
			# Vector Host executables
			# HAM benchmark
			add_executable(benchmark_ham_offload_veo_vh benchmark_ham_offload.cpp)
			target_link_libraries(benchmark_ham_offload_veo_vh ham_offload_veo_vh)

			set_target_properties(benchmark_veo_vh PROPERTIES
				CXX_STANDARD 11
				CXX_STANDARD_REQUIRED YES
				CXX_EXTENSIONS NO)
		else () 
			# Vector Engine libraries

			# HAM Benchmark
			add_library(benchmark_ham_offload_veo_ve benchmark_ham_offload.cpp)
			target_link_libraries(benchmark_ham_offload_veo_ve ${HAM_LIB_VEO_VE_CLI})
			set_property(TARGET benchmark_ham_offload_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(benchmark_ham_offload_veo_ve ${HAM_LIB_VEO_VE} "")
		endif ()

		# VEO + DMA Backend

		if (NOT HAM_NEC_COMPILER_DETECTED)
			# Vector Host executables

			# HAM benchmark
			add_executable(benchmark_ham_offload_vedma_vh benchmark_ham_offload.cpp)
			target_link_libraries(benchmark_ham_offload_vedma_vh ham_offload_vedma_vh)

			set_target_properties(benchmark_ham_offload_vedma_vh PROPERTIES
				CXX_STANDARD 11
				CXX_STANDARD_REQUIRED YES
				CXX_EXTENSIONS NO)
		else () 
			# Vector Engine libraries

			# HAM Benchmark
			add_library(benchmark_ham_offload_vedma_ve benchmark_ham_offload.cpp)
			target_link_libraries(benchmark_ham_offload_vedma_ve ${HAM_LIB_VEDMA_VE_CLI})
			set_property(TARGET benchmark_ham_offload_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(benchmark_ham_offload_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")
		endif ()

	endif ()

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

		add_executable(test_multiple_targets_mpi test_multiple_targets.cpp)
		target_link_libraries(test_multiple_targets_mpi ham_offload_mpi)
	endif ()

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

		add_executable(test_multiple_targets_scif test_multiple_targets.cpp)
		target_link_libraries(test_multiple_targets_scif ham_offload_scif)
	endif ()


	if (VEO_FOUND)

		# VEO-only Backend

		if (NOT HAM_NEC_COMPILER_DETECTED)
			# Vector Host executables

			add_executable(ham_offload_test_veo_vh ham_offload.cpp)
			target_link_libraries(ham_offload_test_veo_vh ham_offload_veo_vh)

			add_executable(ham_offload_test_explicit_veo_vh ham_offload_explicit.cpp)
			target_link_libraries(ham_offload_test_explicit_veo_vh ham_offload_veo_vh_explicit)

			add_executable(hello_nec_dma_veo_vh hello_nec_dma.cpp)
			target_link_libraries(hello_nec_dma_veo_vh ham_offload_veo_vh)

			add_executable(inner_product_veo_vh inner_product.cpp)
			target_link_libraries(inner_product_veo_vh ham_offload_veo_vh)

			add_executable(test_data_transfer_veo_vh test_data_transfer.cpp)
			target_link_libraries(test_data_transfer_veo_vh ham_offload_veo_vh)

			add_executable(test_argument_transfer_veo_vh test_argument_transfer.cpp)
			target_link_libraries(test_argument_transfer_veo_vh ham_offload_veo_vh)

			add_executable(test_multiple_targets_veo_vh test_multiple_targets.cpp)
			target_link_libraries(test_multiple_targets_veo_vh ham_offload_veo_vh)
		else ()
			# Vector Engine libraries

			add_library(ham_offload_test_veo_ve ham_offload.cpp)
			target_link_libraries(ham_offload_test_veo_ve ${HAM_LIB_VEO_VE_CLI})
			set_property(TARGET ham_offload_test_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(ham_offload_test_veo_ve ${HAM_LIB_VEO_VE} "")

			add_library(ham_offload_test_explicit_veo_ve ham_offload_explicit.cpp)
			target_link_libraries(ham_offload_test_explicit_veo_ve -Wl,--whole-archive ham_offload_veo_ve_explicit -Wl,--no-whole-archive)
			set_property(TARGET ham_offload_test_explicit_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(ham_offload_test_explicit_veo_ve ham_offload_veo_ve_explicit "")

			add_library(hello_nec_dma_veo_ve hello_nec_dma.cpp) # for static VEO lib
			target_link_libraries(hello_nec_dma_veo_ve ${HAM_LIB_VEO_VE_CLI} veos_system_libraries)
			set_property(TARGET hello_nec_dma_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(hello_nec_dma_veo_ve ${HAM_LIB_VEO_VE} "")

			# NOTE: static vs. shared
			# add_library(inner_product_veo_ve SHARED inner_product.cpp) # for shared VEO lib
			add_library(inner_product_veo_ve inner_product.cpp) # for static VEO lib
			target_link_libraries(inner_product_veo_ve ${HAM_LIB_VEO_VE_CLI})
			set_property(TARGET inner_product_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(inner_product_veo_ve ${HAM_LIB_VEO_VE} "")

			add_library(test_data_transfer_veo_ve test_data_transfer.cpp)
			target_link_libraries(test_data_transfer_veo_ve ${HAM_LIB_VEO_VE_CLI})
			set_property(TARGET test_data_transfer_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(test_data_transfer_veo_ve ${HAM_LIB_VEO_VE} "")

			add_library(test_argument_transfer_veo_ve test_argument_transfer.cpp)
			target_link_libraries(test_argument_transfer_veo_ve ${HAM_LIB_VEO_VE_CLI})
			set_property(TARGET test_argument_transfer_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(test_argument_transfer_veo_ve ${HAM_LIB_VEO_VE} "")

			add_library(test_multiple_targets_veo_ve test_multiple_targets.cpp)
			target_link_libraries(test_multiple_targets_veo_ve ${HAM_LIB_VEO_VE_CLI})
			set_property(TARGET test_multiple_targets_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(test_multiple_targets_veo_ve ${HAM_LIB_VEO_VE} "")

		endif ()

		# VEO + DMA Backend

		if (NOT HAM_NEC_COMPILER_DETECTED)
			# Vector Host executables

			add_executable(ham_offload_test_vedma_vh ham_offload.cpp)
			target_link_libraries(ham_offload_test_vedma_vh ham_offload_vedma_vh)

			add_executable(ham_offload_test_explicit_vedma_vh ham_offload_explicit.cpp)
			target_link_libraries(ham_offload_test_explicit_vedma_vh ham_offload_vedma_vh_explicit)

			add_executable(hello_nec_dma_vedma_vh hello_nec_dma.cpp)
			target_link_libraries(hello_nec_dma_vedma_vh ham_offload_vedma_vh)

			add_executable(inner_product_vedma_vh inner_product.cpp)
			target_link_libraries(inner_product_vedma_vh ham_offload_vedma_vh)

			add_executable(test_data_transfer_vedma_vh test_data_transfer.cpp)
			target_link_libraries(test_data_transfer_vedma_vh ham_offload_vedma_vh)

			add_executable(test_argument_transfer_vedma_vh test_argument_transfer.cpp)
			target_link_libraries(test_argument_transfer_vedma_vh ham_offload_vedma_vh)

			add_executable(test_multiple_targets_vedma_vh test_multiple_targets.cpp)
			target_link_libraries(test_multiple_targets_vedma_vh ham_offload_vedma_vh)
		else ()
			# Vector Engine libraries

			add_library(ham_offload_test_vedma_ve ham_offload.cpp)
			target_link_libraries(ham_offload_test_vedma_ve ${HAM_LIB_VEDMA_VE_CLI})
			set_property(TARGET ham_offload_test_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(ham_offload_test_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")

			add_library(ham_offload_test_explicit_vedma_ve ham_offload_explicit.cpp)
			target_link_libraries(ham_offload_test_explicit_vedma_ve -Wl,--whole-archive ham_offload_vedma_ve_explicit -Wl,--no-whole-archive)
			set_property(TARGET ham_offload_test_explicit_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(ham_offload_test_explicit_vedma_ve ham_offload_vedma_ve_explicit "${MK_VEORUN_STATIC_LIBS}")

			add_library(hello_nec_dma_vedma_ve hello_nec_dma.cpp) # for static VEO lib
			target_link_libraries(hello_nec_dma_vedma_ve ${HAM_LIB_VEDMA_VE_CLI} veos_system_libraries)
			set_property(TARGET hello_nec_dma_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(hello_nec_dma_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")

			# NOTE: static vs. shared
			# add_library(inner_product_vedma_ve SHARED inner_product.cpp) # for shared VEO lib
			add_library(inner_product_vedma_ve inner_product.cpp) # for static VEO lib
			target_link_libraries(inner_product_vedma_ve ${HAM_LIB_VEDMA_VE_CLI})
			set_property(TARGET inner_product_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(inner_product_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")

			add_library(test_data_transfer_vedma_ve test_data_transfer.cpp)
			target_link_libraries(test_data_transfer_vedma_ve ${HAM_LIB_VEDMA_VE_CLI})
			set_property(TARGET test_data_transfer_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(test_data_transfer_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")

			add_library(test_argument_transfer_vedma_ve test_argument_transfer.cpp)
			target_link_libraries(test_argument_transfer_vedma_ve ${HAM_LIB_VEDMA_VE_CLI})
			set_property(TARGET test_argument_transfer_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(test_argument_transfer_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")

			add_library(test_multiple_targets_vedma_ve test_multiple_targets.cpp)
			target_link_libraries(test_multiple_targets_vedma_ve ${HAM_LIB_VEDMA_VE_CLI})
			set_property(TARGET test_multiple_targets_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
			create_static_veorun_binary(test_multiple_targets_vedma_ve ${HAM_LIB_VEDMA_VE} "${MK_VEORUN_STATIC_LIBS}")
		endif ()

	endif ()

endif () # HAM_BUILD_EXECUTABLES
