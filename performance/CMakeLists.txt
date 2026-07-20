set(BOX_PERF_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# The C++ comparisons need a C++ compiler, but the library is C99 and the root
# project declares LANGUAGES C. Enabling CXX here rather than at the top means
# the requirement only exists when BOX_BUILD_PERF is on -- a normal
# `cmake -B build` never probes for a C++ compiler at all.
# std::unordered_map and std::vector need only a compiler, so C++ is enabled
# whenever one exists -- not only when the third-party C++ headers were fetched.
set(BOX_PERF_CXX OFF)
include(CheckLanguage)
check_language(CXX)
if(CMAKE_CXX_COMPILER)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(BOX_PERF_CXX ON)
else()
    message(STATUS "box_perf: no C++ compiler, skipping C++ comparisons")
endif()

add_executable(box_perf
    main.c
    bench.c
    bench_hmap.c
    bench_darray.c
    bench_spset.c
    bench_bitset.c
    bench_stc.c
)

if(BOX_PERF_CXX)
    target_sources(box_perf PRIVATE bench_cpp.cpp)
    target_compile_definitions(box_perf PRIVATE BOX_PERF_CXX_ENABLED)
    message(STATUS "box_perf: C++ enabled, std::unordered_map and std::vector included")
    # ankerl is an extra on top of the std comparisons, not a precondition.
    if(EXISTS ${BOX_PERF_THIRD_PARTY_DIR}/unordered_dense.h)
        target_compile_definitions(box_perf PRIVATE BOX_PERF_HAS_UNORDERED_DENSE)
        message(STATUS "box_perf: found ankerl::unordered_dense")
    endif()
else()
    target_sources(box_perf PRIVATE bench_cpp_stub.c)
endif()

target_link_libraries(box_perf PRIVATE box)
target_include_directories(box_perf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# clock_gettime / CLOCK_MONOTONIC are POSIX, not C99, so -std=c99 alone hides them.
target_compile_definitions(box_perf PRIVATE _POSIX_C_SOURCE=199309L)

# Third-party headers are optional and are not vendored into the repository.
# performance/third_party/fetch.sh downloads them; whatever is present gets
# compiled in, and the suite still runs box-only when none are.
foreach(dep khash:KHASH:khash.h verstable:VERSTABLE:verstable.h stb_ds:STB_DS:stb_ds.h stc:STC:stc/hmap.h)
    string(REPLACE ":" ";" dep_parts ${dep})
    list(GET dep_parts 0 dep_name)
    list(GET dep_parts 1 dep_macro)
    list(GET dep_parts 2 dep_header)
    if(EXISTS ${BOX_PERF_THIRD_PARTY_DIR}/${dep_header})
        target_compile_definitions(box_perf PRIVATE BOX_PERF_HAS_${dep_macro})
        message(STATUS "box_perf: found ${dep_name}")
    else()
        message(STATUS "box_perf: ${dep_name} not present, skipping its comparisons")
    endif()
endforeach()

if(EXISTS ${BOX_PERF_THIRD_PARTY_DIR})
    target_include_directories(box_perf PRIVATE ${BOX_PERF_THIRD_PARTY_DIR})
endif()

# stb_ds needs its implementation compiled exactly once.
if(EXISTS ${BOX_PERF_THIRD_PARTY_DIR}/stb_ds.h)
    target_sources(box_perf PRIVATE stb_ds_impl.c)
endif()

# Benchmarks are meaningless at -O0 and asserts distort the hot paths, so the
# perf target always builds optimized regardless of the surrounding build type.
target_compile_options(box_perf PRIVATE -O3)
target_compile_definitions(box_perf PRIVATE NDEBUG)

if(BOX_PERF_NATIVE)
    target_compile_options(box_perf PRIVATE -march=native)
endif()
