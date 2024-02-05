#!/bin/bash

#find the package manager
declare -A os_info
os_info['/etc/redhat-release']='yum'
os_info['/etc/arch-release']='pacman'
os_info['/etc/debian_version']='apt'
os_info['/etc/alpine-release']='apk'

for f in "${!os_info[@]}"; do
    if [ -f "$f" ]; then
        package_manager="${os_info[$f]}"
        break
    fi
done


# Show executed shell commands
set -o xtrace
# Exit on error.
set -e


with_apt() {
    apt update
    apt install -y python3.9-dev git wget gnupg2 elfutils libdw-dev python3-pip libgmp3-dev

    pip install cpplint pytest numpy
    pip install cmake

    apt install -y clang-12 clang-format-12 clang-tidy-6.0 libclang-12-dev llvm-12
}

with_yum() {
    yum update
    yum install -y python39-devel git wget gnupg2 elfutils-libelf-devel python3-pip gmp-devel

    pip install cpplint pytest numpy
    pip install cmake

    yum install -y clang clang-format clang-tools-extra llvm-devel
}

with_dnf() {
    dnf update
    dnf install -y python39-devel git wget gnupg2 elfutils-libelf-devel python3-pip gmp-devel

    pip install cpplint pytest numpy
    pip install cmake

    dnf install -y clang clang-format clang-tools-extra llvm-devel
}

with_apk() {

apk update

apk add python3-dev git wget gnupg elfutils-dev dw-dev py3-pip gmp-dev clang clang-dev llvm llvm-dev clang-tools clang-extra-tools

pip install cpplint pytest numpy cmake

}

with_pacman() {
    pacman -Syu --noconfirm
    pacman -S --noconfirm python39 git wget gnupg elfutils libdw python-pip gmp

    pip install cpplint pytest numpy
    pip install cmake

    pacman -S --noconfirm clang clang-format clang-tools-extra llvm
}

case $package_manager in
  yum)
    with_yum
    ;;
  pacman)
    with_pacman
    ;;
  apt)
    with_apt
    ;;
  apk)
    with_apk
    ;;
  *)
    echo "no package manager found"
    exit 1
    ;;
esac

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
