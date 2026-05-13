# Baseline RAND Function Tasks

- [x] Verify `RAND()` behavior against MySQL 8.4.9 for range, labels,
  whitespace, `DUAL`, `DO`, bare identifier behavior, seeded deferral, and
  argument-count diagnostics.
- [x] Specify the narrow supported grammar, runtime randomness source,
  diagnostics, ownership boundaries, and deferred surfaces.
- [ ] Add parser/AST support for `RAND()` with deterministic unsupported seed
  and argument-count marker nodes.
- [ ] Add runtime scalar evaluation for no-source/`DUAL`/`DO` `RAND()` using
  SQLite public randomness.
- [ ] Add MySQL-runtime expectation script and fast C parser/runtime tests.
- [ ] Update compatibility documentation for the exact supported subset.
- [ ] Run focused tests, MySQL expectation script, and
  `cmake --workflow --preset check`.
- [ ] Review, commit, and push to `origin/main`.
