Build the docker image:

```bash
podman build -t stone-cairo1:recursive .
```

Push the image to registry (optional):

```bash
podman push localhost/stone-cairo1:recursive docker.io/username/stone-cairo1:recursive
```

Install the `cairo1-compile`:

```bash
cargo install --path cairo1-compile
```

For the compilation to work, the `cairo` corelib folder is required. A couple of paths are permitted but the easiest way is to have it in your home folder:

```bash
cd cairo && git pull && cd ..
```

```bash
rm -rf corelib && cp -r cairo/corelib corelib
```

The example directory showcases the workflow of using the image to proof arbitrary cairo1 progams.

```bash
cd example
```

Compile the cairo program:

```bash
sierra_compile_sdk compile lib.cairo -o program.sierra.json
```

Merge the program with the input:

```bash
sierra_compile_sdk merge program.sierra.json input.json -o merged.json
```

Compute the proof:

```bash
podman run -i --rm stone5-cairo1 < merged.json > proof.json
```

Verify

```bash
podman run -i --rm verifier < proof.json
```

## Creating and verifying a proof of a Cairo program

Navigate to the example test directory (`e2e_test/Cairo`):

```bash
cd e2e_test/Cairo
```

Install `cairo-vm/cairo1-run` (see further instructions in the
[cairo-vm repository](https://github.com/lambdaclass/cairo-vm)):

```bash
git clone https://github.com/lambdaclass/cairo-vm.git
cd cairo-vm/cairo1-run
make deps
```

Compile and run the program to generate the prover input files:

```bash
cargo run ../../fibonacci.cairo \
    --layout=small \
    --air_public_input=fibonacci_public_input.json \
    --air_private_input=fibonacci_private_input.json \
    --trace_file=fibonacci_trace.json \
    --memory_file=fibonacci_memory.json \
    --proof_mode
```

Run the prover:

```bash
cpu_air_prover \
    --out_file=fibonacci_proof.json \
    --private_input_file=fibonacci_private_input.json \
    --public_input_file=fibonacci_public_input.json \
    --prover_config_file=../../cpu_air_prover_config.json \
    --parameter_file=../../cpu_air_params.json
```

The proof is now available in the file `fibonacci_proof.json`.

Finally, run the verifier to verify the proof:

```bash
cpu_air_verifier --in_file=fibonacci_proof.json && echo "Successfully verified example proof."
```

**Note**: The verifier only checks that the proof is consistent with
the public input section that appears in the proof file.
The public input section itself is not checked.
For example, the verifier does not check what Cairo program is being proved,
or that the builtins memory segments are of valid size.
These things need to be checked externally.

## Creating and verifying a proof of a CairoZero program

To run and prove the example program `fibonacci.cairo`,
install `cairo-lang` version 0.12.0 (see further instructions in the
[cairo-lang repository](https://github.com/starkware-libs/cairo-lang/tree/v0.12.0)):

```bash
pip install cairo-lang==0.12.0
```

Navigate to the example test directory (`e2e_test/CairoZero`):

```bash
cd e2e_test/CairoZero
```

Compile `fibonacci.cairo`:

```bash
cairo-compile fibonacci.cairo --output fibonacci_compiled.json --proof_mode
```

Run the compiled program to generate the prover input files:

```bash
cairo-run \
    --program=fibonacci_compiled.json \
    --layout=small \
    --program_input=fibonacci_input.json \
    --air_public_input=fibonacci_public_input.json \
    --air_private_input=fibonacci_private_input.json \
    --trace_file=fibonacci_trace.json \
    --memory_file=fibonacci_memory.json \
    --print_output \
    --proof_mode
```

Run the prover:

```bash
cpu_air_prover \
    --out_file=fibonacci_proof.json \
    --private_input_file=fibonacci_private_input.json \
    --public_input_file=fibonacci_public_input.json \
    --prover_config_file=cpu_air_prover_config.json \
    --parameter_file=cpu_air_params.json
```

The proof is now available in the file `fibonacci_proof.json`.

Finally, run the verifier to verify the proof:

```bash
cpu_air_verifier --in_file=fibonacci_proof.json && echo "Successfully verified example proof."
```

**Note**: The verifier only checks that the proof is consistent with
the public input section that appears in the proof file.
The public input section itself is not checked.
For example, the verifier does not check what CairoZero program is being proved,
or that the builtins memory segments are of valid size.
These things need to be checked externally.

## Configuration for other input sizes

The number of steps affects the size of the trace.
Such changes may require modification of `cpu_air_params.json`.
Specifically, the following equation must be satisfied.

```
log₂(last_layer_degree_bound) + ∑fri_step_list = log₂(#steps) + 4
```

For instance, assuming a fixed `last_layer_degree_bound`,
a larger number of steps requires changes to the `fri_step_list`
to maintain the equality.

FRI steps should typically be in the range 2-4;
the degree bound should be in the range 4-7.

The constant 4 that appears in the equation is hardcoded `log₂(trace_rows_per_step) = log₂(16) = 4`.

`cargo run -r -- compile example/lib.cairo > resources/compiled.sierra.json`
`cargo run -r -- merge resources/compiled.sierra.json example/input.json > resources/prover_input.json`
`podman run -i stone-prover-cairo1:recursive < resources/prover_input.json > resources/proof.json`
`cairo-proof-parser-output < resources/proof.json`
