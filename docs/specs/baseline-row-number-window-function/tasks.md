# Baseline ROW_NUMBER Window Function Tasks

- [x] Record MySQL 8.4.9 expectations for the supported and rejected syntax.
- [x] Add parser and AST support for the narrow `ROW_NUMBER() OVER (...)` grammar.
- [x] Extend row-scalar SELECT planning to detect projection-only `ROW_NUMBER()`.
- [x] Resolve window partition/order columns from MyLite descriptors.
- [x] Generate descriptor-built SQLite `row_number() OVER (...)` SQL.
- [x] Add result metadata for non-null unsigned `BIGINT` row-number output.
- [x] Add parser/runtime tests for success, diagnostics, metadata, and warnings.
- [x] Update `COMPATIBILITY.md` and window-function compatibility docs.
- [x] Run targeted verification and `cmake --workflow --preset check`.
- [x] Review, commit, and push the completed slice.
