load("//src/starkware/air/fibonacci:load_targets.bzl", "load_targets")

load_targets()

cc_test(
    name = "fibonacci_air_test",
    srcs = [
        "fibonacci_air_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":fibonacci_air_lib",
        "//src/starkware/air:air_test_utils",
        "//src/starkware/algebra",
        "//src/starkware/algebra/lde",
        "//src/starkware/error_handling:test_utils",
        "//src/starkware/gtest:starkware_gtest",
    ],
)

package(default_visibility = ["//visibility:public"])

exports_files([
    "fibonacci_trace_context.h",
])
