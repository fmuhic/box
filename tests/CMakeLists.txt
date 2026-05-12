add_executable(box_tests 
    test_box.c 
    hmap/test_hmap.c
    darray/test_darray.c
    bitset/test_bitset.c
    spset/test_spset.c
)

target_include_directories(box_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(box_tests PRIVATE box)
add_test(NAME BoxUnitTests COMMAND box_tests)
