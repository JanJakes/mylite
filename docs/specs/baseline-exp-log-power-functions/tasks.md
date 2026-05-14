# Baseline EXP, LOG, And POW Functions Tasks

- [x] Triage existing function coverage and choose the coherent missing
  exponential/logarithm/power math batch.
- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime
  behavior for values, invalid arguments, warnings, overflow errors, arity, and
  deferred accepted forms.
- [x] Specify the narrow no-source/`DUAL`/`DO` scalar function subset,
  ownership boundaries, grammar snippets, runtime semantics, diagnostics, and
  tests.
- [x] Add MySQL-runtime expectation script for supported behavior and deferred
  forms.
- [ ] Update parser/AST support for `EXP`, `LN`, `LOG`, `LOG10`, `LOG2`,
  `POW`, and `POWER` with correct arity behavior.
- [ ] Add runtime evaluation, formatting, warning, and overflow handling.
- [ ] Add C parser/runtime coverage and CMake registration.
- [ ] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [ ] Run focused build/tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL behavior, warning/error ordering,
  top-level scope control, diagnostics, formatting, public ABI stability,
  storage/catalog isolation, cleanup safety, and docs accuracy.
