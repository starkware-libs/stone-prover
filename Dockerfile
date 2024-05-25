FROM ciimage/python:3.9 as base

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
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

RUN curl -L -o /tmp/bazel_install.sh https://github.com/bazelbuild/bazel/releases/download/5.4.0/bazel-5.4.0-installer-linux-x86_64.sh
RUN chmod +x /tmp/bazel_install.sh
RUN /tmp/bazel_install.sh

WORKDIR /app/
COPY WORKSPACE /app/
COPY .bazelrc /app/
COPY src /app/src
COPY bazel_utils /app/bazel_utils


FROM base as build
# Build.
WORKDIR /app/
RUN bazel build //...
RUN cargo install --git https://github.com/lambdaclass/cairo-vm --rev f4a22140018f62309ade09ecd517b40e915031b1 cairo1-run


FROM python:3.12.3-slim-bookworm as final
RUN apt update && apt install -y elfutils jq

COPY --from=build /root/.cargo/bin/cairo1-run /usr/local/bin/cairo1-run
COPY --from=build /app/build/bazelbin/src/starkware/main/cpu/cpu_air_prover /usr/local/bin/stone

COPY config-generator.py /usr/local/bin/config-generator.py
COPY prover-entrypoint.sh /usr/local/bin/prover-entrypoint.sh

WORKDIR /tmp/workspace
COPY cpu_air_prover_config.json cpu_air_prover_config.json
COPY corelib corelib

ENTRYPOINT [ "prover-entrypoint.sh" ]
