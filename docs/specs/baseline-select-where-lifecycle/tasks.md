# Baseline Select Where Lifecycle Tasks

## Goal

Add the next narrow query slice: descriptor-driven single-table filtered
`SELECT` through `mylite_execute()`, backed by authoritative MyLite catalog
descriptors, stable SQLite physical table names, and MyLite-owned integer/NULL
predicate conversion.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-select-where-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, projection and predicate column
     resolution, predicate semantics, physical SQLite query generation,
     diagnostics, result behavior, read ordering, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported filtered
     selects and intentionally rejected wider MySQL forms.
   - Verify result rows, column labels, `NULL` values, diagnostics, warnings,
     affected rows where applicable, and side effects.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `WHERE` on descriptor-backed table `SELECT`.
   - Add AST node kinds for where clauses and predicate nodes.
   - Map `WHERE`, `IS`, and comparison operators in the parser adapter.
   - Admit one column predicate, optional predicate parentheses, signed integer
     predicate literals, comparison operators, `IS NULL`, and `IS NOT NULL`.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected filtered-select syntax.

4. Analyzer/planner/runtime execution
   - Extend the existing table select execution path, using the existing
     statement context.
   - Resolve unqualified and schema-qualified source tables against selected
     schema and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` source schema/table names before generated
     SQLite SQL.
   - Resolve projection and predicate columns from descriptors, not SQLite
     metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     projection columns, unknown predicate columns, unsupported predicate
     shapes, unsupported literals, unsupported operators, unsupported clauses,
     and out-of-range predicate literals with deterministic diagnostics.
   - Return descriptor-driven text results for filtered selects.

5. Physical SQLite filtering
   - Generate SQLite query SQL only from descriptors and stable physical table
     names.
   - Quote every generated SQLite identifier.
   - Bind comparison predicate values through prepared statements.
   - Lower `IS NULL` and `IS NOT NULL` without bound values.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged for filtered reads.

6. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful filtered wildcard and explicit projections, all admitted
     operators, `IS NULL`, `IS NOT NULL`, duplicate projected columns, integer
     boundaries, schema resolution, diagnostics, unsupported syntax, reopen
     persistence, rename/drop behavior, independent handles, preamble safety,
     and result API behavior.
   - Keep tests deterministic and avoid adding a new test framework.

7. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename/row-values
     lifecycle entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical filtering, comparison conversion,
     file-format safety, VFS preservation, zero-init safety, cleanup on
     failure, scope control, compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Full `WHERE`, general expression evaluation, aliases, table-qualified
  predicate columns, literal-on-left comparisons, `col = NULL`, `col <> NULL`,
  sorting, limiting, joins, grouping, subqueries, CTEs, set operations,
  aggregate/window functions, locks, and arbitrary SQLite pass-through.
- `UPDATE`, `DELETE`, `REPLACE`, `INSERT ... SELECT`, default expressions,
  auto-increment, generated IDs, constraints, indexes, triggers, temporary
  tables, views, privileges, full type conversion, warning demotion, unsigned
  64-bit storage beyond the signed-64-bit physical encoding, and SQLite fork
  patches.
