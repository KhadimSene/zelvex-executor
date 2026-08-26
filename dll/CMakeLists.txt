cmake_minimum_required(VERSION 3.16)
project(lua_dll)

# Build as a 32-bit DLL (game is 32-bit WoW64)
set(CMAKE_C_COMPILER "C:/msys64/mingw32/bin/i686-w64-mingw32-gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/mingw32/bin/i686-w64-mingw32-g++.exe")

# Lua 5.1.5 core - compiled directly into the DLL (real Lua VM).
# Exclude the standalone interpreter main (lua.c), compiler tool (luac.c)
# and the bytecode dumper (print.c).
file(GLOB LUA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.1.5/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/luac\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/print\\.c$")

# Dear ImGui core + DX11/Win32 backends (compiled straight into the DLL)
file(GLOB IMGUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui*.cpp")
list(FILTER IMGUI_SOURCES EXCLUDE REGEX "imgui_demo")

add_library(lua_dll SHARED lua_dll.cpp game_overlay.cpp ${LUA_SOURCES} ${IMGUI_SOURCES})

target_compile_definitions(lua_dll PRIVATE LUA_DLL_EXPORTS)
target_include_directories(lua_dll PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.1.5"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

# 32-bit flags
target_compile_options(lua_dll PRIVATE -m32)
target_link_options(lua_dll PRIVATE -m32 -static)
target_link_libraries(lua_dll PRIVATE d3d11 dxgi d3dcompiler dwmapi wininet)

# Output to the build directory
set_target_properties(lua_dll PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
