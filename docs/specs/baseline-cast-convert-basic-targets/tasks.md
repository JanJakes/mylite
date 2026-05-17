# Baseline CAST/CONVERT Basic Targets Tasks

- [x] Research official MySQL 8.4 cast documentation and observed MySQL 8.4.9 runtime behavior.
- [x] Specify the independently authored supported grammar and runtime semantics.
- [x] Add a MySQL 8.4.9 expectation script for the new user-visible behavior.
- [x] Extend parser/AST support for `CHAR`, `SIGNED [INTEGER|INT]`, and `UNSIGNED [INTEGER|INT]` cast targets.
- [x] Implement MyLite-owned scalar conversion and staged warning behavior.
- [x] Add parser and runtime C tests for values, warnings, diagnostics, and unsupported shapes.
- [x] Update compatibility detail docs to match the implemented subset.
- [x] Run focused parser/runtime tests.
- [x] Keep the MySQL 8.4.9 expectation script aligned with the implemented behavior; fresh rerun is currently blocked by an unresponsive local Docker daemon.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff, commit atomically, and push `origin main`.
