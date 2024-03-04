package(default_visibility = ["//visibility:public"])

exports_files([
    "trace_generation_context.h",
    "virtual_column.h",
])

cc_test(
    name = "trace_generation_context_test",
    srcs = [
        "trace_generation_context.h",
        "trace_generation_context_test.cc",
        "virtual_column.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
        "//src/starkware/math:math.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_library(
    name = "trace_generation_context",
    srcs = [
        "trace_generation_context.cc",
    ],
    hdrs = [
        "trace_generation_context.h",
        "virtual_column.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
