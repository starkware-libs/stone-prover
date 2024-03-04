package(default_visibility = ["//visibility:public"])

exports_files([
    "poseidon.h",
    "poseidon.inl",
])

cc_library(
    name = "poseidon",
    srcs = [
    ],
    hdrs = [
        "poseidon.h",
        "poseidon.inl",
        "//src/starkware/air:trace.h",
        "//src/starkware/air/components:trace_generation_context.h",
        "//src/starkware/air/components:virtual_column.h",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra/polymorphic:field.h",
        "//src/starkware/algebra/polymorphic:field.inl",
        "//src/starkware/algebra/polymorphic:field_element.h",
        "//src/starkware/algebra/polymorphic:field_element.inl",
        "//src/starkware/algebra/polymorphic:field_element_span.h",
        "//src/starkware/algebra/polymorphic:field_element_span.inl",
        "//src/starkware/algebra/polymorphic:field_element_vector.h",
        "//src/starkware/algebra/polymorphic:field_element_vector.inl",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/starkware/utils:to_from_string.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
