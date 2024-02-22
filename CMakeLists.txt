if(EXISTS ${CMAKE_SOURCE_DIR}/src/starkware/stark)
  file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/src/starkware/stark)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/src/starkware/air)
  file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/src/starkware/air)
  file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/src/starkware/air)
endif()

if(APPLE)
  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/starkware/air)
  file(COPY ${CMAKE_SOURCE_DIR}/src/starkware/air_apple/ DESTINATION ${CMAKE_SOURCE_DIR}/src/starkware/air/)

  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde)
  file(COPY ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde_apple/ DESTINATION ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde/)

  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/starkware/stark)
  file(COPY ${CMAKE_SOURCE_DIR}/src/starkware/stark_apple/ DESTINATION ${CMAKE_SOURCE_DIR}/src/starkware/stark/)

  cmake_minimum_required (VERSION 3.22)
  if (NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm@12/bin/clang++)
  endif()
  if (NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /opt/homebrew/opt/llvm@12/bin/clang)
  endif()

  project(StarkStack VERSION 0.1.0 LANGUAGES CXX C ASM)
  include(CTest)

  # Basic setup for gtest
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  enable_testing()

  include_directories("${CMAKE_SOURCE_DIR}/src")
  include_directories("${CMAKE_BINARY_DIR}/src")

  # Basic setup for gflags
  find_package(gflags REQUIRED)

  if (NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
  endif()

  # Clang specific flags.
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # Use absolute-paths for diagnostics to make them clickable in vscode.
    add_compile_options("-fdiagnostics-absolute-paths")

    # Increase constexpr steps.
    add_compile_options("-fconstexpr-steps=20000000")
  endif()

  # Force colored output.
  add_compile_options ("-fdiagnostics-color=always")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override")
  if (DEFINED NO_AVX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_AVX=1")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAARCH64=1")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mpclmul -maes")
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fno-strict-aliasing -Wno-mismatched-tags -fPIC")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

  set(CC_OPTIMIZE "-O3")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CC_OPTIMIZE} -g -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CC_OPTIMIZE}")

  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  # USE_REGISTER_HINTS enables a trick based on GCC's extended asm to improve the generated code.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_REGISTER_HINTS")

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_subdirectory(src)

  set(CPACK_PROJECT_NAME ${PROJECT_NAME})
  set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
  include(CPack)
    
else()

  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/starkware/air)
  file(COPY ${CMAKE_SOURCE_DIR}/src/starkware/air_ref/ DESTINATION ${CMAKE_SOURCE_DIR}/src/starkware/air/)

  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde)
  file(COPY ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde_ref/ DESTINATION ${CMAKE_SOURCE_DIR}/src/starkware/algebra/lde/)

  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/starkware/stark)
  file(COPY ${CMAKE_SOURCE_DIR}/src/starkware/stark_ref/ DESTINATION ${CMAKE_SOURCE_DIR}/src/starkware/stark/)

  cmake_minimum_required (VERSION 3.22)
  if (NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++-12)
  endif()
  if (NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/clang-12)
  endif()

  project(StarkStack VERSION 0.1.0 LANGUAGES CXX C ASM)
  include(CTest)

  # Basic setup for gtest
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  enable_testing()

  include_directories("${CMAKE_SOURCE_DIR}/src")
  include_directories("${CMAKE_BINARY_DIR}/src")

  # Basic setup for gflags
  find_package(gflags REQUIRED)

  if (NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
  endif()

  # Clang specific flags.
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # Use absolute-paths for diagnostics to make them clickable in vscode.
    add_compile_options("-fdiagnostics-absolute-paths")

    # Increase constexpr steps.
    add_compile_options("-fconstexpr-steps=20000000")
  endif()

  # Force colored output.
  add_compile_options ("-fdiagnostics-color=always")

  if (DEFINED NO_AVX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_AVX=1")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAARCH64=1")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mpclmul -maes")
  endif()

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wno-mismatched-tags -fno-strict-aliasing -fPIC")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

  set(CC_OPTIMIZE "-O3")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CC_OPTIMIZE} -g -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CC_OPTIMIZE}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  # USE_REGISTER_HINTS enables a trick based on GCC's extended asm to improve the generated code.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_REGISTER_HINTS")

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_subdirectory(src)

  set(CPACK_PROJECT_NAME ${PROJECT_NAME})
  set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
  include(CPack)
endif()
