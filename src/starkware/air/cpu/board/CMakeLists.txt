add_library(cpu_air cpu_air.h)
target_link_libraries(cpu_air trace_generation_context composition_polynomial profiling pedersen_hash_context)

add_executable(cpu_air_test cpu_air_test.cc)
target_link_libraries(cpu_air_test cpu_air cpu_air_statement air_test_utils algebra lde
    starkware_gtest)
add_test(cpu_air_test cpu_air_test)

