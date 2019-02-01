# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham_exe LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

add_subdirectory(ham)

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


# VEO Backend
if (VEO_FOUND)
	# https://stackoverflow.com/questions/5693405/specifying-link-flags-for-only-one-static-lib-while-linking-executable
	# hams static lib contains symbols needed for VEO calls
	SET(HAMLIB_VE -Wl,--whole-archive ham_offload_veo_ve -Wl,--no-whole-archive)

	# Vector Host executables
	if (NOT HAM_NEC_COMPILER_DETECTED)
		# HAM benchmark
		add_executable(benchmark_ham_offload_veo_vh benchmark_ham_offload.cpp)
		target_link_libraries(benchmark_ham_offload_veo_vh ham_offload_veo_vh)

		# VEO benchmark
		add_executable(benchmark_veo_vh benchmark_veo.cpp)
		target_link_libraries(benchmark_veo_vh veo_library cli11_library noma_bmt)

		set_target_properties(benchmark_veo_vh PROPERTIES
			CXX_STANDARD 11
			CXX_STANDARD_REQUIRED YES
			CXX_EXTENSIONS NO)
	endif ()

	# Vector Engine executables

	# HAM Benchmark
	add_library(benchmark_ham_offload_veo_ve benchmark_ham_offload.cpp)
	target_link_libraries(benchmark_ham_offload_veo_ve ${HAMLIB_VE})
	set_property(TARGET benchmark_ham_offload_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	# for static VE build
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET benchmark_ham_offload_veo_ve
			               POST_BUILD
			               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_benchmark_ham_offload ./libbenchmark_ham_offload_veo_ve.a ./ham/libham_offload_veo_ve.a)
	endif ()

	# VEO benchmark
	add_library(benchmark_veo_ve benchmark_veo_lib.cpp)
	set_property(TARGET benchmark_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	# for static VE build
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET benchmark_veo_ve
			               POST_BUILD
			               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_benchmark_veo ./libbenchmark_veo_ve.a)
	endif ()

endif ()


# VEO + DMA Backend
if (VEO_FOUND)
	# https://stackoverflow.com/questions/5693405/specifying-link-flags-for-only-one-static-lib-while-linking-executable
	# hams static lib contains symbols needed for VEO calls
	SET(HAMLIB_VE -Wl,--whole-archive ham_offload_vedma_ve -Wl,--no-whole-archive)

	# Vector Host executables
	if (NOT HAM_NEC_COMPILER_DETECTED)
		# HAM benchmark
		add_executable(benchmark_ham_offload_vedma_vh benchmark_ham_offload.cpp)
		target_link_libraries(benchmark_ham_offload_vedma_vh ham_offload_vedma_vh)

		set_target_properties(benchmark_ham_offload_vedma_vh PROPERTIES
			CXX_STANDARD 11
			CXX_STANDARD_REQUIRED YES
			CXX_EXTENSIONS NO)
	endif ()

	# Vector Engine executables
	if (HAM_NEC_COMPILER_DETECTED)
		# HAM Benchmark
		add_library(benchmark_ham_offload_vedma_ve benchmark_ham_offload.cpp)
		target_link_libraries(benchmark_ham_offload_vedma_ve ${HAMLIB_VE})
		set_property(TARGET benchmark_ham_offload_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		# for static VE build
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET benchmark_ham_offload_vedma_ve
					           POST_BUILD
					           COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_benchmark_ham_offload_vedma ./libbenchmark_ham_offload_vedma_ve.a ./ham/libham_offload_vedma_ve.a)
		endif ()
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

	add_executable(hello_world_mpi hello_world.cpp)
	target_link_libraries(hello_world_mpi ham_offload_mpi)

	add_executable(inner_product_mpi inner_product.cpp)
	target_link_libraries(inner_product_mpi ham_offload_mpi)

	add_executable(test_data_transfer_mpi test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_mpi ham_offload_mpi)

	add_executable(test_argument_transfer_mpi test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_mpi ham_offload_mpi)
endif ()

if (SCIF_FOUND)
	add_executable(ham_offload_test_scif ham_offload.cpp)
	target_link_libraries(ham_offload_test_scif ham_offload_scif)

	add_executable(ham_offload_test_explicit_scif ham_offload_explicit.cpp)
	target_link_libraries(ham_offload_test_explicit_scif ham_offload_scif_explicit)

	add_executable(hello_world_scif hello_world.cpp)
	target_link_libraries(hello_world_scif ham_offload_scif)

	add_executable(inner_product_scif inner_product.cpp)
	target_link_libraries(inner_product_scif ham_offload_scif)

	add_executable(test_data_transfer_scif test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_scif ham_offload_scif)

	add_executable(test_argument_transfer_scif test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_scif ham_offload_scif)
endif ()

