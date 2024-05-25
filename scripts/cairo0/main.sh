#!/usr/bin/env bash

./scripts/cairo0/build.sh && \
./scripts/cairo0/venv.sh && \
./scripts/cairo0/compile.sh && \
./scripts/cairo0/merge.sh && \
./scripts/cairo0/prove.sh && \
./scripts/cairo0/verify.sh
