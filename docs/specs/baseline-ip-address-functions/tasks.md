# Baseline IP Address Functions Tasks

- [x] Research official MySQL 8.4 documentation and runtime-observed
      MySQL 8.4.9 behavior for `INET_ATON()` and `INET_NTOA()`.
- [x] Specify syntax, metadata, warnings, and supported contexts.
- [x] Add MySQL-runtime expectation script.
- [x] Implement parser AST specialization for the two nonreserved function
      names and native argument-count diagnostics.
- [x] Implement shared IPv4 conversion helpers and SQLite scalar UDFs.
- [x] Wire no-source scalar execution, source-backed row-scalar planning,
      metadata, and SQL lowering.
- [x] Add C runtime coverage and CMake registration.
- [x] Update compatibility docs.
- [x] Run focused MySQL/runtime checks, `git diff --check`, staged diff check,
      and `cmake --workflow --preset check`.
- [x] Release-gate review, fix findings, commit, and push.
