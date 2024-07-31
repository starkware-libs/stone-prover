FROM ubuntu:22.04 AS build

WORKDIR /app

COPY . .

# Install dependencies.
RUN ./install_deps.sh

# Build.
RUN bazelisk build //...

FROM build AS test

# Run tests.
RUN bazelisk test //...

# Copy cpu_air_prover and cpu_air_verifier.
RUN ln -s /app/build/bazelbin/src/starkware/main/cpu/cpu_air_prover /bin/cpu_air_prover
RUN ln -s /app/build/bazelbin/src/starkware/main/cpu/cpu_air_verifier /bin/cpu_air_verifier

# End to end test.
WORKDIR /app/e2e_test/CairoZero

RUN cairo-compile fibonacci.cairo --output fibonacci_compiled.json --proof_mode

RUN cairo-run \
    --program=fibonacci_compiled.json \
    --layout=small \
    --program_input=fibonacci_input.json \
    --air_public_input=fibonacci_public_input.json \
    --air_private_input=fibonacci_private_input.json \
    --trace_file=fibonacci_trace.json \
    --memory_file=fibonacci_memory.json \
    --print_output \
    --proof_mode

RUN cpu_air_prover \
    --out_file=fibonacci_proof.json \
    --private_input_file=fibonacci_private_input.json \
    --public_input_file=fibonacci_public_input.json \
    --prover_config_file=cpu_air_prover_config.json \
    --parameter_file=cpu_air_params.json

RUN cpu_air_verifier --in_file=fibonacci_proof.json && echo "Successfully verified example proof."
