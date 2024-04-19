#!/usr/bin/env bash

# Read from stdin
cat > program_input.json && \

cairo-run \
    --program program_compiled.json \
    --layout recursive \
    --program_input program_input.json \
    --air_public_input program_public_input.json \
    --air_private_input program_private_input.json \
    --trace_file program_trace.bin \
    --memory_file program_memory.bin \
    --proof_mode \
    2>&1 > /dev/null && \

config-generator.py < program_public_input.json > cpu_air_params.json && \

cpu_air_prover \
    --out_file program_proof.json \
    --private_input_file program_private_input.json \
    --public_input_file program_public_input.json \
    --prover_config_file cpu_air_prover_config.json \
    --parameter_file cpu_air_params.json \
    -generate_annotations && \

cat program_proof.json