# VEO Backend
if (VEO_FOUND)
	SET(HAMLIB_VE -Wl,--whole-archive ham_offload_veo_ve -Wl,--no-whole-archive)
	# Vector Host Executables
	if (NOT HAM_NEC_COMPILER_DETECTED)
		add_executable(ham_offload_test_veo_vh ham_offload.cpp)
		target_link_libraries(ham_offload_test_veo_vh ham_offload_veo_vh)

		add_executable(ham_offload_test_explicit_veo_vh ham_offload_explicit.cpp)
		target_link_libraries(ham_offload_test_explicit_veo_vh ham_offload_veo_vh_explicit)

		add_executable(hello_nec_dma_veo_vh hello_nec_dma.cpp)
		target_link_libraries(hello_nec_dma_veo_vh ham_offload_veo_vh)

		add_executable(hello_world_veo_vh hello_world.cpp)
		target_link_libraries(hello_world_veo_vh ham_offload_veo_vh)

		add_executable(inner_product_veo_vh inner_product.cpp)
		target_link_libraries(inner_product_veo_vh ham_offload_veo_vh)

		add_executable(test_data_transfer_veo_vh test_data_transfer.cpp)
		target_link_libraries(test_data_transfer_veo_vh ham_offload_veo_vh)

		add_executable(test_argument_transfer_veo_vh test_argument_transfer.cpp)
		target_link_libraries(test_argument_transfer_veo_vh ham_offload_veo_vh)
	endif ()
	
	# Vector Engine Libraries
	add_library(ham_offload_test_veo_ve ham_offload.cpp)
	target_link_libraries(ham_offload_test_veo_ve ${HAMLIB_VE})
	set_property(TARGET ham_offload_test_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	# for static VE build
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET ham_offload_test_veo_ve
	    	               POST_BUILD
	    	               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_ham_offload_test ./libham_offload_test_veo_ve.a ./ham/libham_offload_veo_ve.a)
	endif ()

#	add_library(ham_offload_test_explicit_veo_ve ham_offload_explicit.cpp)
#	target_link_libraries(ham_offload_test_explicit_veo_ve ham_offload_veo_ve_explicit)
#	set_property(TARGET ham_offload_test_explicit_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
#	# for static VE build
#	add_custom_command(TARGET benchmark_ham_offload_veo_ve
#	                   POST_BUILD
#	                   COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_ham_offload_test_explicit ./libham_offload_test_explicit_veo_ve.a ./ham/libham_offload_veo_ve.a)

	# will not build on host
	if (HAM_NEC_COMPILER_DETECTED)
		add_library(hello_nec_dma_veo_ve hello_nec_dma.cpp) # for static VEO lib
		target_link_libraries(hello_nec_dma_veo_ve ${HAMLIB_VE} veos_musl_library)
		set_property(TARGET hello_nec_dma_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET hello_nec_dma_veo_ve
					           POST_BUILD
					           COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_hello_dma ./libhello_nec_dma_veo_ve.a ./ham/libham_offload_veo_ve.a) # /opt/nec/ve/musl/lib/libsysve.a)
		endif ()
	endif ()

	add_library(hello_world_veo_ve hello_world.cpp) # for static VEO lib
	target_link_libraries(hello_world_veo_ve ${HAMLIB_VE})
	set_property(TARGET hello_world_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET hello_world_veo_ve
			               POST_BUILD
			               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_hello_world ./libhello_world_veo_ve.a ./ham/libham_offload_veo_ve.a)
	endif ()

