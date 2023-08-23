# Compile the test
add_executable(containers_test containers_test.cc)
target_link_libraries(containers_test starkware_gtest)
add_test(containers_test containers_test)

add_executable(string_test string_test.cc)
target_link_libraries(string_test starkware_gtest)
add_test(string_test string_test)
