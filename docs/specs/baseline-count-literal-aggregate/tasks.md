# Baseline COUNT Literal Aggregate Tasks

## Goal

Add the next narrow aggregate slice: `COUNT(decimal_integer_literal)` and
`COUNT(NULL)` through `mylite_execute()`, with descriptor-backed table forms
still lowered to SQLite aggregates and optional baseline predicate conversion.

## Tasks

1. Design and documentation
   - [x] Create `docs/specs/baseline-count-literal-aggregate/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, no-space function-call behavior, source/table
     resolution, predicate reuse, generated SQLite SQL, diagnostics, result
     behavior, row-count behavior, and unsupported aggregate forms.
   - [x] Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - [x] Add a reproducible MySQL 8.4.9 expectation script for supported
     `COUNT(integer)` / `COUNT(NULL)` behavior and intentionally rejected or
     deferred wider forms.
   - [x] Verify no-source, `DUAL`, empty/nonempty tables, no-match predicates,
     labels, warnings, following `ROW_COUNT()`, and diagnostics for
     syntax-rejected forms.
   - [x] Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - [x] Add an AST node for one-literal `COUNT()` aggregate functions.
   - [x] Extend Lemon grammar for no-space `COUNT(count_literal)`, including
     whitespace or comments inside the argument list.
   - [x] Preserve source spans for default result labels.
   - [x] Preserve `COUNT` as an identifier where the grammar admits
     nonreserved function-name identifiers.
   - [x] Add parser tests for supported count-literal syntax and unsupported
     argument/spacing forms.

4. Analyzer/planner/runtime execution
   - [x] Extend the count aggregate select path for `COUNT(integer)` and
     `COUNT(NULL)` while preserving existing `COUNT(*)` and `COUNT(column)`.
   - [x] Accept only one aggregate select item, optional no-source/`DUAL`, or
     one descriptor-backed persistent base table with optional baseline
     `WHERE`.
   - [x] Resolve unqualified and schema-qualified table names against selected
     schema and MyLite catalog descriptors for table-backed forms.
   - [x] Reject reserved `_mylite_*` schema/table names before generated
     SQLite SQL.
   - [x] Reuse the existing descriptor-driven predicate planner for optional
     `WHERE`.
   - [x] Reject unsupported aggregate arguments, aliases, mixed projections,
     multiple aggregate items, `ORDER BY`, `LIMIT`, `GROUP BY`, `HAVING`,
     joins, subqueries, CTEs, and window clauses with deterministic
     diagnostics.
   - [x] Return one row with one decimal count value, warning count `0`, and
     affected rows `0` for supported forms.

5. Physical SQLite aggregate
   - [x] Generate SQLite `SELECT COUNT(?) FROM "physical_table"` with optional
     descriptor-built `WHERE`.
   - [x] Quote every generated SQLite identifier.
   - [x] Bind the count-literal parameter and predicate values through
     prepared statements.
   - [x] Avoid SQLite fork patches and custom SQLite functions.
   - [x] Keep descriptor rows, catalog generation, descriptor versions,
     descriptor caches, and SQLite schema generation unchanged.

6. Tests
   - [x] Extend the fast count aggregate C test under `packages/libmylite/tests/`.
   - [x] Cover supported no-source, `DUAL`, table-backed, predicate, empty,
     no-match, label, warning count, affected rows, following `ROW_COUNT()`,
     reopen persistence, physical failure, independent handle, preamble safety,
     and unsupported syntax behavior.
   - [x] Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - [x] Reuse the existing count aggregate test binary where possible.
   - [x] Keep first-party warning and clang-tidy policy enabled.
   - [x] Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run the count aggregate CTest entry and relevant parser/count-column
     lifecycle entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_count_literal_aggregate_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review the final diff for parser independence, descriptor authority,
     generated SQL safety, result semantics, row-count semantics,
     file-format safety, cleanup on failure, compatibility docs, and scope
     control.

## Out Of Scope

- Full aggregate support, general `COUNT(expr)`, `COUNT(DISTINCT)`, aliases,
  table-qualified aggregate arguments, string/decimal/float/hex/bit/boolean
  literal arguments, expression arguments, multiple aggregate items, mixed
  projections, grouping, having, order/limit aggregate semantics, window
  functions, joins, CTEs, subqueries, temporary tables, views, privileges,
  protocol metadata, optimizer behavior, SQL modes such as `IGNORE_SPACE`,
  arbitrary SQLite pass-through, and SQLite fork patches.
