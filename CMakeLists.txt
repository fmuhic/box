cmake_minimum_required(VERSION 3.10)
project(Box VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BOX_SOURCES
    src/box/core.c
    src/box/bitset.c
    src/box/darray_core.c
    src/box/spset_core.c
)

set(BOX_HEADERS
    include/box/core.h
    include/box/darray_core.h
    include/box/darray.h
    include/box/hmap_core.h
    include/box/hmap.h
    include/box/bitset.h
    include/box/spset_core.h
    include/box/spset.h
)

add_library(box STATIC 
    ${BOX_SOURCES}
    ${BOX_HEADERS}
)

target_include_directories(box PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

enable_testing()
add_subdirectory(tests)
