# Baseline JSON_TABLE literal table source tasks

- [x] Verify MySQL 8.4.9 behavior for literal rows, root and array row paths,
      ordinality, typed PATH, EXISTS PATH, metadata, and alias diagnostics.
- [x] Add parser and AST support for `JSON_TABLE()` as a table source.
- [x] Plan literal JSON documents into a synthetic select source.
- [x] Bind synthetic row values through normal SQLite parameters.
- [x] Add MySQL-runtime expectation script and C runtime tests.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/functions-json.md`.
- [x] Run focused verification, full check workflow, release-gate review,
      commit, and push.
