package(default_visibility = ["//visibility:public"])

exports_files(["proof_system.h"])

cc_library(
    name = "proof_system",
    srcs = [
        "proof_system.cc",
    ],
    hdrs = [
        "proof_system.h",
        "//src/starkware/error_handling:error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = ["//src/third_party"],
)

cc_test(
    name = "proof_system_test",
    srcs = [
        "proof_system.h",
        "proof_system_test.cc",
        "//src/starkware/error_handling:error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "proof_system",
        "//src/starkware/gtest:starkware_gtest",
    ],
)
