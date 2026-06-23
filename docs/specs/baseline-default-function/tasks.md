# Baseline DEFAULT() Function Tasks

- [x] Research official MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior.
- [x] Define the narrow MyLite grammar, ownership boundary, runtime semantics, and diagnostics.
- [x] Prepare a MySQL 8.4.9 expectation script for the admitted user-visible behavior.
- [x] Add parser and AST support for `DEFAULT(column_name)`.
- [x] Resolve source columns through MyLite descriptors in row-scalar SELECT and supported DML value positions.
- [x] Materialize supported literal descriptor defaults without consulting SQLite schema text.
- [x] Reject expression defaults, missing defaults, unknown columns, generated current date/time defaults, and unsupported target conversions deterministically.
- [x] Add parser and runtime C coverage for SELECT, INSERT, REPLACE, UPDATE, duplicate-key update, diagnostics, persistence, and cleanup paths.
- [x] Update compatibility documentation for the exact supported subset.
- [x] Run the MySQL expectation script, targeted CTest entries, `cmake --build --preset dev`, and `cmake --workflow --preset check`.
- [x] Commit and push the completed slice.
- [x] Run a review pass and amend if needed.
