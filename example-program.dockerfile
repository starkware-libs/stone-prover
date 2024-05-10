FROM localhost/stone5-cairo1

COPY cairo1.cairo .

ENTRYPOINT [ "prover-entrypoint.sh" ]
