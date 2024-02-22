add_executable(diluted_check_cell_test diluted_check_cell_test.cc)
target_link_libraries(diluted_check_cell_test air_test_utils algebra lde starkware_gtest
                      trace_generation_context)
add_test(diluted_check_cell_test diluted_check_cell_test)

add_executable(diluted_check_test diluted_check_test.cc)
target_link_libraries(diluted_check_test air_test_utils algebra lde starkware_gtest prng
                      trace_generation_context)
add_test(diluted_check_test diluted_check_test)
