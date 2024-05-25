#!/usr/bin/env bash

mkdir -p resources/cairo && \
cargo run -r -- compile example/cairo/src/lib.cairo > resources/cairo/example.sierra.json
