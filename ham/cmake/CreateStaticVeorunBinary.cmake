# Copyright (c) 2013-2019 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

# see HAM project for usage

if(__create_static_veorun_binary)
	return()
endif()
set(__create_static_veorun_binary YES)

set(CWD_create_static_veorun_binary "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "" FORCE)

set(MK_VEORUN_STATIC_NAME mk_veorun_static)
set(MK_VEORUN_STATIC_DEFAULT_PATH /opt/nec/ve/bin/)
find_program(MK_VEORUN_STATIC_BIN PATHS ${MK_VEORUN_STATIC_DEFAULT_PATH} ENV PATH NAMES ${MK_VEORUN_STATIC_NAME})

message("${MK_VEORUN_STATIC_BIN}")
message("${MK_VEORUN_STATIC_DEFAULT_PATH}")
if(MK_VEORUN_STATIC_BIN STREQUAL "MK_VEORUN_STATIC_BIN-NOTFOUND")
	message(STATUS "'${MK_VEORUN_STATIC_NAME}' not found.")
else()
	message(STATUS "Found ${MK_VEORUN_STATIC_NAME}: ${MK_VEORUN_STATIC_BIN}")
endif()

# Arguments: 
# LIB_TARGET .. CMake target of the statically built library we are using
# DEPENDENCIES .. other CMake library targets LIB_TARGET depends on
# CLI_ARGUMENTS .. passed through command line arguments for mk_veorun_static
function(create_static_veorun_binary LIB_TARGET DEPENDENCIES CLI_ARGUMENTS)
#	set(OUTPUT_FILE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/veorun_${LIB_TARGET}) # the output file we generate
	set(OUTPUT_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/veorun_${LIB_TARGET})
	set(TARGET_NAME mk_veorun_${LIB_TARGET}) # the CMake target we generate

	if (DEPENDENCIES)
		set(DEPENDENCIES_FILE $<TARGET_FILE:${DEPENDENCIES}>)
	endif ()

	# generate veorun binary from static library
	add_custom_command(OUTPUT ${OUTPUT_FILE}
	                   COMMAND ${MK_VEORUN_STATIC_BIN} ${OUTPUT_FILE} $<TARGET_FILE:${LIB_TARGET}> ${DEPENDENCIES_FILE} ${CLI_ARGUMENTS}
	                   DEPENDS ${LIB_TARGET})
	# add a CMake target that depends on the output from above and is built by default
	add_custom_target(${TARGET_NAME} ALL DEPENDS ${OUTPUT_FILE})

endfunction(create_static_veorun_binary)

