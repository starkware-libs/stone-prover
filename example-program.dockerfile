FROM neotheprogramist/stone5-poseidon3:latest

RUN pip install cairo-lang==0.12.3

COPY program.cairo .
RUN cairo-compile \
    --proof_mode \
    --output program_compiled.json \
    program.cairo

ENTRYPOINT [ "prover-entrypoint.sh" ]
