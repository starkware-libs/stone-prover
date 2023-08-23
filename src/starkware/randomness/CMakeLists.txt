add_library(prng prng.cc hash_chain.cc)
target_link_libraries(prng third_party to_from_string crypto_utils)

add_executable(prng_test prng_test.cc)
target_link_libraries(prng_test algebra starkware_gtest)
add_test(prng_test prng_test)

add_executable(hash_chain_test hash_chain_test.cc)
target_link_libraries(hash_chain_test algebra starkware_gtest)
add_test(hash_chain_test hash_chain_test)
