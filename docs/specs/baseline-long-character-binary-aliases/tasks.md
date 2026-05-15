# Baseline LONG Character Binary Aliases Tasks

- [x] Verify MySQL 8.4.9 behavior for `LONG`, `LONG VARCHAR`, and
      `LONG VARBINARY` aliases.
- [x] Add MySQL expectation coverage for normalized metadata, DML, clone/copy,
      and unsupported forms.
- [x] Extend lexer/parser/AST support for admitted alias grammar.
- [x] Route `LONG` and `LONG VARCHAR` through the existing `MEDIUMTEXT`
      descriptor path.
- [x] Route `LONG VARBINARY` through the existing `MEDIUMBLOB` descriptor path.
- [x] Add parser tests for accepted aliases and rejected length forms.
- [x] Add runtime tests for metadata, DML, add-column, clone/copy, persistence,
      and file-format safety.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run focused MySQL expectation, parser/runtime tests, and
      `cmake --workflow --preset check`.
- [x] Review, commit, push, and run a post-commit review.
