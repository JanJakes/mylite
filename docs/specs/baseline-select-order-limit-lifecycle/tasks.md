# Baseline Select Order Limit Lifecycle Tasks

## Goal

Add the next narrow query slice: descriptor-driven single-table sorted and
limited `SELECT` through `mylite_execute()`, backed by authoritative MyLite
catalog descriptors, stable SQLite physical table names, and MyLite-owned
integer/`NULL` predicate plus limit/offset conversion.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-select-order-limit-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, projection/predicate/order
     column resolution, ordering semantics, limit/offset semantics, physical
     SQLite query generation, diagnostics, result behavior, read ordering, and
     unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported ordered
     and limited selects and intentionally rejected wider MySQL forms.
   - Verify result rows, column labels, `NULL` values, diagnostics, warnings,
     affected rows where applicable, limit/offset edge cases, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for optional `ORDER BY` and `LIMIT` clauses on
     descriptor-backed table `SELECT`.
   - Add AST node kinds for order and limit clauses.
   - Map `ORDER`, `BY`, `ASC`, `DESC`, `LIMIT`, and `OFFSET` keywords in the
     parser adapter.
   - Admit one order identifier, optional order direction, unsigned decimal
     limit literals, and the three supported MySQL limit spellings.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected ordered/limited select syntax.

4. Analyzer/planner/runtime execution
   - Extend the existing table select execution path, using the existing
     statement context.
   - Resolve unqualified and schema-qualified source tables against selected
     schema and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` source schema/table names before generated
     SQLite SQL.
   - Resolve projection, predicate, and ordering columns from descriptors, not
     SQLite metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     projection columns, unknown predicate columns, unknown order columns,
     unsupported order shapes, unsupported limit shapes, unsupported literals,
     and out-of-range limit/offset literals with deterministic diagnostics.
   - Return descriptor-driven text results for sorted and limited selects.

5. Physical SQLite sorting and limiting
   - Generate SQLite query SQL only from descriptors and stable physical table
     names.
   - Quote every generated SQLite identifier.
   - Bind comparison predicate, limit row count, and offset values through
     prepared statements.
   - Preserve SQLite native integer/`NULL` ordering only for the verified
     descriptor-limited subset.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged for sorted/limited reads.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful ordered wildcard and explicit projections, default/ASC/
     DESC directions, nullable order columns, `NULL` ordering, duplicate ties,
     duplicate projected columns, limit forms, limit zero, offset behavior,
     integer boundaries, schema resolution, diagnostics, unsupported syntax,
     reopen persistence, rename/drop behavior, independent handles, preamble
     safety, and result API behavior.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename/row-values/
     select-where lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical sorting/limiting, integer conversion
     correctness, `NULL` ordering correctness, file-format safety, VFS
     preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Full `ORDER BY`, expression ordering, aliases, ordinal ordering,
  table-qualified order columns, multiple sort keys, collations, `RAND()`,
  full `LIMIT/OFFSET`, signed limit literals, parameters or variables in limit
  clauses, sorting/limit over joins or query expressions, grouping, subqueries,
  CTEs, set operations, aggregate/window functions, locks, and arbitrary SQLite
  pass-through.
- `UPDATE`, `DELETE`, `REPLACE`, `INSERT ... SELECT`, default expressions,
  auto-increment, generated IDs, constraints, indexes, triggers, temporary
  tables, views, privileges, full type conversion, warning demotion, unsigned
  64-bit storage beyond the signed-64-bit physical encoding, and SQLite fork
  patches.
