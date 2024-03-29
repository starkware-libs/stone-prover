package(default_visibility = ["//visibility:public"])

exports_files(glob([
    "*.h",
    "*.inl",
    "*.py",
]))

cc_test(
    name = "diluted_check_cell_test",
    srcs = [
        "diluted_check_cell.h",
        "diluted_check_cell.inl",
        "diluted_check_cell_test.cc",
        "//src/starkware/air/components:trace_generation_context.h",
        "//src/starkware/air/components:virtual_column.h",
        "//src/starkware/air/components/perm_table_check:table_check_cell.h",
        "//src/starkware/air/components/perm_table_check:table_check_cell.inl",
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
        "//src/starkware/air:air_test_utils",
        "//src/starkware/air/components:trace_generation_context",
        "//src/starkware/algebra",
        "//src/starkware/algebra/lde",
        "//src/starkware/gtest:starkware_gtest",
        "//src/third_party",
    ],
)

cc_test(
    name = "diluted_check_test",
    srcs = [
        "diluted_check.h",
        "diluted_check.inl",
        "diluted_check_cell.h",
        "diluted_check_cell.inl",
        "diluted_check_test.cc",
        "//src/starkware/air:trace.h",
        "//src/starkware/air/components:trace_generation_context.h",
        "//src/starkware/air/components:virtual_column.h",
        "//src/starkware/air/components/perm_table_check:table_check_cell.h",
        "//src/starkware/air/components/perm_table_check:table_check_cell.inl",
        "//src/starkware/air/components/permutation:permutation.h",
        "//src/starkware/air/components/permutation:permutation.inl",
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
        "//src/starkware/air:air_test_utils",
        "//src/starkware/air/components:trace_generation_context",
        "//src/starkware/algebra",
        "//src/starkware/algebra/lde",
        "//src/starkware/gtest:starkware_gtest",
        "//src/starkware/randomness:prng",
        "//src/third_party",
    ],
)

cc_library(
    name = "diluted_check",
    srcs = [
    ],
    hdrs = [
        "diluted_check.h",
        "diluted_check.inl",
        "diluted_check_cell.h",
        "diluted_check_cell.inl",
        "//src/starkware/air:trace.h",
        "//src/starkware/air/components:trace_generation_context.h",
        "//src/starkware/air/components:virtual_column.h",
        "//src/starkware/air/components/perm_table_check:table_check_cell.h",
        "//src/starkware/air/components/perm_table_check:table_check_cell.inl",
        "//src/starkware/air/components/permutation:permutation.h",
        "//src/starkware/air/components/permutation:permutation.inl",
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
