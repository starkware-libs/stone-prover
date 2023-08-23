add_subdirectory(jsoncpp)

# Add a monolithic library that encompasses all of our third-party code
set(THIRD_PARTY_LIBS glog gflags pthread jsoncpp)
add_library(third_party INTERFACE)
target_link_libraries(third_party INTERFACE ${THIRD_PARTY_LIBS})

add_subdirectory(blake2)
add_subdirectory(XKCP)
