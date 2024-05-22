#!/usr/bin/env bash

cairo1-compile compile \
    --output resources/fibonacci_compiled.sierra.json \
    e2e_test/Cairo/fibonacci.cairo && \

cairo1-compile merge \
    --output resources/fibonacci_prover_input.json \
    resources/fibonacci_compiled.sierra.json e2e_test/Cairo/input.json && \

podman run -i \
    --rm stone5-cairo1:recursive \
    < resources/fibonacci_prover_input.json \
    > resources/proof.json
