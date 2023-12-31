add_library(committed_trace committed_trace.cc)
target_link_libraries(committed_trace cached_lde_manager bit_reversal table lde)

add_library(composition_oracle composition_oracle.cc)
target_link_libraries(composition_oracle committed_trace channel)

add_library(oods oods.cc)
target_link_libraries(oods breaker composition_oracle channel)

add_library(stark stark.cc)
target_link_libraries(stark starkware_common fri committed_trace composition_oracle oods channel json third_party profiling)

add_library(stark_utils utils.cc)
target_link_libraries(stark_utils table commitment_scheme_builder)

add_executable(committed_trace_test committed_trace_test.cc)
target_link_libraries(committed_trace_test committed_trace merkle_tree channel stark_utils starkware_gtest)
add_test(committed_trace_test committed_trace_test)

add_executable(composition_oracle_test composition_oracle_test.cc)
target_link_libraries(composition_oracle_test composition_oracle starkware_gtest)
add_test(composition_oracle_test composition_oracle_test)

add_executable(oods_test oods_test.cc)
target_link_libraries(oods_test oods channel stark_utils starkware_gtest fibonacci_air)
add_test(oods_test oods_test)

add_executable(stark_test stark_test.cc)
target_link_libraries(stark_test stark fibonacci_air degree_three_example_air permutation_dummy_air merkle_tree commitment_scheme_builder proof_system starkware_gtest)
add_test(stark_test stark_test)

add_executable(stark_params_test stark_params_test.cc)
target_link_libraries(stark_params_test degree_three_example_air stark starkware_gtest)
add_test(stark_params_test stark_params_test)
