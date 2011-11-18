# Building libflocke

file (GLOB_RECURSE src_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} flocke/*.cpp)
file (GLOB_RECURSE header_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} flocke/*.h)

if (WIN32) # Nice Grouping in Visual Studio
   # Found here: http://www.vtk.org/Wiki/CMakeMacroGenerateProject
   SET (all ${src_files} ${header_files})
   SET (ProjectDir ${CMAKE_CURRENT_SOURCE_DIR})
   FOREACH ( Source ${all} )
     STRING ( REGEX REPLACE "${ProjectDir}" "" RelativePath "${Source}" )
     STRING ( REGEX REPLACE "[\\\\/][^\\\\/]*$" "" RelativePath "${RelativePath}" )
     STRING ( REGEX REPLACE "^[\\\\/]" "" RelativePath "${RelativePath}" )
     STRING ( REGEX REPLACE "/" "\\\\\\\\" RelativePath "${RelativePath}" )
     SOURCE_GROUP ( "${RelativePath}" FILES ${Source} )
   ENDFOREACH ( Source )
endif()

set (all_files ${src_files} ${header_files})
sfautoreflect_decorate_auto (all_files)

add_library (flocke ${LIBRARY_FLAGS} ${all_files})
if (${LIBRARY_FLAGS} STREQUAL "SHARED")
	install (TARGETS flocke LIBRARY DESTINATION lib)
endif()

# Documentation

find_package (Doxygen)
if (DOXYGEN_FOUND)
	message (STATUS "  Doxygen found at ${DOXYGEN_EXECUTABLE}")
	set (DOXYGEN_ENABLED_SECTIONS "")
	set (DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/apidoc_flocke")
	configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
	set (DOXYGEN_ENABLED_SECTIONS "DEV")
	set (DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/apidoc_flocke_dev")
	configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_dev)
	add_custom_target (
		apidoc_flocke # name of the target 
		COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Doxygen generated API documentation")
	add_custom_target (
		apidoc_flocke_dev # name of the target 
		COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_dev
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Doxygen generated API documentation (Developer Version)")
	else()
		message (STATUS "  Doxygen not found, skipping documentation")
endif()

