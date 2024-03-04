def load_targets(
        tags = []):
    """
    Generates additonal targets.
    """

    native.cc_library(
        name = "permutation_dummy_air_lib",
        hdrs = [
            "permutation_dummy_air.h",
            "permutation_dummy_air.inl",
            "permutation_dummy_air_definition.h",
            "permutation_dummy_air_definition_class.h",
            "permutation_dummy_air_definition0.h",
            "permutation_dummy_air_definition0.inl",
        ],
        tags = ["external_prover"],
        deps = [
            ":permutation",
        ],
    )
