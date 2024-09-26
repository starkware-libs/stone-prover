package(default_visibility = ["//visibility:public"])

exports_files([
    "mod_builtin_prover_context.h",
    "mod_builtin_prover_context.inl",
    "add_mod_builtin_prover_context.h",
    "add_mod_builtin_prover_context.inl",
    "mul_mod_builtin_prover_context.h",
    "mul_mod_builtin_prover_context.inl",
])

cc_library(
    name = "mod_common",
    srcs = [
    ],
    hdrs = [
        "mod_builtin_prover_context.h",
        "mod_builtin_prover_context.inl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/air:air_lib",
        "//src/starkware/air/components:trace_generation_context",
        "//src/starkware/air/components/memory",
        "//src/starkware/air/components/perm_table_check",
        "//src/starkware/air/components/permutation",
        "//src/starkware/algebra:algebra_additional_headers",
        "//src/starkware/crypt_tools:crypto_utils",
        "//src/starkware/error_handling",
        "//src/starkware/math",
        "//src/starkware/randomness:prng",
        "//src/starkware/stl_utils",
        "//src/starkware/utils:task_manager",
        "//src/starkware/utils:to_from_string",
        "//src/third_party/gsl",
    ],
)

cc_library(
    name = "add_mod",
    srcs = [
    ],
    hdrs = [
        "add_mod_builtin_prover_context.h",
        "add_mod_builtin_prover_context.inl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mod_common",
    ],
)

cc_library(
    name = "mul_mod",
    srcs = [
    ],
    hdrs = [
        "mul_mod_builtin_prover_context.h",
        "mul_mod_builtin_prover_context.inl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mod_common",
        "//src/starkware/air/components/perm_range_check",
    ],
)

cc_library(
    name = "modulo",
    srcs = [
    ],
    hdrs = [],
    visibility = ["//visibility:public"],
    deps = [
        "add_mod",
        "mul_mod",
    ],
)
