FROM localhost/stone5-cairo1

COPY cairo1.cairo .
COPY config-generator.py .

ENTRYPOINT [ "prover-entrypoint.sh" ]
