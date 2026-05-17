# Baseline CAST/CONVERT Basic Targets Tasks

- [x] Research official MySQL 8.4 cast documentation and observed MySQL 8.4.9 runtime behavior.
- [x] Specify the independently authored supported grammar and runtime semantics.
- [x] Add a MySQL 8.4.9 expectation script for the new user-visible behavior.
- [ ] Extend parser/AST support for `CHAR`, `SIGNED [INTEGER|INT]`, and `UNSIGNED [INTEGER|INT]` cast targets.
- [ ] Implement MyLite-owned scalar conversion and staged warning behavior.
- [ ] Add parser and runtime C tests for values, warnings, diagnostics, and unsupported shapes.
- [ ] Update compatibility detail docs to match the implemented subset.
- [ ] Run focused parser/runtime tests and the MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff, commit atomically, and push `origin main`.
