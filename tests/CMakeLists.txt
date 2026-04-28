add_executable(box_tests test_box.c)
target_link_libraries(box_tests PRIVATE box)
add_test(NAME BoxUnitTests COMMAND box_tests)
