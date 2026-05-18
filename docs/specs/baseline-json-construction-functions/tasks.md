# Baseline JSON Construction Functions Tasks

## Goal

Add a limited MySQL-compatible `JSON_ARRAY()` and `JSON_OBJECT()` slice for
common scalar and row-scalar JSON construction.

## Tasks

1. Design and evidence
   - Create `docs/specs/baseline-json-construction-functions/specs.md`.
   - Verify MySQL 8.4.9 behavior for admitted constructor values, object keys,
     diagnostics, labels, row count, and warning count.
   - Add a MySQL expectation script for the verified behavior.

2. Parser and AST
   - Add lexer/parser/AST support for `JSON_ARRAY()` and
     `JSON_ARRAY(value[, ...])`.
   - Add lexer/parser/AST support for `JSON_OBJECT()` and
     `JSON_OBJECT(key, value[, ...])`.
   - Preserve `JSON_ARRAY` and `JSON_OBJECT` as identifiers where MyLite's
     identifier grammar permits nonreserved function names.

3. Runtime
   - Extend the MyLite JSON runtime with construction helpers for supported SQL
     values.
   - Register variadic MyLite SQLite scalar callbacks for row-scalar
     construction through public SQLite APIs.
   - Implement scalar, `DUAL`, `DO`, and row-scalar planning for the supported
     subset.
   - Bind literal values and quote descriptor column identifiers; do not
     interpolate SQL literals.
   - Preserve current unsupported boundaries for broad expressions,
     predicates, ordering expressions, DML assignment values, and JSON mutation.

4. Tests and docs
   - Add focused parser and runtime tests.
   - Update `COMPATIBILITY.md` and JSON compatibility docs with exact partial
     wording.
   - Run the MySQL expectation script, focused CTests, build, and full check
     workflow.

5. Review and publish
   - Review the final diff with a subagent.
   - Amend any actionable findings.
   - Commit atomically and push `origin/main`.
