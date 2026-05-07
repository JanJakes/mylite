# Baseline COUNT Aggregate Tasks

## Goal

Add the next narrow aggregate slice: descriptor-driven `SELECT COUNT(*)`
through `mylite_execute()`, backed by authoritative MyLite catalog descriptors,
stable SQLite physical table names, and the existing baseline predicate
conversion path.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-count-aggregate/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, no-space function-call behavior, schema/table
     resolution, predicate reuse, generated SQLite SQL, diagnostics, result
     behavior, row-count behavior, and unsupported aggregate forms.
   - Update compatibility docs only for the exact partial subset after
     implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `COUNT(*)` behavior and intentionally rejected/deferred wider forms.
   - Verify empty/nonempty counts, `NULL` row handling, no-source and `DUAL`
     forms, predicate counts, labels, warnings, following `ROW_COUNT()`, and
     diagnostics for syntax-rejected count forms.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Add `COUNT` keyword recognition without making it reserved.
   - Add an AST node for `COUNT(*)`.
   - Extend Lemon grammar for no-space `COUNT(*)`, including whitespace or
     comments inside the argument list.
   - Preserve source spans for default result labels.
   - Preserve `COUNT` as an identifier where the grammar admits nonreserved
     function-name identifiers.
   - Add parser tests for supported count syntax and unsupported count
     argument/spacing forms.

4. Analyzer/planner/runtime execution
   - Add a count aggregate select path distinct from scalar session selects and
     regular table projection selects.
   - Accept only one `COUNT(*)` select item, with optional `FROM DUAL` or one
     descriptor-backed persistent base table.
   - Resolve unqualified and schema-qualified table names against selected
     schema and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` schema/table names before generated SQLite SQL.
   - Reuse the existing descriptor-driven predicate planner for optional
     `WHERE`.
   - Reject unsupported aggregate arguments, aliases, mixed projections,
     multiple count items, `ORDER BY`, `LIMIT`, `GROUP BY`, `HAVING`, joins,
     subqueries, CTEs, and window clauses with deterministic diagnostics.
   - Return one row with one decimal count value, warning count `0`, and
     affected rows `0` for supported forms.

5. Physical SQLite count
   - Generate SQLite `SELECT COUNT(*) FROM "physical_table" [WHERE ...]` only
     from descriptors and stable physical table names.
   - Quote every generated SQLite identifier.
   - Bind predicate values through prepared statements.
   - Avoid SQLite fork patches and custom SQLite functions.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover no-source, `DUAL`, empty/nonempty tables, nullable rows,
     schema-qualified/unqualified resolution, predicate reuse, integer-family
     predicate boundaries, labels, warning count, affected rows, following
     `ROW_COUNT()`, reopen persistence, rename/truncate/drop behavior,
     independent handles, preamble safety, and unsupported syntax.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and relevant parser/select lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for parser independence, descriptor authority,
     generated SQL safety, result semantics, row-count semantics,
     file-format safety, cleanup on failure, compatibility docs, and scope
     control.

## Out Of Scope

- Full aggregate support, `COUNT(expr)`, `COUNT(DISTINCT)`, other aggregate
  functions, aliases, multiple aggregate items, mixed projections, grouping,
  having, order/limit aggregate semantics, window functions, joins, CTEs,
  subqueries, temporary tables, views, privileges, protocol metadata, optimizer
  behavior, SQL modes such as `IGNORE_SPACE`, arbitrary SQLite pass-through,
  and SQLite fork patches.
