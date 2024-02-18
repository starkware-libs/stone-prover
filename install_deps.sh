#!/bin/bash

# Show executed shell commands
set -o xtrace
# Exit on error.
set -e

os=$(uname)

if [ "$os" == "Darwin" ]; then
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    export PATH=/opt/homebrew/bin/:$PATH

    # Python3.9
    brew install python@3.9
    pip3 install --upgrade pip

    # Git
    brew install git

    # wget
    brew install wget

    # gnupg2
    brew install gnupg

    # libgmp3-dev
    brew install gmp

    # libgflags-dev
    brew install gflags

    # CMake
    brew install cmake

    # Clang and LLVM
    brew install llvm@12

    # libdwarf
    brew install dwarfutils

	dwarf_version=$(brew info dwarfutils | grep -o 'stable [0-9.]*' | cut -d ' ' -f 2) 
	export LIBRARY_PATH=/usr/local/lib:/usr/lib/:/opt/homebrew/Cellar/dwarfutils/$dwarf_version/lib/:$LIBRARY_PATH


    rm -rf /tmp/benchmark /tmp/glog /tmp/gflags /tmp/googletest

    # Install gtest
    git clone -b release-1.8.0 https://github.com/google/googletest.git /tmp/googletest
    cd /tmp/googletest
    cmake CMakeLists.txt && make -j && sudo make install
    cd -

    # Install gflags
    git clone -b v2.2.1 https://github.com/gflags/gflags.git /tmp/gflags
    cd /tmp/gflags
    cmake CMakeLists.txt && make -j && sudo make install
    cd -

    # Install glog
    git clone -b v0.3.5 https://github.com/google/glog.git /tmp/glog
    cd /tmp/glog
    cmake CMakeLists.txt && make -j && sudo make install
    cd -

    # Install google benchmark
    git clone -b v1.4.0 https://github.com/google/benchmark.git /tmp/benchmark
    cd /tmp/benchmark
	sed '/Werror/d' CMakeLists.txt > CMakeLists_temp.txt && mv CMakeLists_temp.txt CMakeLists.txt
    cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release && make -j && sudo make install
    cd -

    # Modifier les permissions pour les répertoires d'installation
    sudo chown -R $(whoami) /usr/local/include /usr/local/lib /usr/local/lib64 /usr/local/bin
else
    export DEBIAN_FRONTEND=noninteractive

    apt update
    apt install -y python3.9-dev git wget gnupg2 elfutils libdw-dev python3-pip libgmp3-dev libgflags-dev

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
fi
