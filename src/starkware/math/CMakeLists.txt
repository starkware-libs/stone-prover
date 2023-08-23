# Compile the test
add_executable(math_test math_test.cc)
target_link_libraries(math_test starkware_gtest)
add_test(math_test math_test)
