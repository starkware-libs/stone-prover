cc_library(
    name = "backward-cpp",
    hdrs = [
        "backward.hpp",
    ],
    visibility = ["//visibility:public"],
    linkopts = select({
        "@platforms//os:linux": [
            "-ldw",
        ],
        "//conditions:default": [
        ],
    }),
)
