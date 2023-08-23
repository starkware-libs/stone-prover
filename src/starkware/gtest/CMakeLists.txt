add_library(starkware_gtest STATIC gtest_main.cc)
target_link_libraries(starkware_gtest prng crypto_utils starkware_common third_party gmock gtest)
