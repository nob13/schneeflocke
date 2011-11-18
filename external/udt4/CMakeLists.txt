# CMakeLists is not a part of offical UDT4 distribution

# Architecture
if (LINUX OR MAC_OSX)
if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	set (arch AMD64)
else()
	set (arch IA32)
endif()
endif()

if (WIN32) # don't know how windows handle 64bit.
set (arch IA32)
endif()

 
# OS Flag
if (MAC_OSX)
set (os OSX)
endif()
if (LINUX)
set (os LINUX)
endif()
if (WIN32)
add_definitions ("-DUDT_EXPORTS")
set (os WIN32)
endif()

message (STATUS "OS:   ${os}")
message (STATUS "ARCH: ${arch}")
add_definitions ("-D${os}" "-D${arch}")

file (GLOB_RECURSE src_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
file (GLOB_RECURSE header_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
add_library (udt4 ${src_files} ${header_files})
