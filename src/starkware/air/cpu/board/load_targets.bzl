def load_targets(
        tags = []):
    """
    Generates additonal targets.
    """

    native.cc_library(
        name = "cpu_air_definitions",
        srcs = native.glob(["cpu_air_definition*.inl"]) + native.glob(["cpu_air_definition*.cc"]),
        hdrs = native.glob(["cpu_air_definition*.h"]),
        visibility = ["//visibility:public"],
        tags = ["external_prover"],
        deps = [
            ":memory_segment",
            "//src/starkware/air/components/ecdsa:ecdsa",
            "//src/starkware/air:air_lib",
            "//src/starkware/air/components:trace_generation_context",
            "//src/starkware/algebra/fft:fft",
            "//src/starkware/algebra/fields:fields_additional_headers",
            "//src/starkware/composition_polynomial:composition_polynomial_additional_headers",
            "//src/starkware/composition_polynomial:composition_polynomial",
            "//src/starkware/air/cpu/component:component",
        ],
        strip_include_prefix = "//src",
    )

    native.cc_library(
        name = "cpu_air_lib",
        hdrs = [
            "cpu_air.h",
            "cpu_air.inl",
            "cpu_air_trace_context.h",
            "memory_segment.h",
        ],
        deps = [
            ":cpu_air_definitions",
            "//src/starkware/air/boundary_constraints",
            "//src/starkware/air/components/diluted_check",
            "//src/starkware/air/components/ecdsa",
            "//src/starkware/air/components/memory",
            "//src/starkware/air/components/pedersen_hash",
            "//src/starkware/air/components/perm_range_check",
            "//src/starkware/air/cpu/builtin/bitwise",
            "//src/starkware/air/cpu/builtin/ec",
            "//src/starkware/air/cpu/builtin/hash",
            "//src/starkware/air/cpu/builtin/keccak",
            "//src/starkware/air/cpu/builtin/poseidon",
            "//src/starkware/air/cpu/builtin/range_check",
            "//src/starkware/air/cpu/builtin/signature",
            "//src/starkware/air/cpu/component",
            "//src/starkware/crypt_tools/hash_context:pedersen_hash_context",
            "//src/starkware/utils",
            "//src/starkware/utils:json",
            "//src/starkware/utils:profiling",
            "//src/starkware/utils:task_manager",
        ],
        tags = ["external_prover"],
    )

    native.cc_test(
        name = "cpu_air_test",
        srcs = [
            "cpu_air_test.cc",
            "cpu_air_test_instructions_memory.bin.h",
            "cpu_air_test_instructions_public_input.json.h",
            "cpu_air_test_instructions_trace.bin.h",
        ],
        visibility = ["//visibility:public"],
        deps = [
            ":cpu_air_lib",
            "//src/starkware/air:air_test_utils",
            "//src/starkware/algebra",
            "//src/starkware/algebra/lde",
            "//src/starkware/error_handling:test_utils",
            "//src/starkware/gtest:starkware_gtest",
            "//src/starkware/statement",
            "//src/starkware/statement/cpu:cpu_air_statement",
            "//src/starkware/utils:input_utils",
        ],
        tags = ["external_prover"],
    )
