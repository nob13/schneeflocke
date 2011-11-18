# Also use environment variable
set (temp $ENV{SFSERIALIZATION_ROOT})
if (temp)
	if (NOT DEFINED SFSerialization_ROOT)
		set (SFSerialization_ROOT ${temp})
	endif()
endif()
if (NOT DEFINED SFSerialization_ROOT)
	message (STATUS "SFSerialization_ROOT not set, compilation will possibly fail")
	set (SFSerialization_INCLUDE_PREFIX "")
	set (SFSerialization_BIN_PREFIX     "")
	set (SFSerialization_LIB_PREFIX     "")
else()
	set (SFSerialization_INCLUDE_PREFIX ${SFSerialization_ROOT}/include)
	set (SFSerialization_BIN_PREFIX     ${SFSerialization_ROOT}/bin)
	set (SFSerialization_LIB_PREFIX     ${SFSerialization_ROOT}/lib)
endif()

# Check installation

# autoreflect command
message (STATUS "Search for sfautoreflect, prefix= ${SFSerialization_BIN_PREFIX}")
find_program (SFSerialization_AUTOREFLECT_CMD sfautoreflect ${SFSerialization_BIN_PREFIX})
if (SFSerialization_AUTOREFLECT_CMD STREQUAL "SFSerialization_AUTOREFLECT_CMD-NOTFOUND")
	message (FATAL_ERROR "sfautoreflect commad not found!")
endif()


# sfserialization library
find_library (SFSerialization_LIBRARY sfserialization ${SFSerialization_LIB_PREFIX})
if (SFSerialization_LIBRARY STREQUAL "SFSerialization_LIBRARY-NOTFOUND")
	message (FATAL_ERROR "sfserialization library not found!")
endif()

# include directory
find_path (SFSerialization_INCLUDE_DIR sfserialization/Serialization.h ${SFSerialization_INCLUDE_PREFIX})
if (SFSerialization_INCLUDE_DIR STREQUAL "SFSerialization_INCLUDE_DIR-NOTFOUND")
	message (FATAL_ERROR "sfserialization include directory not found!")
endif()

# Success
message (STATUS "  SFSerialization include dir:  ${SFSerialization_INCLUDE_DIR}")
message (STATUS "  SFSerialization library:      ${SFSerialization_LIBRARY}")
message (STATUS "  SFSerialization autoreflect:  ${SFSerialization_AUTOREFLECT_CMD}")

# Tool Macro for automating autoreflect
# Will auto reflect all files in src_files and add the compiled files into the list gen_files
macro (sfautoreflect_decorate gen_files src_files)
	set (listcopy ${ARGV})
	list (REMOVE_AT listcopy 0) # Removing two parameters
	foreach (src ${listcopy})
		set (out "${src}_reflect.cpp")
		
		set (src_path "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
		set (out_path "${CMAKE_CURRENT_BINARY_DIR}/${out}")

		# autoreflect won't create directory by itself
		get_filename_component (out_path_dir ${out_path} PATH)
		file (MAKE_DIRECTORY ${out_path_dir})
		

		add_custom_command (OUTPUT ${out} DEPENDS ${src} 
		COMMAND ${SFSerialization_AUTOREFLECT_CMD} ${src_path} -o ${out_path})
		list (APPEND ${gen_files} ${out})
	endforeach ()
endmacro()

# Looks in the content of a list of files for the SF_AUTOREFLECT_PATTERN 
# and if it is the case, it will process it with autoreflect_decorate
# and adds the generated file to the list
macro (sfautoreflect_decorate_auto files)
	set (original ${${files}})
	foreach (f ${original})
		file (STRINGS ${f} found LIMIT_COUNT 1 REGEX SF_AUTOREFLECT_)
		if (found)
			message (STATUS "   - ${f} contains (problably) reflection code")
			sfautoreflect_decorate (${files} ${f})
		endif() 
	endforeach ()
endmacro ()


set (SFSerialization_FOUND TRUE)






