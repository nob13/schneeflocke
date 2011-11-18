# An command line application
macro (add_app name)
	message (STATUS " - Adding: ${name}")
	file (GLOB_RECURSE src_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${name}/*.cpp)
	file (GLOB_RECURSE header_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${name}/*.h)
	add_executable (${name} ${src_files})
	target_link_libraries (${name} ${LIBS})
	if (DEPLOY_COMMANDLINE)
		install (TARGETS ${name} RUNTIME DESTINATION bin)
	endif()
endmacro ()

# Interactive Programs
add_app (im_cmdline_client)
add_app (sf_cmdline_sharer)

# "Real" Programs
add_subdirectory (schneeflocke)
