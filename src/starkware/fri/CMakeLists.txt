add_library(fri fri_prover.cc fri_verifier.cc fri_details.cc fri_folder.cc fri_layer.cc fri_committed_layer.cc)
target_link_libraries(fri algebra channel lde json table third_party)

add_executable(fri_test fri_test.cc fri_details_test.cc fri_folder.cc)
target_link_libraries(fri_test fri channel commitment_scheme_builder table proof_system starkware_gtest)
add_test(fri_test fri_test)

add_executable(fri_layer_test fri_layer_test.cc fri_layer.cc)
target_link_libraries(fri_layer_test fri starkware_gtest)
add_test(fri_layer_test fri_layer_test)

add_executable(fri_committed_layer_test fri_committed_layer_test.cc fri_committed_layer.cc fri_layer.cc)
target_link_libraries(fri_committed_layer_test channel fri starkware_gtest stark_utils table)
add_test(fri_committed_layer_test fri_committed_layer_test)
