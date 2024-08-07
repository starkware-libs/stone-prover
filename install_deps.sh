#!/bin/bash

# Show executed shell commands
set -o xtrace
# Exit on error.
set -e

os=$(uname | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | sed s/aarch64/arm64/ | sed s/x86_64/amd64/)

if [ "$os" == "linux" ]; then
    export DEBIAN_FRONTEND=noninteractive

    apt-get update
    apt-get install -y git wget libtinfo5 libdw-dev libgmp3-dev python3 python3-pip

    pip install --upgrade pip
    pip install cpplint pytest numpy sympy==1.12.1 cairo-lang==0.12.0

    wget "https://github.com/bazelbuild/bazelisk/releases/download/v1.20.0/bazelisk-$os-$arch"
    chmod 755 "bazelisk-$os-$arch"
    mv "bazelisk-$os-$arch" /bin/bazelisk

elif [ "$os" == "darwin" ]; then
    # Homebrew is most likely already installed. If not, uncomment the next line.
    # /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    brew install git
    brew install gmp
    brew install python@3.9

    python3 -m pip install --upgrade pip
    python3 -m pip install cpplint pytest numpy sympy==1.12.1 cairo-lang==0.12.0
    
    brew install bazelisk

else
    echo "$os/$arch is not supported"
    exit 1

fi
