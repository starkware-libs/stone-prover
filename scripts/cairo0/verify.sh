#!/usr/bin/env bash

podman run -i --rm localhost/stone-verifier:latest < resources/cairo0/proof.json 
