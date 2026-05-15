# Baseline Named UNIQUE Constraint Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior.
- [x] Specify the supported grammar, semantics, metadata, diagnostics, and storage boundary.
- [x] Add MySQL 8.4.9 expectation script for named unique constraint behavior.
- [x] Extend the parser to accept admitted `CONSTRAINT ... UNIQUE` forms.
- [x] Add parser coverage for supported and deferred forms.
- [x] Add runtime coverage for metadata, duplicate enforcement, drop-index integration, persistence, and independent handles.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused MySQL, parser, runtime, and metadata tests.
- [x] Run `cmake --workflow --preset check`.
- [x] Review, amend if needed, commit, and push to `origin/main`.
