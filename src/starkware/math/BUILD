package(default_visibility = ["//visibility:public"])

exports_files(["math.h"])

cc_test(
    name = "math_test",
    srcs = [
        "math.h",
        "math_test.cc",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_library(
    name = "math",
    srcs = [
    ],
    hdrs = [
        "math.h",
        "//src/starkware/error_handling:error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
