#!/usr/bin/env bash

source .venv/bin/activate && \
mkdir -p resources/cairo0 && \
cairo-compile \
  --output resources/cairo0/example.casm.json \
  --proof_mode \
  example/cairo0/main.cairo && \
deactivate
