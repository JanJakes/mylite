# Baseline JSON Extraction Functions Tasks

## Goal

Add a limited MySQL-compatible JSON extraction and unquoting slice for common
scalar and row-scalar read paths.

## Tasks

1. Design and evidence
   - Create `docs/specs/baseline-json-extract-functions/specs.md`.
   - Verify MySQL 8.4.9 behavior for the admitted extraction, unquote, and path
     operator subset.
   - Add a MySQL expectation script for result values, diagnostics, labels,
     row count, and warning count.

2. Parser and AST
   - Add parser/AST support for `JSON_EXTRACT(json_doc, path)`.
   - Add parser/AST support for `JSON_UNQUOTE(value)`.
   - Add parser/AST support for `column -> path` and `column ->> path`.
   - Add argument-count diagnostics for both functions.
   - Preserve function names as identifiers where MyLite's identifier grammar
     permits them.

3. Runtime
   - Extend the MyLite JSON runtime with simple path parsing/evaluation and
     JSON string unquoting.
   - Register MyLite SQLite scalar callbacks for row-scalar extraction and
     unquoting through public SQLite APIs.
   - Implement scalar, `DUAL`, `DO`, and row-scalar planning for the supported
     subset.
   - Bind literal and path values; do not interpolate SQL literal text.
   - Preserve current unsupported boundaries for broad expressions,
     predicates, ordering expressions, DML assignment values, and path
     wildcards/ranges.

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
