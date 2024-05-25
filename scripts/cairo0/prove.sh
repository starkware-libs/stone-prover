#!/usr/bin/env bash

podman run -i --rm localhost/stone-cairo0:recursive < resources/cairo0/example.json > resources/cairo0/proof.json
