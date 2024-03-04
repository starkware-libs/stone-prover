cc_library(
    name = "cpu_air_statement",
    srcs = ["cpu_air_statement.cc"],
    hdrs = ["cpu_air_statement.h"],
    deps = [
        "//src/starkware/air/cpu/board:cpu_air_lib",
        "//src/starkware/cairo/lang/vm/cpp:cpu_decoder",
        "//src/starkware/crypt_tools",
        "//src/starkware/statement",
        "//src/starkware/utils:json",
    ],
)

package(default_visibility = ["//visibility:public"])

exports_files(["cpu_air_statement.h"])

cc_library(
    name = "cpu",
    srcs = [
    ],
    hdrs = [
        "cpu_air_statement.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
