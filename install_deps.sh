#!/bin/bash

# Show executed shell commands
set -o xtrace
# Exit on error.
set -e

apt update
apt install -y python3.9-dev git wget gnupg2 elfutils libdw-dev python3-pip libgmp3-dev unzip

pip install --upgrade pip
pip install cpplint pytest numpy

apt install -y clang-12 clang-format-12 clang-tidy-6.0 libclang-12-dev llvm-12

ln -sf /usr/bin/clang++-12 /usr/bin/clang++
ln -sf /usr/bin/clang-12 /usr/bin/clang

