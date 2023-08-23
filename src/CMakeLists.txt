set(STARKWARE_COMMON_LIBS task_manager error_handling to_from_string)
add_library(starkware_common INTERFACE)
target_link_libraries(starkware_common INTERFACE ${STARKWARE_COMMON_LIBS})

add_subdirectory(starkware)
add_subdirectory(third_party)
