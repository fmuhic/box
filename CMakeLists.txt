cmake_minimum_required(VERSION 3.10)
project(Box VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BOX_SOURCES
    src/box/core.c
    src/box/bitset.c
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

# hmap/spset/bitset keep their cores in .c, so their inline shells still call
# across a TU boundary. Release-only so dev and test builds stay fast.
include(CheckIPOSupported)
check_ipo_supported(RESULT BOX_IPO_SUPPORTED OUTPUT BOX_IPO_ERROR)
if(BOX_IPO_SUPPORTED)
    set_property(TARGET box PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
else()
    message(STATUS "box: IPO/LTO unavailable, skipping: ${BOX_IPO_ERROR}")
endif()

enable_testing()
add_subdirectory(tests)

# Off by default so the normal build stays fast; ./perf.sh turns it on.
option(BOX_BUILD_PERF "Build the performance suite" OFF)
option(BOX_PERF_NATIVE "Build the performance suite with -march=native" ON)
if(BOX_BUILD_PERF)
    add_subdirectory(performance)
endif()
