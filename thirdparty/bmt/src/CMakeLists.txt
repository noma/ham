# Copyright (c) 2017 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

# do not put executable into subdir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (NOMA_BMT_BUILD_EXAMPLES)
	# simpel example application measuring overhead
	add_executable(example example.cpp)
	target_link_libraries(example noma_bmt)
endif ()

