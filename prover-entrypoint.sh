#!/usr/bin/env bash

# Read from stdin
cat > args.json && \

jq -r '.program_input | join(" ")' args.json | tr -d '\n' > args.txt && \
jq '.program' args.json > program.sierra && \

cairo1-run \
    program.sierra \
    --layout recursive \
    --args_file args.txt \
    --trace_file program_trace.bin \
    --memory_file program_memory.bin \
    --proof_mode \
    --air_public_input program_public_input.json \
    --air_private_input program_private_input.json && \
    2>&1 > /dev/null && \

python3 config-generator.py < program_public_input.json > cpu_air_params.json && \

stone \
    --out_file program_proof.json \
    --private_input_file program_private_input.json \
    --public_input_file program_public_input.json \
    --prover_config_file cpu_air_prover_config.json \
    --parameter_file cpu_air_params.json \
    -generate_annotations && \

cat program_proof.json
