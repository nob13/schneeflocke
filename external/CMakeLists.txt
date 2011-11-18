# Compiling linenoise in Linux
if (LINUX OR MAC_OSX)
message (STATUS "  Adding liblinenoise")
set (sources "linenoise/linenoise.c")
add_library (linenoise ${sources})
endif()

# udt4 lib
add_subdirectory (udt4)
