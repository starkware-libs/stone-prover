add_library(to_from_string to_from_string.cc)
target_link_libraries(to_from_string third_party)

add_library(stats stats.cc)
target_link_libraries(stats third_party)

add_executable(stats_test stats_test.cc)
target_link_libraries(stats_test starkware_gtest stats)
add_test(stats_test stats_test)

add_library(profiling profiling.cc)
target_link_libraries(profiling stats third_party)

add_executable(profiling_test profiling_test.cc)
target_link_libraries(profiling_test starkware_gtest profiling)
add_test(profiling_test profiling_test)

add_library(json json.cc)
target_link_libraries(json name_to_field third_party)

add_library(flag_validators flag_validators.cc)

add_executable(json_test json_test.cc)
target_link_libraries(json_test json starkware_gtest)
add_test(json_test json_test)

add_library(task_manager task_manager.cc)
target_link_libraries(task_manager third_party)

add_library(bit_reversal bit_reversal.cc)
target_link_libraries(bit_reversal)

add_executable(bit_reversal_test bit_reversal_test.cc)
target_link_libraries(bit_reversal_test bit_reversal algebra starkware_gtest)
add_test(bit_reversal_test bit_reversal_test)

add_executable(maybe_owned_ptr_test maybe_owned_ptr_test.cc)
target_link_libraries(maybe_owned_ptr_test starkware_gtest)
add_test(maybe_owned_ptr_test maybe_owned_ptr_test)

add_executable(to_from_string_test to_from_string_test.cc)
target_link_libraries(to_from_string_test starkware_gtest)
add_test(to_from_string_test to_from_string_test)

add_executable(serialization_test serialization_test.cc)
target_link_libraries(serialization_test starkware_gtest)
add_test(serialization_test serialization_test)

add_executable(task_manager_test task_manager_test.cc)
target_link_libraries(task_manager_test starkware_gtest task_manager)
add_test(task_manager_test task_manager_test)

add_library(input_utils input_utils.cc)
target_link_libraries(input_utils json third_party)