# NOTE: static vs. shared
#	add_library(inner_product_veo_ve SHARED inner_product.cpp) # for shared VEO lib
	add_library(inner_product_veo_ve inner_product.cpp) # for static VEO lib
	target_link_libraries(inner_product_veo_ve ${HAMLIB_VE})
	set_property(TARGET inner_product_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET inner_product_veo_ve
			               POST_BUILD
			               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_inner_product ./libinner_product_veo_ve.a ./ham/libham_offload_veo_ve.a)
	endif ()

	add_library(test_data_transfer_veo_ve test_data_transfer.cpp)
	target_link_libraries(test_data_transfer_veo_ve ${HAMLIB_VE})
	set_property(TARGET test_data_transfer_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET test_data_transfer_veo_ve
			               POST_BUILD
			               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_test_data_transfer ./libtest_data_transfer_veo_ve.a ./ham/libham_offload_veo_ve.a)
	endif ()

	add_library(test_argument_transfer_veo_ve test_argument_transfer.cpp)
	target_link_libraries(test_argument_transfer_veo_ve ${HAMLIB_VE})
	set_property(TARGET test_argument_transfer_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	if (HAM_NEC_COMPILER_DETECTED)
		add_custom_command(TARGET test_argument_transfer_veo_ve
			               POST_BUILD
			               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_test_argument_transfer ./libtest_argument_transfer_veo_ve.a ./ham/libham_offload_veo_ve.a)
	endif ()
	
endif ()


# VEO + DMA Backend
if (VEO_FOUND)
	SET(HAMLIB_VE -Wl,--whole-archive ham_offload_vedma_ve -Wl,--no-whole-archive)
	# Vector Host Executables
	if (NOT HAM_NEC_COMPILER_DETECTED)
		add_executable(ham_offload_test_vedma_vh ham_offload.cpp)
		target_link_libraries(ham_offload_test_vedma_vh ham_offload_vedma_vh)

		add_executable(ham_offload_test_explicit_vedma_vh ham_offload_explicit.cpp)
		target_link_libraries(ham_offload_test_explicit_vedma_vh ham_offload_vedma_vh_explicit)

		add_executable(hello_nec_dma_vedma_vh hello_nec_dma.cpp)
		target_link_libraries(hello_nec_dma_vedma_vh ham_offload_vedma_vh)

		add_executable(hello_world_vedma_vh hello_world.cpp)
		target_link_libraries(hello_world_vedma_vh ham_offload_vedma_vh)

		add_executable(inner_product_vedma_vh inner_product.cpp)
		target_link_libraries(inner_product_vedma_vh ham_offload_vedma_vh)

		add_executable(test_data_transfer_vedma_vh test_data_transfer.cpp)
		target_link_libraries(test_data_transfer_vedma_vh ham_offload_vedma_vh)

		add_executable(test_argument_transfer_vedma_vh test_argument_transfer.cpp)
		target_link_libraries(test_argument_transfer_vedma_vh ham_offload_vedma_vh)
	endif ()

	# Vector Engine Libraries
	if (HAM_NEC_COMPILER_DETECTED)
		add_library(ham_offload_test_vedma_ve ham_offload.cpp)
		target_link_libraries(ham_offload_test_vedma_ve ${HAMLIB_VE})
		set_property(TARGET ham_offload_test_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		# for static VE build
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET ham_offload_test_vedma_ve
				               POST_BUILD
				               COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_ham_offload_test_vedma ./libham_offload_test_vedma_ve.a ./ham/libham_offload_vedma_ve.a)
		endif ()

	#	add_library(ham_offload_test_explicit_vedma_ve ham_offload_explicit.cpp)
	#	target_link_libraries(ham_offload_test_explicit_vedma_ve ham_offload_vedma_ve_explicit)
	#	set_property(TARGET ham_offload_test_explicit_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
	#	# for static VE build
	#	add_custom_command(TARGET benchmark_ham_offload_vedma_ve
	#	                   POST_BUILD
	#	                   COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_ham_offload_test_explicit ./libham_offload_test_explicit_vedma_ve.a ./ham/libham_offload_vedma_ve.a)

		# will not build on host
		add_library(hello_nec_dma_vedma_ve hello_nec_dma.cpp) # for static VEO lib
		target_link_libraries(hello_nec_dma_vedma_ve ${HAMLIB_VE} veos_musl_library)
		set_property(TARGET hello_nec_dma_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET hello_nec_dma_vedma_ve
						       POST_BUILD
						       COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_hello_dma_vedma ./libhello_nec_dma_vedma_ve.a ./ham/libham_offload_vedma_ve.a) # /opt/nec/ve/musl/lib/libsysve.a)
		endif ()

		add_library(hello_world_vedma_ve hello_world.cpp) # for static VEO lib
		target_link_libraries(hello_world_vedma_ve ${HAMLIB_VE})
		set_property(TARGET hello_world_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET hello_world_vedma_ve
					           POST_BUILD
					           COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_hello_world_vedma ./libhello_world_vedma_ve.a ./ham/libham_offload_vedma_ve.a)
		endif ()

	# NOTE: static vs. shared
	#	add_library(inner_product_vedma_ve SHARED inner_product.cpp) # for shared VEO lib
		add_library(inner_product_vedma_ve inner_product.cpp) # for static VEO lib
		target_link_libraries(inner_product_vedma_ve ${HAMLIB_VE})
		set_property(TARGET inner_product_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET inner_product_vedma_ve
					           POST_BUILD
					           COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_inner_product_vedma ./libinner_product_vedma_ve.a ./ham/libham_offload_vedma_ve.a)
		endif ()

		add_library(test_data_transfer_vedma_ve test_data_transfer.cpp)
		target_link_libraries(test_data_transfer_vedma_ve ${HAMLIB_VE})
		set_property(TARGET test_data_transfer_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET test_data_transfer_vedma_ve
					           POST_BUILD
					           COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_test_data_transfer_vedma ./libtest_data_transfer_vedma_ve.a ./ham/libham_offload_vedma_ve.a)
		endif ()

		add_library(test_argument_transfer_vedma_ve test_argument_transfer.cpp)
		target_link_libraries(test_argument_transfer_vedma_ve ${HAMLIB_VE})
		set_property(TARGET test_argument_transfer_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)
		if (HAM_NEC_COMPILER_DETECTED)
			add_custom_command(TARGET test_argument_transfer_vedma_ve
					           POST_BUILD
					           COMMAND /opt/nec/ve/libexec/mk_veorun_static ../veorun_test_argument_transfer_vedma ./libtest_argument_transfer_vedma_ve.a ./ham/libham_offload_vedma_ve.a)
		endif ()
	endif ()
endif ()
