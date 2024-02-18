add_executable(range_check_cell_test range_check_cell_test.cc)
target_link_libraries(range_check_cell_test air_test_utils algebra lde starkware_gtest
                      trace_generation_context)
add_test(range_check_cell_test range_check_cell_test)
