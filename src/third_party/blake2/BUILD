exports_files([
    "blake2-impl.h",
    "blake2.h",
])

cc_library(
    name = "blake2s",
    srcs = [
        "blake2s.c",
    ],
    hdrs = [
        "blake2.h",
        "blake2-config.h",
        "blake2-impl.h",
        "blake2s-load-sse2.h",
        "blake2s-round.h",
    ],
    visibility = ["//visibility:public"],
    tags = ["external_prover"],
    deps = [
    ],
)

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "blake2",
    srcs = [
    ],
    hdrs = [
        "blake2.h",
        "blake2-config.h",
        "blake2-impl.h",
        "blake2b-load-sse2.h",
        "blake2b-load-sse41.h",
        "blake2b-round.h",
        "blake2s-load-sse2.h",
        "blake2s-load-sse41.h",
        "blake2s-load-xop.h",
        "blake2s-round.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
