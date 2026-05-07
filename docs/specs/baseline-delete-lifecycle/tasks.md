# Baseline Delete Lifecycle Tasks

## Goal

Add the next narrow DML slice: descriptor-driven single-table `DELETE` through
`mylite_execute()`, backed by authoritative MyLite catalog descriptors, stable
SQLite physical table names, and MyLite-owned integer/`NULL` predicate plus
limit conversion.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-delete-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, predicate/order column
     resolution, ordering and limit semantics, physical SQLite delete
     generation, diagnostics, result behavior, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset after implementation.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported deletes
     and intentionally rejected wider MySQL forms.
   - Verify affected rows, warnings, remaining rows, diagnostics, limit edge
     cases, ordering, `NULL` behavior, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `DELETE FROM table_name [WHERE ...] [ORDER BY
     ...] [LIMIT row_count]`.
   - Add an AST node kind for delete statements.
   - Map the `DELETE` keyword in the parser adapter.
   - Reuse the existing baseline predicate, order, and limit AST nodes where
     the delete subset has the same semantics.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected delete syntax.

4. Analyzer/planner/runtime execution
   - Add a statement execution path using the existing statement context.
   - Resolve unqualified and schema-qualified target tables against selected
     schema and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` target schema/table names before generated
     SQLite SQL.
   - Resolve predicate and ordering columns from descriptors, not SQLite
     metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     predicate columns, unknown order columns, unsupported predicate/order/
     limit shapes, unsupported literals, and out-of-range limit literals with
     deterministic diagnostics.
   - Return an empty DML result with exact affected-row count.

5. Physical SQLite deletion
   - Generate SQLite `DELETE` SQL only from descriptors and stable physical
     table names.
   - Quote every generated SQLite identifier.
   - Bind comparison predicate and limit row-count values through prepared
     statements.
   - Avoid relying on SQLite's optional `DELETE ... ORDER BY ... LIMIT` syntax.
   - Use internal rowid selection only for generated MyLite user rowid tables.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged for deletes.

6. Atomicity and cleanup
   - Execute supported deletes inside one MyLite-owned `BEGIN IMMEDIATE`
     transaction.
   - Plan and convert predicate/limit literals before writing the first row.
   - Roll back if prepare, bind, step, allocation, or SQLite execution fails.
   - Make new planner cleanup functions tolerate zero-initialized state.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful full-table, filtered, ordered, limited, and no-match
     deletes; all admitted integer families; `NULL` handling; predicate reuse;
     ordering and limit edge cases; affected rows; warnings; diagnostics;
     unsupported syntax; reopen persistence; rename/drop behavior; independent
     handles; preamble safety; and result API behavior.
   - Keep tests deterministic and avoid adding a new test framework.

8. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename/row-values/
     select-where/select-order-limit lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical deletion, integer conversion
     correctness, `NULL` ordering correctness, exact affected-row semantics,
     file-format safety, VFS preservation, zero-init safety, cleanup on
     failure, scope control, compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Full `DELETE`, aliases, partitions, modifiers, multi-table and joined
  deletes, `USING`, CTEs, subqueries, arbitrary SQLite pass-through,
  table-qualified/order expression/ordinal/multiple-key ordering, full
  `LIMIT/OFFSET`, triggers, cascades, foreign keys, privileges, temporary
  tables, views, indexes, constraints, auto-increment reset behavior, and
  SQLite fork patches.
