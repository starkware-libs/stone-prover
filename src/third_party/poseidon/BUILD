exports_files([
    "f251.h",
    "poseidon.h",
])

cc_library(
    name = "poseidon",
    srcs = [
        "f251_asm.s",
        "f251.c",
        "poseidon.c",
        "poseidon_rc.c",
    ],
    hdrs = [
        "f251.h",
        "poseidon.h",
        "poseidon_rc.h",
    ],
    copts = ["-DASSEMBLY"],
    visibility = ["//visibility:public"],
    tags = ["external_prover"],
)
