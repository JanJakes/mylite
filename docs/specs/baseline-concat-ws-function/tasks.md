# Baseline CONCAT_WS Function Tasks

## Goal

Add a limited MySQL-compatible `CONCAT_WS()` scalar and row-scalar projection
slice for common application string assembly.

## Tasks

1. Design and evidence
   - Create `docs/specs/baseline-concat-ws-function/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Add a MySQL expectation script for result values, diagnostics, labels,
     row count, and warning count.

2. Parser and AST
   - Add a `CONCAT_WS` lexer/parser token.
   - Add `CONCAT_WS()` AST nodes for function calls and wrong zero-argument
     calls.
   - Add parser tests for accepted calls, whitespace, identifier fallback, and
     wrong-argument-count shapes.

3. Runtime
   - Add a MyLite-owned SQLite scalar helper for `_mylite_concat_ws`.
   - Plan `CONCAT_WS()` in the existing row-scalar expression path.
   - Bind literal/session arguments and reference descriptor columns without
     interpolating SQL literal text.
   - Preserve current unsupported boundaries for nested functions, predicates,
     ordering expressions, DML assignments, and binary operands.

4. Tests and docs
   - Add a focused C runtime test or extend existing row-scalar tests.
   - Update compatibility docs only for the limited supported surface.
   - Run the MySQL expectation script, parser/runtime CTest entries, build, and
     full check workflow.

5. Review and publish
   - Review the final diff with a subagent.
   - Amend any actionable findings.
   - Commit atomically and push `main`.
