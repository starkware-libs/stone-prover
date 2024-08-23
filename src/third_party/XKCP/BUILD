config_setting(
    name = "no_avx",
    values = {
        "copt": "-DNO_AVX",
    },
)

cc_library(
    name = "Keccak1600F",
    srcs = select({
        ":no_avx": [
            "CompactFIPS202/Keccak-readable-and-compact.c",
        ],
        "@platforms//cpu:x86_64": [
            "KeccakP-1600-OptimizedAVX2/KeccakP-1600-AVX2.s",
        ],
        "//conditions:default": [
            "CompactFIPS202/Keccak-readable-and-compact.c",
        ],
    }),  
    visibility = ['//visibility:public'],  
    tags = ["external_prover"],
)
