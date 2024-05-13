FROM localhost/stone5-cairo1

COPY test_project/target/dev/test_project.sierra.json ./cairo1.sierra

ENTRYPOINT [ "prover-entrypoint.sh" ]
