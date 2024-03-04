cc_library(
    name = "starkware_gtest",
    srcs = [
        "gtest_main.cc",
    ],
    hdrs = [
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/utils:task_manager.h",
        "//src/starkware/utils:task_manager.inl",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware:starkware_common",
        "//src/starkware/crypt_tools:crypto_utils",
        "//src/starkware/randomness:prng",
        "//src/third_party",
        "@com_github_gtest_gtest//:gtest",
    ],
)
