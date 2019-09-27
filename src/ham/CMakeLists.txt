# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham_lib LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

# interface target for ham
add_library(ham_interface INTERFACE)
target_compile_features(ham_interface INTERFACE cxx_auto_type cxx_range_for cxx_variadic_templates)
target_link_libraries(ham_interface INTERFACE noma_bmt boost_library cli11_library)
target_include_directories(ham_interface INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../../include)

set(HAM_LIB_SRC
	misc/options.cpp
	net/communicator.cpp
	offload/runtime.cpp
	offload/offload.cpp
	util/cpu_affinity.cpp)

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

	set_target_properties(ham_offload_mpi ham_offload_mpi_explicit PROPERTIES
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
	target_link_libraries(ham_offload_scif PUBLIC ham_interface scif_library)

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

# NEC VEO backend
if (VEO_FOUND)
# TODO: remove -msched=none, set to reduce compile time with NCC
	SET(HAM_LIB_COMMON_CFLAGS $<$<CONFIG:DEBUG>:HAM_DEBUG_ON> -DHAM_LOG_NODE_PREFIX -DHAM_COMM_VEO_STATIC) # for static VEO lib
#	SET(HAM_LIB_COMMON_CFLAGS $<$<CONFIG:DEBUG>:HAM_DEBUG_ON> -DHAM_LOG_NODE_PREFIX) # for shared VEO lib

	# Vector Host libraries
	if (NOT HAM_NEC_COMPILER_DETECTED)	
		add_library(ham_offload_veo_vh # SHARED if BUILD_SHARED_LIBS = TRUE
			        ${HAM_LIB_SRC}
			        offload/main.cpp
			        net/communicator_veo.cpp net/communicator_veo_vh.cpp)
		target_compile_definitions(ham_offload_veo_vh PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEO=0) # NOTE: 0 is for vector host
		target_link_libraries(ham_offload_veo_vh PUBLIC ham_interface veo_library)

		add_library(ham_offload_veo_vh_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
			        ${HAM_LIB_SRC}
			        offload/main_explicit.cpp
			        net/communicator_veo.cpp net/communicator_veo_vh.cpp)
		target_compile_definitions(ham_offload_veo_vh_explicit PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEO=0 -DHAM_EXPLICIT=1) # NOTE = 0 is for vector host
		target_link_libraries(ham_offload_veo_vh_explicit PUBLIC ham_interface veo_library)

		set_target_properties(ham_offload_veo_vh ham_offload_veo_vh_explicit PROPERTIES
			CXX_STANDARD 11
			CXX_STANDARD_REQUIRED YES
			CXX_EXTENSIONS NO)
	endif ()

	# Vector Engine libraries
	add_library(ham_offload_veo_ve # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main.cpp
	            net/communicator_veo.cpp net/communicator_veo_ve.cpp)
	target_compile_definitions(ham_offload_veo_ve PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEO=1) # NOTE: 1 is for vector engine
	target_link_libraries(ham_offload_veo_ve PUBLIC ham_interface) # NOTE: no VEO dependency

	add_library(ham_offload_veo_ve_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
	            ${HAM_LIB_SRC}
	            offload/main_explicit.cpp
	            net/communicator_veo.cpp net/communicator_veo_ve.cpp)
	target_compile_definitions(ham_offload_veo_ve_explicit PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEO=1 -DHAM_EXPLICIT=1) # NOTE = 1 is for vector engine
	target_link_libraries(ham_offload_veo_ve_explicit PUBLIC ham_interface) # NOTE: no VEO dependency

#	set_property(TARGET ham_offload_veo_ve PROPERTY POSITION_INDEPENDENT_CODE ON)

	set_target_properties(ham_offload_veo_ve ham_offload_veo_ve_explicit PROPERTIES POSITION_INDEPENDENT_CODE ON)

	set_target_properties(ham_offload_veo_ve ham_offload_veo_ve_explicit PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO)
endif ()


# NEC VEO + DMA Backend
if (VEO_FOUND)
# TODO: remove -msched=none, set to reduce compile time with NCC
	SET(HAM_LIB_COMMON_CFLAGS $<$<CONFIG:DEBUG>:HAM_DEBUG_ON> -DHAM_LOG_NODE_PREFIX -DHAM_COMM_VEO_STATIC) # for static VEO lib
#	SET(HAM_LIB_COMMON_CFLAGS $<$<CONFIG:DEBUG>:HAM_DEBUG_ON> -DHAM_LOG_NODE_PREFIX) # for shared VEO lib

	# Vector Host libraries
	if (NOT HAM_NEC_COMPILER_DETECTED)
		add_library(ham_offload_vedma_vh # SHARED if BUILD_SHARED_LIBS = TRUE
			        ${HAM_LIB_SRC}
			        offload/main.cpp
			        net/communicator_vedma.cpp net/communicator_vedma_vh.cpp)
		target_compile_definitions(ham_offload_vedma_vh PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEDMA=0) # NOTE: 0 is for vector host
		target_link_libraries(ham_offload_vedma_vh PUBLIC ham_interface veo_library)

		add_library(ham_offload_vedma_vh_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
			        ${HAM_LIB_SRC}
			        offload/main_explicit.cpp
			        net/communicator_vedma.cpp net/communicator_vedma_vh.cpp)
		target_compile_definitions(ham_offload_vedma_vh_explicit PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEDMA=0 -DHAM_EXPLICIT=1) # NOTE = 0 is for vector host
		target_link_libraries(ham_offload_vedma_vh_explicit PUBLIC ham_interface veo_library)

		set_target_properties(ham_offload_vedma_vh ham_offload_vedma_vh_explicit PROPERTIES
			CXX_STANDARD 11
			CXX_STANDARD_REQUIRED YES
			CXX_EXTENSIONS NO)
	endif ()

	# Vector Engine libraries
	if (HAM_NEC_COMPILER_DETECTED)
		add_library(ham_offload_vedma_ve # SHARED if BUILD_SHARED_LIBS = TRUE
			        ${HAM_LIB_SRC}
			        offload/main.cpp
			        net/communicator_vedma.cpp net/communicator_vedma_ve.cpp)
		target_compile_definitions(ham_offload_vedma_ve PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEDMA=1) # NOTE: 1 is for vector engine
		target_link_libraries(ham_offload_vedma_ve PUBLIC ham_interface veos_system_libraries) # NOTE: no VEO dependency

		add_library(ham_offload_vedma_ve_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
			        ${HAM_LIB_SRC}
			        offload/main_explicit.cpp
			        net/communicator_vedma.cpp net/communicator_vedma_ve.cpp)
		target_compile_definitions(ham_offload_vedma_ve_explicit PUBLIC ${HAM_LIB_COMMON_CFLAGS} -DHAM_COMM_VEDMA=1 -DHAM_EXPLICIT=1) # NOTE = 1 is for vector engine
		target_link_libraries(ham_offload_vedma_ve_explicit PUBLIC ham_interface) # NOTE: no VEO dependency

	#	set_property(TARGET ham_offload_vedma_ve PROPERTY POSITION_INDEPENDENT_CODE ON)

		set_target_properties(ham_offload_vedma_ve ham_offload_vedma_ve_explicit PROPERTIES POSITION_INDEPENDENT_CODE ON)

		set_target_properties(ham_offload_vedma_ve ham_offload_vedma_ve_explicit PROPERTIES
			CXX_STANDARD 11
			CXX_STANDARD_REQUIRED YES
			CXX_EXTENSIONS YES) # wen need --std=gnu++11 for inline assembly
	endif ()
endif ()

