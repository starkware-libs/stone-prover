exports_files([
    "blake2-impl.h",
    "blake2.h",
])
cc_library(
    name = "blake2s",
    srcs = select({
        "@platforms//cpu:x86_64": [
            "sse/blake2s.c",
        ],
        "@platforms//cpu:aarch64": [
            "neon/blake2s.c",
        ],
        "//conditions:default": [
            "reference/blake2s.c",
        ],
    }),
    hdrs = [
        "blake2.h",
        "blake2-impl.h",
    ] + select({
        "@platforms//cpu:x86_64": [
            "sse/blake2-config.h",
            "sse/blake2s-load-sse2.h",
            "sse/blake2s-load-sse41.h",
            "sse/blake2s-load-xop.h",
            "sse/blake2s-round.h",
        ],
        "@platforms//cpu:aarch64": [
            "neon/blake2s-load-neon.h",
            "neon/blake2s-round.h",
        ],
    }, no_match_error = "No blake2s implementation for the target platform"),
    visibility = ["//visibility:public"],
    tags = ["external_prover"],
    deps = [],
)
