cc_library(
    name = "error_handling",
    srcs = [
        "error_handling.cc",
    ],
    hdrs = [
        "error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party/backward-cpp",
    ],
)

cc_library(
    name = "test_utils",
    srcs = [
        "test_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "error_handling",
    ],
)

cc_library(
    name = "error_handling_no_stacktrace",
    srcs = [
        "error_handling.cc",
    ],
    hdrs = [
        "error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party",
        "//src/third_party/backward-cpp",
    ],
)

package(default_visibility = ["//visibility:public"])

exports_files([
    "error_handling.h",
    "test_utils.h",
])

cc_test(
    name = "error_handling_test",
    srcs = [
        "error_handling.h",
        "error_handling_test.cc",
        "test_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "error_handling",
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_library(
    name = "error_handling_additional_headers",
    srcs = [
    ],
    hdrs = [
        "error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
