package(default_visibility = ["//visibility:public"])

exports_files(["containers.h"])

cc_test(
    name = "containers_test",
    srcs = [
        "containers.h",
        "containers_test.cc",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_test(
    name = "string_test",
    srcs = [
        "string.h",
        "string_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_library(
    name = "stl_utils",
    srcs = [
    ],
    hdrs = [
        "containers.h",
        "string.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
