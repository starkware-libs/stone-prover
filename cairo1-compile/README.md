
# Installation Guide

Follow the steps below to install `cairo1-compile` and `cairo1-run`.

## Install `cairo1-compile`

To install `cairo1-compile`, run the following command:

```sh
cargo install --path cairo1-compile
```

## Install `cairo1-run`

To install `cairo1-run` from the repository, run the following command:

```sh
cargo install --git https://github.com/lambdaclass/cairo-vm cairo1-run
```


`cairo1-compile compile --output resources/fibonacci_compiles.sierra.json e2e_test/Cairo/fibonacci.cairo`
`cairo1-compile merge -o resources/fibonacci_prover_input.json resources/fibonacci_compiles.sierra.json e2e_test/Cairo/input.json`
