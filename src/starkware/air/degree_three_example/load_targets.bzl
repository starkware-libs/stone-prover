def load_targets(
        tags = []):
    """
    Generates additonal targets.
    """

    native.cc_library(
        name = "degree_three_example_air_lib",
        hdrs = [
            "degree_three_example_air.h",
            "degree_three_example_air.inl",
            "degree_three_example_air0.h",
            "degree_three_example_air0.inl",
            "degree_three_example_air_class.h",
        ],
    )
