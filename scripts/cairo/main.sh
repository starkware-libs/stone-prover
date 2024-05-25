#!/usr/bin/env bash

./scripts/cairo/build.sh && \
./scripts/cairo/compile.sh && \
./scripts/cairo/merge.sh && \
./scripts/cairo/prove.sh && \
./scripts/cairo/verify.sh
