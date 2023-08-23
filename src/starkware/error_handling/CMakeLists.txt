add_library(error_handling error_handling.cc)
target_link_libraries(error_handling third_party dw)


# Same as error_handling but doesn't try to unwind the stack.
add_library(error_handling_no_stacktrace error_handling.cc)
set_target_properties(error_handling_no_stacktrace PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DDISABLE_STACKTRACE ")
target_link_libraries(error_handling_no_stacktrace third_party)

add_executable(error_handling_test error_handling_test.cc)
target_link_libraries(error_handling_test error_handling starkware_gtest)
add_test(error_handling_test error_handling_test)
