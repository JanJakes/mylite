# Baseline HAVING Grouped Aggregate Tasks

- [x] Read grouped aggregate, parser, runtime, catalog, result, storage, and
  compatibility context.
- [x] Research official MySQL 8.4 `SELECT`, `GROUP BY`, `HAVING`, and aggregate
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for supported `HAVING` predicates,
  alias resolution, null tests, clause ordering, and diagnostics.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Add parser grammar, AST kind/name, keyword mapping, and parser helpers for
  `HAVING`.
- [x] Add descriptor-driven grouped aggregate `HAVING` planning, SQL generation,
  and parameter binding.
- [x] Add parser and runtime lifecycle tests, including persistence,
  diagnostics, generation stability, preamble preservation, and independent
  file-backed handles.
- [x] Register any new tests in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, descriptor authority,
  performance, cleanup, compatibility wording, and test relevance.
- [x] Commit the implementation slice.
