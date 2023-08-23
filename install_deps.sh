#!/bin/bash

# Show executed shell commands
set -o xtrace
# Exit on error.
set -e

apt update
apt install -y python3.9-dev git wget gnupg2 elfutils libdw-dev python3-pip libgmp3-dev

pip install cpplint pytest numpy
pip install cmake

apt install -y clang-12 clang-format-12 clang-tidy-6.0 libclang-12-dev llvm-12

ln -sf /usr/bin/clang++-12 /usr/bin/clang++
ln -sf /usr/bin/clang-12 /usr/bin/clang

# Install gtest.
(
cd /tmp
rm -rf /tmp/googletest
git clone -b release-1.8.0 https://github.com/google/googletest.git
cd /tmp/googletest
cmake CMakeLists.txt && make -j && make install
)

# Install gflags.
(
cd /tmp
rm -rf /tmp/gflags
git clone -b v2.2.1 https://github.com/gflags/gflags.git
cd /tmp/gflags
cmake CMakeLists.txt && make -j && make install
)

# Install glog.
(
cd /tmp
rm -rf /tmp/glog
git clone -b v0.3.5 https://github.com/google/glog.git
cd glog
cmake CMakeLists.txt && make -j && make install
)

# Install google benchmark.
(
cd /tmp
rm -rf /tmp/benchmark
git clone -b v1.4.0 https://github.com/google/benchmark.git
cd /tmp/benchmark
cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release && make -j && make install
)
