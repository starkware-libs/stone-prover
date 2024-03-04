load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "com_github_gflags_gflags",
    commit = "986e8eed00ded8168ef4eaa6f925dc6be50b40fa",
    patches = ["//src/third_party:gflags.patch"],
    remote = "https://github.com/gflags/gflags.git",
    shallow_since = "1641684284 +0000",
)

git_repository(
    name = "com_github_glog_glog",
    commit = "96a2f23dca4cc7180821ca5f32e526314395d26a",
    patches = ["//src/third_party:glog.patch"],
    remote = "https://github.com/google/glog.git",
    shallow_since = "1553223106 +0900",
    # tag = "v0.4.0",
)

git_repository(
    name = "com_github_gtest_gtest",
    commit = "703bd9caab50b139428cea1aaff9974ebee5742e",
    remote = "https://github.com/google/googletest.git",
    shallow_since = "1570114335 -0400",
    # tag = "release-1.11.0",
)

BAZEL_TOOLCHAIN_TAG = "0.8"

BAZEL_TOOLCHAIN_SHA = "f121449dd565d59274b7421a62f3ed1f16ad7ceab4575c5b34f882ba441093bd"

http_archive(
    name = "com_grail_bazel_toolchain",
    canonical_id = BAZEL_TOOLCHAIN_TAG,
    patch_args = [
        "-p1",
    ],  # from man patch, "-pnum  or  --strip=num" Strip  the smallest prefix containing num leading slashes from each file name found in the patch file.
    patches = ["//bazel_utils/patches:llvm_toolchain.patch"],
    sha256 = BAZEL_TOOLCHAIN_SHA,
    strip_prefix = "toolchains_llvm-{tag}".format(tag = BAZEL_TOOLCHAIN_TAG),
    url = "https://starkware-third-party.s3.us-east-2.amazonaws.com/bazel/toolchains_llvm-{tag}.tar.gz".format(
        tag = BAZEL_TOOLCHAIN_TAG,
    ),
)

load("@com_grail_bazel_toolchain//toolchain:deps.bzl", "bazel_toolchain_dependencies")

bazel_toolchain_dependencies()

load("@com_grail_bazel_toolchain//toolchain:rules.bzl", "llvm_toolchain")

llvm_toolchain(
    name = "llvm_toolchain",
    compile_flags = {
        "": [
            "-DUSE_REGISTER_HINT",
            "-fno-omit-frame-pointer",
            "-fno-strict-aliasing",
            "-fPIC",
            "-Isrc",
            "-maes",
            "-mpclmul",
            "-Wall",
            "-Wextra",
            "-Wno-mismatched-tags",
        ],
    },
    cxx_flags = {
        "": [
            "-std=c++17",
            "-fconstexpr-steps=20000000",
        ],
    },
    link_flags = {
        "": [
            "-fuse-ld=gold",
            "-Wl,-no-as-needed",
            "-Wl,-z,relro,-z,now",
            "-B/usr/bin",
            "-lstdc++",
            "-lm",
        ],
    },
    llvm_version = "12.0.0",
    opt_compile_flags = {
        "": [
            "-g0",
            "-O3",
            "-D_FORTIFY_SOURCE=1",
            "-DNDEBUG",
            "-ffunction-sections",
            "-fdata-sections",
        ],
    },
    stdlib = {"": "stdc++"},
)

load("@llvm_toolchain//:toolchains.bzl", "llvm_register_toolchains")

llvm_register_toolchains()
