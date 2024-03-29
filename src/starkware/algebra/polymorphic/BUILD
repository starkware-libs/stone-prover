package(default_visibility = ["//visibility:public"])

exports_files(glob([
    "*.h",
    "*.inl",
]))

cc_library(
    name = "polymorphic_algebra",
    srcs = [
        "field.cc",
        "field_element.cc",
        "field_element_span.cc",
        "field_element_vector.cc",
    ],
    hdrs = [
        "field.h",
        "field.inl",
        "field_element.h",
        "field_element.inl",
        "field_element_span.h",
        "field_element_span.inl",
        "field_element_vector.h",
        "field_element_vector.inl",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:big_prime_constants.h",
        "//src/starkware/algebra/fields:extension_field_element.h",
        "//src/starkware/algebra/fields:extension_field_element.inl",
        "//src/starkware/algebra/fields:long_field_element.h",
        "//src/starkware/algebra/fields:prime_field_element.h",
        "//src/starkware/algebra/fields:prime_field_element.inl",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
        "//src/starkware/algebra/utils:invoke_template_version.h",
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
        "//src/starkware/algebra/fields",
        "//src/third_party",
    ],
)

cc_test(
    name = "field_element_test",
    srcs = [
        "field.h",
        "field.inl",
        "field_element.h",
        "field_element.inl",
        "field_element_test.cc",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
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

cc_test(
    name = "field_test",
    srcs = [
        "field.h",
        "field.inl",
        "field_element.h",
        "field_element.inl",
        "field_test.cc",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
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

cc_test(
    name = "field_element_vector_test",
    srcs = [
        "field.h",
        "field.inl",
        "field_element.h",
        "field_element.inl",
        "field_element_span.h",
        "field_element_span.inl",
        "field_element_vector.h",
        "field_element_vector.inl",
        "field_element_vector_test.cc",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:long_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
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

cc_test(
    name = "field_element_span_test",
    srcs = [
        "field.h",
        "field.inl",
        "field_element.h",
        "field_element.inl",
        "field_element_span.h",
        "field_element_span.inl",
        "field_element_span_test.cc",
        "field_element_vector.h",
        "field_element_vector.inl",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
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
