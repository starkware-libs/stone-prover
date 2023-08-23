add_library(lde lde.cc)
target_link_libraries(lde fft algebra task_manager)

add_library(cached_lde_manager cached_lde_manager.cc)
target_link_libraries(cached_lde_manager)

add_executable(lde_test lde_test.cc)
target_link_libraries(lde_test lde starkware_gtest)
add_test(lde_test lde_test)

add_executable(cached_lde_manager_test cached_lde_manager_test.cc)
target_link_libraries(cached_lde_manager_test cached_lde_manager lde starkware_gtest)
add_test(cached_lde_manager_test cached_lde_manager_test)
