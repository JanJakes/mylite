# Baseline Load Data Infile Tasks

- [x] Capture MySQL 8.4.9 behavior for default server-side `LOAD DATA INFILE`,
  column lists, skipped header lines, row-shape diagnostics, `\N`, empty
  fields, strict/nonstrict conversion, and `LOCAL` disabled diagnostics.
- [x] Write an independently authored feature specification with MyLite grammar
  snippets, runtime boundaries, diagnostics, performance, and storage policy.
- [x] Add the MySQL expectation script for this slice.
- [x] Add parser and AST support for the admitted `LOAD DATA INFILE` forms.
- [x] Add descriptor-driven runtime planning and streaming import execution.
- [x] Add parser/runtime C tests and CMake registrations.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md`.
- [x] Run the MySQL expectation script, focused parser/runtime CTests, and the
  full `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL evidence, descriptor authority,
  streaming behavior, conversion ownership, diagnostics, file-format safety,
  and scope control.
