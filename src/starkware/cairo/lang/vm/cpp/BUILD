package(default_visibility = ["//visibility:public"])

exports_files([
    "decoder.h",
    "trace_utils.h",
    "trace_utils.inl",
])

cc_library(
    name = "cpu_decoder",
    srcs = [
        "decoder.cc",
    ],
    hdrs = [
        "decoder.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)

cc_test(
    name = "decoder_test",
    srcs = [
        "decoder.h",
        "decoder_test.cc",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:big_prime_constants.h",
        "//src/starkware/algebra/fields:prime_field_element.h",
        "//src/starkware/algebra/fields:prime_field_element.inl",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/starkware/utils:attributes.h",
        "//src/starkware/utils:serialization.h",
        "//src/starkware/utils:serialization.inl",
        "//src/starkware/utils:to_from_string.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "cpu_decoder",
        "//src/starkware/algebra",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_test(
    name = "trace_utils_test",
    srcs = [
        "decoder.h",
        "trace_utils.h",
        "trace_utils.inl",
        "trace_utils_test.cc",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:field_to_int.h",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:big_prime_constants.h",
        "//src/starkware/algebra/fields:prime_field_element.h",
        "//src/starkware/algebra/fields:prime_field_element.inl",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/starkware/utils:attributes.h",
        "//src/starkware/utils:serialization.h",
        "//src/starkware/utils:serialization.inl",
        "//src/starkware/utils:to_from_string.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/algebra",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)
