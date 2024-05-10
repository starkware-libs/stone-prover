FROM ciimage/python:3.9 as base_image

# Show executed shell commands
RUN set -o xtrace
# Exit on error.
RUN set -e
RUN apt update
RUN apt install -y python3.9-dev git wget gnupg2 elfutils libdw-dev python3-pip libgmp3-dev unzip
RUN pip install --upgrade pip
RUN pip install cpplint pytest numpy
RUN apt install -y clang-12 clang-format-12 clang-tidy-6.0 libclang-12-dev llvm-12
RUN ln /usr/bin/clang++-12 /usr/bin/clang++
RUN ln /usr/bin/clang-12 /usr/bin/clang

WORKDIR /app/

RUN curl -L -o /tmp/bazel_install.sh https://github.com/bazelbuild/bazel/releases/download/5.4.0/bazel-5.4.0-installer-linux-x86_64.sh
RUN chmod +x /tmp/bazel_install.sh
RUN /tmp/bazel_install.sh
RUN groupadd -g 1234 starkware && useradd -m starkware -u 1234 -g 1234

RUN chown -R starkware:starkware /app

COPY WORKSPACE /app/
COPY .bazelrc /app/
COPY src /app/src
COPY bazel_utils /app/bazel_utils

# Build.
RUN bazel build //...

FROM base_image as intermediate_image

# Install rust
RUN apt install -y unzip
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Clone cairo-vm
WORKDIR /cairo-vm
RUN wget https://github.com/lambdaclass/cairo-vm/archive/main.zip -O cairo-vm.zip
RUN unzip cairo-vm.zip && mv cairo-vm-main/* . && rm -r cairo-vm-main cairo-vm.zip

# Install cairo-vm
WORKDIR /cairo-vm/cairo1-run
RUN cargo build --release
WORKDIR /cairo-vm-bin
RUN mv /cairo-vm/target/release/cairo1-run .

# "Link" the cairo corelib
RUN wget https://github.com/starkware-libs/cairo/archive/refs/tags/v2.6.3.zip -O cairo.zip
RUN unzip cairo.zip && mv cairo-2.6.3/corelib . && rm -r cairo-2.6.3 cairo.zip

ENV PATH="/cairo-vm-bin:${PATH}"

RUN which cairo1-run
RUN pwd
RUN ls

FROM intermediate_image

# Link cpu_air_prover.
RUN ln /app/build/bazelbin/src/starkware/main/cpu/cpu_air_prover /bin/cpu_air_prover
COPY config-generator.py /bin/
COPY prover-entrypoint.sh /bin/

WORKDIR /tmp/workspace
COPY cpu_air_prover_config.json .
RUN mv /cairo-vm-bin/corelib . 
COPY config-generator.py .
