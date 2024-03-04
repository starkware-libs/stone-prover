def load_targets(
        tags = []):
    """
    Generates additonal targets.
    """

    native.cc_library(
        name = "fibonacci_air_lib",
        hdrs = [
            "fibonacci_air.h",
            "fibonacci_air.inl",
            "fibonacci_air0.h",
            "fibonacci_air0.inl",
            "fibonacci_air_class.h",
        ],
        tags = ["external_prover"],
    )
