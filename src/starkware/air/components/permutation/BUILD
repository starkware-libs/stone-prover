load("//src/starkware/air/components/permutation:load_targets.bzl", "load_targets")

cc_library(
    name = "permutation",
    hdrs = [
        "multi_column_permutation.h",
        "multi_column_permutation.inl",
        "permutation.h",
        "permutation.inl",
        "permutation_trace_context.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/air/components:trace_generation_context",
    ],
)

load_targets()

cc_test(
    name = "permutation_dummy_air_test",
    srcs = [
        "permutation_dummy_air_test.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":permutation",
        ":permutation_dummy_air_lib",
        "//src/starkware/air:air_test_utils",
        "//src/starkware/algebra",
        "//src/starkware/algebra/lde",
        "//src/starkware/error_handling:test_utils",
        "//src/starkware/gtest:starkware_gtest",
    ],
)

package(default_visibility = ["//visibility:public"])

exports_files(glob([
    "*.h",
    "*.inl",
    "*.py",
]))

cc_library(
    name = "permutation_additional_headers",
    srcs = [
    ],
    hdrs = [
        "multi_column_permutation.h",
        "permutation.h",
        "permutation_trace_context.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
