cmake_minimum_required (VERSION 2.6)
project (Schneeflocke)
set (SF_VERSION "0.5.5pre")
add_definitions (-DSF_VERSION="${SF_VERSION}")


# set (CMAKE_VERBOSE_MAKEFILE ON)
enable_testing () # tests inside testcases 

# other modules
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Special ENABLE_DEPLOY flag to optimize it for building packages/setups
if (NOT DEFINED ENABLE_DEPLOY)
	set (ENABLE_DEPLOY FALSE)
endif()
if (ENABLE_DEPLOY)
	set (LIBRARY_FLAGS STATIC)
	if (NOT CMAKE_BUILD_TYPE)
		set (CMAKE_BUILD_TYPE RELEASE)
	endif()
	set (ENABLE_TESTCASES FALSE)
endif()

# Auto Debug if no build type is set
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE "DEBUG")
endif()
if (NOT DEFINED ENABLE_TESTCASES)
	set (ENABLE_TESTCASES TRUE)
endif()

# Platform specific
if (WIN32)
	message (STATUS "Machine: Win32")
	set (TEMP $ENV{BOOST_ROOT})
	set (Boost_ADDITIONAL_VERSIONS "1.41")
	if (TEMP)
		set (BOOST_ROOT $ENV{BOOST_ROOT})
		message (STATUS "Setting BOOST_ROOT to ${BOOST_ROOT}")
	endif()
	# Boost ASIO Needs a Windows Target (using Windows XP)
	add_definitions ("-D_WIN32_WINNT=0x0501")
	# Enabling ASIO Operation Cancelling on Win < Vista DANGEROUS!!
	message (STATUS "Enabling dangerous IO cancelling in Windows")
	add_definitions ("-DBOOST_ASIO_ENABLE_CANCELIO")
	# No deprecated C-lib Function Warnings
	add_definitions ("-D_CRT_SECURE_NO_WARNINGS")
	add_definitions ("-D_SCL_SECURE_NO_WARNINGS")

	set (LIBRARY_FLAGS "STATIC") # SHARED doesn't work in Win32 at the moment.

elseif (APPLE)
	set (MAC_OSX TRUE)
	message (STATUS "Machine: MacOSX")
