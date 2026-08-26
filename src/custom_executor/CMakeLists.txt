cmake_minimum_required(VERSION 3.16)
project(lua_full)

# Build as a 32-bit DLL (game is 32-bit WoW64)
set(CMAKE_C_COMPILER "C:/msys64/mingw32/bin/i686-w64-mingw32-gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/mingw32/bin/i686-w64-mingw32-g++.exe")

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Compiler options for 32-bit
add_compile_options(-m32 -Wall -Wextra -std=c++11 -fPIC -O2)
add_link_options(-m32 -static-libgcc -static-libstdc++)

# Source files for the custom executor
file(GLOB_RECURSE EXECUTOR_SOURCES
    "src/custom_executor/lua_full.cpp"
    "src/custom_executor/lua_full.h"
    "src/custom_executor/lua_full_def.h"
    "src/custom_executor/lua_full_helper.cpp"
)

# Main executor library
add_library(lua_full SHARED ${EXECUTOR_SOURCES})

# Set output name
set_target_properties(lua_full PROPERTIES
    OUTPUT_NAME "lua_full"
    SUFFIX ".dll"
    PREFIX ""
)

# Include directories for the target
target_include_directories(lua_full PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(lua_full kernel32 psapi)

# Installation
install(TARGETS lua_full
    RUNTIME DESTINATION "."
    LIBRARY DESTINATION "."
    ARCHIVE DESTINATION "."
)

# Create a build script
add_custom_target(build_executor
    COMMAND echo "Building Custom Lua Executor..."
    COMMAND "${CMAKE_COMMAND}" -E cmake_echo_error "Build complete. Output in bin/lua_full.dll"
    DEPENDS lua_full
)

# Documentation target
add_custom_target(docs
    COMMAND mkdir -p "docs"
    COMMAND "${CMAKE_COMMAND}" -E echo "Documentation for Custom Lua Executor"
    COMMAND "${CMAKE_COMMAND}" -E echo "See lua_full.h for API documentation"
    COMMAND "${CMAKE_COMMAND}" -E echo "See lua_full_def.h for function definitions"
    COMMAND "${CMAKE_COMMAND}" -E echo "See examples in lua_full.cpp for usage"
    COMMENT "Generate documentation"
)

# Clean target
add_custom_target(clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory "bin"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "lib"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "docs"
    COMMENT "Clean build directories"
)

message(STATUS "Custom Lua Executor project configured")
message(STATUS "Build type: 32-bit DLL")
message(STATUS "Output: bin/lua_full.dll")
