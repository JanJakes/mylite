# Baseline NULLIF Function Tasks

- [x] Read current compatibility, scalar select, literal projection, `IF()`,
  `IFNULL()`, `COALESCE()`, parser, runtime, result, storage, and test context.
- [x] Research official MySQL 8.4 `NULLIF()` / flow-control-function
  documentation.
- [x] Probe MySQL 8.4.9 runtime behavior for argument count, equal and unequal
  operands, `NULL` operands, booleans, integer values, labels, nested calls,
  aliases, identifiers, `FROM DUAL`, warnings, row count, syntax errors, and
  broader deferred forms.
- [x] Write the independent feature spec with MyLite grammar snippets,
  ownership boundaries, scalar evaluation semantics, diagnostics, performance
  constraints, and tests.
- [x] Add MySQL-runtime expectation script for this feature.
- [x] Update compatibility documentation for the exact limited subset.
- [x] Commit and push the start-feature artifacts.
- [x] Add lexer/parser/AST support and parser tests for `NULLIF()`.
- [x] Extend the no-source/`FROM DUAL` scalar projection runtime to evaluate
  supported `NULLIF()` expressions without SQLite SQL.
- [x] Add runtime lifecycle tests for values, labels, aliases, nested calls,
  warnings, row count, file safety, independent handles, identifiers, and
  deterministic rejection of unsupported broader forms.
- [x] Register any new test binary in `packages/libmylite/CMakeLists.txt`.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for architecture boundaries, expression-scope
  control, performance, cleanup, compatibility wording, and test relevance.
- [x] Commit, push `main`, and continue to the next baseline slice.
