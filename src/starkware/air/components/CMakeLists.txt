add_executable(trace_generation_context_test trace_generation_context_test.cc)
target_link_libraries(trace_generation_context_test starkware_gtest)
add_test(trace_generation_context_test trace_generation_context_test)

add_library(trace_generation_context trace_generation_context.cc)

add_subdirectory(diluted_check)
add_subdirectory(memory)
add_subdirectory(perm_range_check)
add_subdirectory(permutation)
