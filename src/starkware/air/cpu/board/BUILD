load("//src/starkware/air/cpu/board:load_targets.bzl", "load_targets")

load_targets()

cc_library(
    name = "memory_segment",
    hdrs = [
        "memory_segment.h",
    ],
    visibility = ["//visibility:public"],
)

package(default_visibility = ["//visibility:public"])

exports_files([
    "cpu_air.h",
    "cpu_air.inl",
    "cpu_air_trace_context.h",
])
