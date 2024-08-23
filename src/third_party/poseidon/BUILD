exports_files([
    "f251.h",
    "poseidon.h",
])

cc_library(
    name = "poseidon",
    srcs = [
        "f251.c",
        "poseidon.c",
        "poseidon_rc.c",
    ] + select({
        "@platforms//cpu:x86_64": [
            "f251_asm.s",
        ],
        "//conditions:default": [
        ],
    }),
    hdrs = [
        "f251.h",
        "poseidon.h",
        "poseidon_rc.h",
    ],
    visibility = ["//visibility:public"],
    tags = ["external_prover"],
)
