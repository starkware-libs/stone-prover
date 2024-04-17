FROM localhost/stone5-poseidon3:recursive

RUN pip install cairo-lang==0.13.1

COPY program.cairo .
RUN cairo-compile \
    --proof_mode \
    --output program_compiled.json \
    program.cairo

ENTRYPOINT [ "prover-entrypoint.sh" ]
