project(ham_library CXX)
cmake_minimum_required(VERSION 3.2)

find_package(Boost 1.40 COMPONENTS program_options REQUIRED)
find_package(MPI)

# Dependency libraries

add_library (ham_cxx_std_11 INTERFACE)
target_compile_features (ham_cxx_std_11 INTERFACE cxx_auto_type cxx_range_for cxx_variadic_templates)
target_include_directories (ham_cxx_std_11 INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../include/)

add_library (boost_library INTERFACE)
target_include_directories (boost_library INTERFACE ${Boost_INCLUDE_DIRS})
target_link_libraries (boost_library INTERFACE ${Boost_LIBRARIES})

if (MPI_FOUND)
    add_library (mpi_library INTERFACE)
    target_include_directories (mpi_library INTERFACE ${MPI_CXX_INCLUDE_PATH})
    target_compile_options (mpi_library INTERFACE ${MPI_CXX_COMPILE_FLAGS})
    target_link_libraries (mpi_library INTERFACE ${MPI_CXX_LIBRARIES})
endif ()

find_file(SCIF_HEADER_FILE "scif.h")
if (SCIF_HEADER_FILE)
    set(SCIF_FOUND ON)
    get_filename_component(SCIF_INCLUDE_DIR "${SCIF_HEADER_FILE}" DIRECTORY)
    message(STATUS "Found SCIF: ${SCIF_HEADER_FILE}")

    add_library (scif_library INTERFACE)
    target_include_directories (scif_library INTERFACE ${SCIF_INCLUDE_DIR})
else ()
    message(STATUS "Could NOT find SCIF (missing: scif.h)")
endif ()

#### Libraries

if (MPI_FOUND)
    add_library (ham_offload_mpi # SHARED if BUILD_SHARED_LIBS = TRUE
                 ham/offload/main.cpp
                 ham/net/communicator.cpp
                 ham/net/communicator_mpi.cpp
                 ham/offload/runtime.cpp
                 ham/offload/offload.cpp
                 ham/util/cpu_affinity.cpp)
    target_compile_definitions (ham_offload_mpi PUBLIC -DHAM_COMM_MPI=1)
    target_link_libraries (ham_offload_mpi PUBLIC ham_cxx_std_11 mpi_library boost_library)

    add_library (ham_offload_mpi_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
                 ham/offload/main_explicit.cpp
                 ham/net/communicator.cpp
                 ham/net/communicator_mpi.cpp
                 ham/offload/runtime.cpp
                 ham/offload/offload.cpp
                 ham/util/cpu_affinity.cpp)
    target_compile_definitions (ham_offload_mpi_explicit PUBLIC -DHAM_COMM_MPI=1 -DHAM_EXPLICIT=1)
    target_link_libraries (ham_offload_mpi_explicit PUBLIC ham_cxx_std_11 mpi_library boost_library)
endif ()

#### Benchmarks

# Explicit targets (not built by default)
add_executable (benchmark_intel_leo EXCLUDE_FROM_ALL benchmark_intel_leo.cpp)
target_link_libraries (benchmark_intel_leo)

if (MPI_FOUND)
    add_executable (benchmark_ham_offload_mpi benchmark_ham_offload.cpp)
    target_link_libraries (benchmark_ham_offload_mpi ham_offload_mpi)
endif()

#### Examples/Tests
add_executable (active_msgs active_msgs.cpp)
target_link_libraries (active_msgs ham_cxx_std_11)

if (MPI_FOUND)
    add_executable (ham_offload_test_mpi ham_offload.cpp)
    target_link_libraries (ham_offload_test_mpi ham_offload_mpi)

    add_executable (ham_offload_test_explicit_mpi ham_offload_explicit.cpp)
    target_link_libraries (ham_offload_test_explicit_mpi ham_offload_mpi_explicit)

    add_executable (inner_product_mpi inner_product.cpp)
    target_link_libraries (inner_product_mpi ham_offload_mpi)

    add_executable (test_data_transfer_mpi test_data_transfer.cpp)
    target_link_libraries (test_data_transfer_mpi ham_offload_mpi)

    add_executable (test_argument_transfer_mpi test_argument_transfer.cpp)
    target_link_libraries (test_argument_transfer_mpi ham_offload_mpi)
endif()

#### Libraries

if (SCIF_FOUND)
    add_library (ham_offload_scif # SHARED if BUILD_SHARED_LIBS = TRUE
                 ham/offload/main.cpp
                 ham/net/communicator.cpp
                 ham/net/communicator_scif.cpp
                 ham/offload/runtime.cpp
                 ham/offload/offload.cpp
                 ham/util/cpu_affinity.cpp)
    target_compile_definitions (ham_offload_scif PUBLIC -DHAM_COMM_SCIF=1)
    target_link_libraries (ham_offload_scif_explicit PUBLIC ham_cxx_std_11 boost_library scif_library)

    add_library (ham_offload_scif_explicit # SHARED if BUILD_SHARED_LIBS = TRUE
                 ham/offload/main_explicit.cpp
                 ham/net/communicator.cpp
                 ham/net/communicator_scif.cpp
                 ham/offload/runtime.cpp
                 ham/offload/offload.cpp
                 ham/util/cpu_affinity.cpp)
    target_compile_definitions (ham_offload_scif_explicit PUBLIC -DHAM_COMM_SCIF=1 -DHAM_EXPLICIT=1)
    target_link_libraries (ham_offload_scif_explicit PUBLIC ham_cxx_std_11 boost_library scif_library)
endif ()

#### Benchmarks
if (SCIF_FOUND)
    add_executable (benchmark_ham_offload_scif benchmark_ham_offload.cpp)
    target_link_libraries (benchmark_ham_offload_scif ham_offload_scif)
endif()

#### Examples/Tests

if (SCIF_FOUND)
    add_executable (ham_offload_test_scif ham_offload.cpp)
    target_link_libraries (ham_offload_test_scif ham_offload_scif)

    add_executable (ham_offload_test_explicit_scif ham_offload_explicit.cpp)
    target_link_libraries (ham_offload_test_explicit_scif ham_offload_scif_explicit)

    add_executable (inner_product_scif inner_product.cpp)
    target_link_libraries (inner_product_scif ham_offload_scif)

    add_executable (test_data_transfer_scif test_data_transfer.cpp)
    target_link_libraries (test_data_transfer_scif ham_offload_scif)

    add_executable (test_argument_transfer_scif test_argument_transfer.cpp)
    target_link_libraries (test_argument_transfer_scif ham_offload_scif)
endif()
