add_library(cpu_decoder decoder.cc)

add_executable(decoder_test decoder_test.cc)
target_link_libraries(decoder_test algebra starkware_gtest cpu_decoder)
add_test(decoder_test decoder_test)

add_executable(trace_utils_test trace_utils_test.cc)
target_link_libraries(trace_utils_test algebra starkware_gtest)
add_test(trace_utils_test trace_utils_test)