else ()
	set (LINUX TRUE)
	message (STATUS "Machine: Linux")

	if (DEFINED BOOST_ROOT)
		set (Boost_LIBRARIES ${BOOST_ROOT}/lib/libboost_filesystem.a ${BOOST_ROOT}/lib/libboost_system.a ${BOOST_ROOT}/lib/libboost_thread.a)
		set (Boost_INCLUDE_DIRS ${BOOST_ROOT}/include)
		set (Boost_FOUND TRUE)
		message (STATUS "Faking boost existance to ${BOOST_ROOT}")
	endif()

	
	add_definitions ("-DLINUX")
	set (CMAKE_CXX_FLAGS_DEBUG      "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -Wall")	
	set (CMAKE_C_FLAGS_DEBUG        "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG -Wall")

	set (CMAKE_CXX_FLAGS_RELEASE    "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
	set (CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -Wall")

endif()

# Packaging Settings
set (CPACK_PACKAGE_VERSION ${SF_VERSION})
if (WIN32)
	SET(CPACK_GENERATOR "ZIP" "NSIS")
	set(CPACK_PACKAGE_EXECUTABLES "schneeflocke;Schneeflocke")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/license.txt")
endif()
if (LINUX)
	SET(CPACK_SOURCE_GENERATOR "TGZ" "DEB")
	SET(CPACK_GENERATOR "TGZ" "DEB")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Friend-2-Friend File Sharing tool.")
	set(CPACK_PACKAGE_CONTACT "sflx.net Authors <package@sflx.net>")
	set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
	set(CPACK_STRIP_FILES TRUE)
	# Debian Hacks
	# Getting right Naming for Deb file
	# See http://www.cmake.org/pipermail/cmake/2010-July/038243.html
	string(TOLOWER "${CPACK_PACKAGE_NAME}" CPACK_PACKAGE_NAME_LOWERCASE)
	find_program(DPKG_PROGRAM dpkg DOC "dpkg program of Debian-based systems")
	if(DPKG_PROGRAM)
		execute_process(
			COMMAND ${DPKG_PROGRAM} --print-architecture
			OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		set(CPACK_PACKAGE_FILE_NAME
		"${CPACK_PACKAGE_NAME_LOWERCASE}_${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")   
	else()
		set(CPACK_PACKAGE_FILE_NAME
		"${CPACK_PACKAGE_NAME_LOWERCASE}_${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")
	endif(DPKG_PROGRAM)
endif()
if (MAC_OSX)
	#TODO
	SET(CPACK_GENERATOR "ZIP")
endif()

include (CPack)
include (InstallRequiredSystemLibraries)
message (STATUS "Generators: ${CPACK_GENERATOR}")

if (NOT DEFINED DEPLOY_COMMANDLINE)
	set (DEPLOY_COMMANDLINE FALSE)
endif()


if (NOT DEFINED LIBRARY_FLAGS)
	set (LIBRARY_FLAGS SHARED)
endif()

message (STATUS "Default Build Type     ${CMAKE_BUILD_TYPE}")
message (STATUS "C++ Debug Flags        ${CMAKE_CXX_FLAGS_DEBUG}")
message (STATUS "C++ Release Flags      ${CMAKE_CXX_FLAGS_RELEASE}")
message (STATUS "ENABLE_DEPLOY          ${ENABLE_DEPLOY}")
message (STATUS "ENABLE_TESTCASES       ${ENABLE_TESTCASES}")
message (STATUS "LIBRARY_FLAGS          ${LIBRARY_FLAGS}")
message (STATUS "DEPLOY_COMMANDLINE     ${DEPLOY_COMMANDLINE}")

message (STATUS "*** Dependencies ***")
	message (STATUS " - external libs")
		if (WIN32)
			add_definitions ("-DUDT_STATIC") # otherwise we get linking errors
		endif()
		include_directories ("external")
		add_subdirectory (external)
		if (LINUX OR MAC_OSX)
			set (LIBS ${LIBS} "linenoise")
		endif()
		set (LIBS ${LIBS} "udt4")

	message (STATUS " - boost")
		set (Boost_USE_STATIC_LIBS TRUE)
		
		if (NOT Boost_FOUND) # maybe faked
			find_package (Boost 1.40.0 COMPONENTS thread system date_time filesystem)
		endif()
		if (Boost_FOUND)
			message (STATUS "Boost Dir: ${Boost_INCLUDE_DIRS}")
			set (LIBS ${LIBS} ${Boost_LIBRARIES})
			message (STATUS "Boost Libs:         ${Boost_LIBRARIES}")
			message (STATUS "Boost Library Dirs: ${Boost_LIBRARY_DIRS}")
			message (STATUS "Boost Ver:          ${Boost_VERSION}")
			include_directories (${Boost_INCLUDE_DIRS})
			link_directories (${Boost_LIBRARY_DIRS})
			
			# TODO: Port to version 3
			add_definitions (-DBOOST_FILESYSTEM_VERSION=2)
		else()
			message (FATAL_ERROR "Boost not found")
		endif()
		
	message (STATUS " - sfserialization")
		find_package (SFSerialization REQUIRED)
		include_directories (${SFSerialization_INCLUDE_DIR})
		set (LIBS ${LIBS} ${SFSerialization_LIBRARY})
	
	message (STATUS " - libtls")
	if (NOT WIN32)
		find_package (GnuTLS REQUIRED)
		set (GNUTLS_LIBRARIES ${GNUTLS_LIBRARIES} gcrypt) # hack for laptop
	else ()
		set (tlsdir "${CMAKE_SOURCE_DIR}/external/gnutls_win/")
		set (GNUTLS_INCLUDE_DIR "${tlsdir}/include")
		set (GNUTLS_LIBRARIES   "${tlsdir}/bin/libgnutls-26.lib" "${tlsdir}/bin/libgcrypt-11.lib")
		file (GLOB GNUTLS_SHARED_LIBRARIES ${tlsdir}/bin/*.dll)
		message (STATUS "GnuTLS DLL Files: ${blub}")
		add_definitions ("-Dssize_t=long") # hack
		install (FILES ${GNUTLS_SHARED_LIBRARIES} DESTINATION bin/) # install GnuTLS DLL files.
	endif()
	include_directories (${GNUTLS_INCLUDE_DIR})
	set (LIBS ${LIBS} ${GNUTLS_LIBRARIES})
	message (STATUS "GnuTLS LIBRARIES:   ${GNUTLS_LIBRARIES}")
	message (STATUS "GnuTLS INCLUDE_DIR: ${GNUTLS_INCLUDE_DIR}")
	
	if (NOT WIN32)
		message (STATUS " - pthread")
			set (LIBS ${LIBS} pthread)
	endif()
	

message (STATUS "*** libschnee ***")
	include_directories ("libschnee/")
	add_subdirectory (libschnee)
	set (LIBS "schnee" ${LIBS})

message (STATUS "*** libflocke ***")
	include_directories ("libflocke/")
	add_subdirectory (libflocke)
	set (LIBS "flocke" ${LIBS})

message (STATUS "*** Applications ***")
	add_subdirectory (apps)
	
message (STATUS "*** libschnee (test support library) ***")
	include_directories ("libschneetest")
	add_subdirectory (libschneetest)
	set (LIBS "schneetest" ${LIBS})

if (ENABLE_TESTCASES)
	message (STATUS "*** testcases ***")
		add_subdirectory (testcases)
endif()
	

