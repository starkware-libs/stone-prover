package(default_visibility = ["//visibility:public"])

exports_files(glob([
    "*.h",
    "*.inl",
]))

cc_library(
    name = "fft",
    srcs = [
        "fft.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party",
    ],
)

cc_test(
    name = "fft_test",
    srcs = [
        "details.h",
        "details.inl",
        "fft_test.cc",
        "fft_with_precompute.h",
        "fft_with_precompute.inl",
        "multiplicative_fft.h",
        "multiplicative_fft.inl",
        "multiplicative_group_ordering.h",
        "transpose.h",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:polynomials.h",
        "//src/starkware/algebra:polynomials.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:big_prime_constants.h",
        "//src/starkware/algebra/fields:extension_field_element.h",
        "//src/starkware/algebra/fields:extension_field_element.inl",
        "//src/starkware/algebra/fields:long_field_element.h",
        "//src/starkware/algebra/fields:prime_field_element.h",
        "//src/starkware/algebra/fields:prime_field_element.inl",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
        "//src/starkware/algebra/polymorphic:field.h",
        "//src/starkware/algebra/polymorphic:field.inl",
        "//src/starkware/algebra/polymorphic:field_element.h",
        "//src/starkware/algebra/polymorphic:field_element.inl",
        "//src/starkware/algebra/polymorphic:field_element_span.h",
        "//src/starkware/algebra/polymorphic:field_element_span.inl",
        "//src/starkware/algebra/polymorphic:field_element_vector.h",
        "//src/starkware/algebra/polymorphic:field_element_vector.inl",
        "//src/starkware/algebra/utils:invoke_template_version.h",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/fft_utils:fft_bases.h",
        "//src/starkware/fft_utils:fft_bases.inl",
        "//src/starkware/fft_utils:fft_domain.h",
        "//src/starkware/fft_utils:fft_group.h",
        "//src/starkware/fft_utils:fft_group.inl",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/starkware/utils:attributes.h",
        "//src/starkware/utils:bit_reversal.h",
        "//src/starkware/utils:serialization.h",
        "//src/starkware/utils:serialization.inl",
        "//src/starkware/utils:task_manager.h",
        "//src/starkware/utils:task_manager.inl",
        "//src/starkware/utils:to_from_string.h",
        "//src/third_party/cppitertools:range.hpp",
        "//src/third_party/cppitertools:reversed.hpp",
        "//src/third_party/cppitertools/internal:iterator_wrapper.hpp",
        "//src/third_party/cppitertools/internal:iterbase.hpp",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "fft",
        "//src/starkware/algebra",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_library(
    name = "fft_additional_headers",
    srcs = [
    ],
    hdrs = [
        "details.h",
        "details.inl",
        "fft_with_precompute.h",
        "fft_with_precompute.inl",
        "multiplicative_fft.h",
        "multiplicative_fft.inl",
        "multiplicative_group_ordering.h",
        "transpose.h",
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
        "//src/starkware/algebra/polymorphic:field.h",
        "//src/starkware/algebra/polymorphic:field.inl",
        "//src/starkware/algebra/polymorphic:field_element.h",
        "//src/starkware/algebra/polymorphic:field_element.inl",
        "//src/starkware/algebra/utils:invoke_template_version.h",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/fft_utils:fft_bases.h",
        "//src/starkware/fft_utils:fft_bases.inl",
        "//src/starkware/fft_utils:fft_domain.h",
        "//src/starkware/fft_utils:fft_group.h",
        "//src/starkware/fft_utils:fft_group.inl",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/starkware/utils:attributes.h",
        "//src/starkware/utils:serialization.h",
        "//src/starkware/utils:serialization.inl",
        "//src/starkware/utils:task_manager.h",
        "//src/starkware/utils:task_manager.inl",
        "//src/starkware/utils:to_from_string.h",
        "//src/third_party/cppitertools:range.hpp",
        "//src/third_party/cppitertools:reversed.hpp",
        "//src/third_party/cppitertools/internal:iterator_wrapper.hpp",
        "//src/third_party/cppitertools/internal:iterbase.hpp",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
