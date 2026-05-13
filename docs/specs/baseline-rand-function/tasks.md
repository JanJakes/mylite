# Baseline RAND Function Tasks

- [x] Verify `RAND()` behavior against MySQL 8.4.9 for range, labels,
  whitespace, `DUAL`, `DO`, bare identifier behavior, seeded deferral, and
  argument-count diagnostics.
- [x] Specify the narrow supported grammar, runtime randomness source,
  diagnostics, ownership boundaries, and deferred surfaces.
- [x] Add parser/AST support for `RAND()` with deterministic unsupported seed
  and argument-count marker nodes.
- [x] Add runtime scalar evaluation for no-source/`DUAL`/`DO` `RAND()` using
  SQLite public randomness.
- [x] Add MySQL-runtime expectation script and fast C parser/runtime tests.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [x] Review, commit, and push to `origin/main`.
