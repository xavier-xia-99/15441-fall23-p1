project(Mixnet)
cmake_minimum_required(VERSION 3.14)

# CMake options
option(DEBUG "Enable debugging" OFF)

# Output targets
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# C++ STD version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# C STD version
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_EXTENSIONS True)

# Threads
find_package(Threads REQUIRED)

# Includes
include_directories(.)

# Global compile options
add_compile_options(-pedantic-errors)
add_compile_options(-Wall -Wextra -Werror)

if (DEBUG)
    message("Debugging is ENABLED (to disable, run `cmake .. -DDEBUG=OFF`)")
    add_compile_options(-O0 -g)
else(DEBUG)
    message("Debugging is DISABLED (to enable, run `cmake .. -DDEBUG=ON`)")
    add_compile_options(-O3)
endif(DEBUG)
message("")

# Directories
add_subdirectory(external)
add_subdirectory(framework)
add_subdirectory(mixnet)
add_subdirectory(testing)
