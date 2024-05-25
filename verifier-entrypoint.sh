#!/usr/bin/env bash

# Read from stdin
cat > proof.json && \

verifier --in_file=proof.json && \
echo "Successfully verified example proof."
