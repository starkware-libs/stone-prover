package(default_visibility = ["//visibility:public"])

exports_files(glob([
    "*.h",
    "*.inl",
]))

cc_library(
    name = "to_from_string",
    srcs = [
        "to_from_string.cc",
    ],
    hdrs = [
        "to_from_string.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party",
    ],
)

cc_library(
    name = "stats",
    srcs = [
        "stats.cc",
    ],
    hdrs = [
        "profiling.h",
        "stats.h",
        "//src/starkware/error_handling:error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = ["//src/third_party"],
)

cc_test(
    name = "stats_test",
    srcs = [
        "stats.h",
        "stats_test.cc",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "stats",
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_library(
    name = "profiling",
    srcs = [
        "profiling.cc",
    ],
    hdrs = [
        "profiling.h",
        "stats.h",
        "//src/starkware/error_handling:error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "stats",
        "//src/third_party",
    ],
)

cc_test(
    name = "profiling_test",
    srcs = [
        "profiling.h",
        "profiling_test.cc",
        "stats.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "profiling",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_library(
    name = "json",
    srcs = [
        "json.cc",
    ],
    hdrs = [
        "json.h",
        "json.inl",
        "to_from_string.h",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra/polymorphic:field.h",
        "//src/starkware/algebra/polymorphic:field.inl",
        "//src/starkware/algebra/polymorphic:field_element.h",
        "//src/starkware/algebra/polymorphic:field_element.inl",
        "//src/starkware/algebra/utils:name_to_field.h",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
        "//src/third_party/jsoncpp/json:json.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/algebra/utils:name_to_field",
        "//src/third_party",
    ],
)

cc_library(
    name = "flag_validators",
    srcs = [
        "flag_validators.cc",
    ],
    hdrs = [
        "flag_validators.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party",
    ],
)

cc_test(
    name = "json_test",
    srcs = [
        "attributes.h",
        "json.h",
        "json.inl",
        "json_builder.h",
        "json_test.cc",
        "serialization.h",
        "serialization.inl",
        "to_from_string.h",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/algebra/fields:test_field_element.h",
        "//src/starkware/algebra/fields:test_field_element.inl",
        "//src/starkware/algebra/polymorphic:field.h",
        "//src/starkware/algebra/polymorphic:field.inl",
        "//src/starkware/algebra/polymorphic:field_element.h",
        "//src/starkware/algebra/polymorphic:field_element.inl",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
        "//src/third_party/jsoncpp/json:json.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "json",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_library(
    name = "task_manager",
    srcs = [
        "task_manager.cc",
    ],
    hdrs = [
        "task_manager.h",
        "task_manager.inl",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party",
    ],
)

cc_library(
    name = "bit_reversal",
    srcs = [
        "bit_reversal.cc",
    ],
    hdrs = [
        "attributes.h",
        "bit_reversal.h",
        "serialization.h",
        "serialization.inl",
        "task_manager.h",
        "task_manager.inl",
        "to_from_string.h",
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
        "//src/starkware/algebra/polymorphic:field_element_span.h",
        "//src/starkware/algebra/polymorphic:field_element_span.inl",
        "//src/starkware/algebra/polymorphic:field_element_vector.h",
        "//src/starkware/algebra/polymorphic:field_element_vector.inl",
        "//src/starkware/algebra/utils:invoke_template_version.h",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/third_party",
    ],
)

cc_test(
    name = "bit_reversal_test",
    srcs = [
        "attributes.h",
        "bit_reversal.h",
        "bit_reversal_test.cc",
        "serialization.h",
        "serialization.inl",
        "task_manager.h",
        "task_manager.inl",
        "to_from_string.h",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:field_element_base.h",
        "//src/starkware/algebra:field_element_base.inl",
        "//src/starkware/algebra:field_operations.h",
        "//src/starkware/algebra:field_operations.inl",
        "//src/starkware/algebra:uint128.h",
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
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "bit_reversal",
        "//src/starkware/algebra",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_test(
    name = "maybe_owned_ptr_test",
    srcs = [
        "maybe_owned_ptr.h",
        "maybe_owned_ptr_test.cc",
        "//src/starkware/error_handling:error_handling.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_test(
    name = "to_from_string_test",
    srcs = [
        "to_from_string.h",
        "to_from_string_test.cc",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
    ],
)

cc_test(
    name = "serialization_test",
    srcs = [
        "attributes.h",
        "serialization.h",
        "serialization.inl",
        "serialization_test.cc",
        "to_from_string.h",
        "//src/starkware/algebra:big_int.h",
        "//src/starkware/algebra:big_int.inl",
        "//src/starkware/algebra:uint128.h",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_test(
    name = "task_manager_test",
    srcs = [
        "task_manager.h",
        "task_manager.inl",
        "task_manager_test.cc",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/error_handling:test_utils.h",
        "//src/third_party/cppitertools:range.hpp",
        "//src/third_party/cppitertools/internal:iterbase.hpp",
        "//src/third_party/gsl:gsl-lite.hpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "task_manager",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_library(
    name = "input_utils",
    srcs = [
        "input_utils.cc",
    ],
    hdrs = [
        "attributes.h",
        "input_utils.h",
        "json.h",
        "json.inl",
        "json_builder.h",
        "serialization.h",
        "serialization.inl",
        "to_from_string.h",
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
        "//src/starkware/algebra/fields:field_operations_helper.h",
        "//src/starkware/algebra/fields:field_operations_helper.inl",
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
        "//src/starkware/algebra/utils:name_to_field.h",
        "//src/starkware/crypt_tools:keccak_256.h",
        "//src/starkware/crypt_tools:keccak_256.inl",
        "//src/starkware/crypt_tools:utils.h",
        "//src/starkware/error_handling:error_handling.h",
        "//src/starkware/math:math.h",
        "//src/starkware/randomness:hash_chain.h",
        "//src/starkware/randomness:prng.h",
        "//src/starkware/stl_utils:containers.h",
        "//src/third_party/gsl:gsl-lite.hpp",
        "//src/third_party/jsoncpp/json:json.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "json",
        "//src/third_party",
    ],
)

cc_library(
    name = "utils",
    srcs = [
    ],
    hdrs = [
        "aligned_unique_ptr.h",
        "maybe_owned_ptr.h",
        "nanotimer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
