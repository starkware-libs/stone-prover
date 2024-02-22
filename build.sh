#!/bin/bash

set -e

os=$(uname)

if [ "$os" == "Linux" ]; then

    docker build --tag stone-prover .
    rm -rf targets/release
    mkdir -p targets/release
    container_id=$(docker create stone-prover)
    docker cp -L ${container_id}:/bin/cpu_air_prover targets/release
    docker cp -L ${container_id}:/bin/cpu_air_verifier targets/release

elif [ "$os" == "Darwin" ]; then

    export PATH=/opt/homebrew/bin/:$PATH

    dwarf_version=$(brew info dwarfutils | grep -o 'stable [0-9.]*' | cut -d ' ' -f 2) 
    export LIBRARY_PATH=/usr/local/lib:/usr/lib/:/opt/homebrew/Cellar/dwarfutils/$dwarf_version/lib/:$LIBRARY_PATH

    mkdir -p build/Release
    cd build/Release
    cmake ../.. -DCMAKE_BUILD_TYPE=Release
    make -j$($(sysctl -n hw.ncpu) - 2)
    mkdir -p ../../targets/release
    cp src/starkware/main/cpu/cpu_air_prover src/starkware/main/cpu/cpu_air_verifier ../../targets/release

else

    echo "Operating system not supported"
    exit 1

fi
