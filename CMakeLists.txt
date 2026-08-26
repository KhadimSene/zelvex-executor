cmake_minimum_required(VERSION 3.16)
project(Zelvex VERSION 4.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

find_library(KEYSTONE_LIB keystone)
find_path(KEYSTONE_INCLUDE_DIR keystone/keystone.h)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/memory_manager.cpp
    src/assembler.cpp
    src/loading_screen.cpp
)

set(HEADERS
    src/mainwindow.h
    src/cheat_defs.h
    src/memory_manager.h
    src/assembler.h
    src/styles.h
    src/layout_config.h
    src/loading_screen.h
    src/lua_highlighter.h
    src/app_icons.h
)

qt_add_executable(Zelvex ${SOURCES} ${HEADERS})

set_target_properties(Zelvex PROPERTIES WIN32_EXECUTABLE TRUE)

# Statically link MinGW runtime libraries (libgcc, libstdc++, winpthread, etc.)
# so the exe works on machines without MSYS2/MinGW installed
target_link_options(Zelvex PRIVATE -static-libgcc -static-libstdc++ -static)

target_sources(Zelvex PRIVATE resources.qrc win32.rc)

target_include_directories(Zelvex PRIVATE
    ${KEYSTONE_INCLUDE_DIR}
)

target_link_libraries(Zelvex PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${KEYSTONE_LIB}
    dbghelp
)

# Build the Lua injection DLL (no Qt, no Keystone)
add_subdirectory(dll)

# Copy DLL to output directory after build
add_custom_command(TARGET Zelvex POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/bin/liblua_dll.dll"
        "$<TARGET_FILE_DIR:Zelvex>/lua_dll.dll"
    COMMENT "Copying lua_dll.dll to output directory"
)
