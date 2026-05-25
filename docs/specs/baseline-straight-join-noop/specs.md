# Baseline STRAIGHT_JOIN No-Op

## Goal

Admit MySQL's `STRAIGHT_JOIN` table-reference operator in the join envelopes
MyLite already executes, while preserving descriptor-driven inner-join
semantics. MyLite does not currently expose a cost-based join-order optimizer,
so this slice treats `STRAIGHT_JOIN` as an accepted optimizer-order hint with no
additional physical-planning effect.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing join specs under `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Official MySQL 8.4 Reference Manual, `JOIN` clause:
  <https://dev.mysql.com/doc/refman/8.4/en/join.html>
- Official MySQL 8.4 Reference Manual, `DELETE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/delete.html>
- Official MySQL 8.4 Reference Manual, `UPDATE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_straight_join_noop_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish the behavior used by this baseline:

- `STRAIGHT_JOIN` between two table references behaves as an inner join for
  result rows.
- `STRAIGHT_JOIN ... ON left = right` accepts the same equality join predicate
  shape as ordinary `JOIN`.
- `STRAIGHT_JOIN` without `ON` is accepted and produces the same Cartesian
  product shape as an ordinary inner join without a join condition.
- `SELECT *` expands visible columns from the syntactic left source followed by
  visible columns from the syntactic right source.
- Chained `STRAIGHT_JOIN` table references are accepted left to right.
- String join predicates use the table columns' collation. With MySQL's default
  `utf8mb4_0900_ai_ci`, `Beta` matches `beta`.
- The statement-level `SELECT STRAIGHT_JOIN ...` modifier can appear with a
  table-reference `STRAIGHT_JOIN`; successful supported selects return normal
  rows, `@@warning_count = 0`, and `ROW_COUNT() = -1`.
- Joined `DELETE` and joined `UPDATE` accept `STRAIGHT_JOIN` where ordinary
  inner `JOIN` is accepted and report normal affected-row and warning counts.
- MySQL accepts `STRAIGHT_JOIN ... USING (...)`, but `USING` changes output
  column coalescing and remains deferred in this slice.
- `NATURAL STRAIGHT_JOIN` and `LEFT STRAIGHT_JOIN` are syntax errors in MySQL
  8.4.9 and remain syntax errors in MyLite.

## Supported Surface

MyLite supports `STRAIGHT_JOIN` in the current descriptor-backed join envelopes:

- plain row-returning `SELECT` over the existing multi-source inner/cartesian
  source chain;
- joined `DELETE` with one target and two sources;
- joined `UPDATE` with one target and two sources;
- optional `ON left_column = right_column` using the same descriptor-column
  resolution and same-family integer or ASCII nonbinary string comparison
  subset as existing inner joins;
- omitted `ON`, producing the same Cartesian product semantics as existing
  inner joins;
- persistent and shadowing session temporary base-table descriptors;
- unqualified and schema-qualified source names, optional aliases, validated
  no-op index hints, projection, `WHERE`, `ORDER BY`, `LIMIT`,
  `SQL_CALC_FOUND_ROWS`, and DML assignment/target rules exactly where the
  existing join paths already support them.

`STRAIGHT_JOIN` maps to `MYLITE_SQL_AST_JOIN_KIND_INNER`. This keeps all
runtime name resolution, descriptor authority, physical SQL generation,
warning handling, and affected-row behavior on the existing inner-join path.

## Deferred Surface

This slice intentionally does not support:

- real optimizer join-order forcing or plan-shape guarantees;
- `USING`, natural joins, parenthesized table references, ODBC escape joins,
  lateral or derived tables, table functions, CTEs, and partitions;
- multi-source outer chains or broader joined DML than the existing joined
  update/delete envelopes;
- mixed comma/explicit precedence beyond existing limitations;
- join predicates other than the current descriptor equality subset;
- arbitrary expression projection, expression ordering, expression predicates,
  full grouping, `DISTINCT` join rows, locking changes, or arbitrary SQLite
  pass-through.

## Grammar

MyLite admits `STRAIGHT_JOIN` as an inner join operator in the existing join
operator nonterminals:

```lemon
inner_join_operator(A) ::= STRAIGHT_JOIN.
join_operator(A) ::= STRAIGHT_JOIN.
```

Both productions assign `MYLITE_SQL_AST_JOIN_KIND_INNER`.

`STRAIGHT_JOIN` as a statement-level select modifier is already supported by
the existing select-modifier grammar and remains separate from this
table-reference operator.

## Architecture

- Public API: no ABI or public result API changes. Successful statements return
  through existing query or non-query result conventions.
- Statement context: unchanged. `STRAIGHT_JOIN` does not mutate statement
  timestamps, warning state outside existing select/DML behavior, or
  transaction state.
- Lexer/parser/AST: the existing reserved `STRAIGHT_JOIN` token is reused. The
  parser maps it to the existing inner-join AST kind instead of adding a new
  runtime-facing join kind.
- Analyzer/planner: unchanged. Table, alias, predicate, projection, ordering,
  DML target, and assignment resolution continue to use MyLite descriptors.
- Catalog: unchanged and authoritative. `STRAIGHT_JOIN` does not read SQLite
  schema text, mutate descriptors, or alter catalog generations.
- Runtime SQL generation: unchanged. Since the AST kind is inner, generated
  SQLite uses the same quoted physical table names, stable aliases, and
  parameter binding as ordinary inner joins.
- Storage/VFS/SQLite: no file-format, VFS, or SQLite fork changes. The feature
  is a MyLite parser/translation compatibility addition.

## Diagnostics

Existing diagnostics are preserved:

- syntax errors for malformed `STRAIGHT_JOIN` forms such as unsupported
  `USING` in MyLite, `NATURAL STRAIGHT_JOIN`, or `LEFT STRAIGHT_JOIN`;
- missing default schema, unknown schema/table, duplicate aliases, ambiguous
  columns, unknown predicate/projection/order columns, unsupported object kinds,
  unsupported join predicates, unsupported joined DML shapes, assignment
  diagnostics, physical SQLite failures, allocation failures, and public API
  misuse all reuse the existing join-path behavior.

## Tests

Coverage includes:

- parser acceptance for plain `SELECT`, chained `SELECT`, joined `DELETE`, and
  joined `UPDATE` with `STRAIGHT_JOIN`;
- parser rejection for unsupported `STRAIGHT_JOIN ... USING (...)`;
- runtime `SELECT` equality joins, Cartesian joins without `ON`, wildcard
  projection order, string-collation joins, chained joins, statement-level
  `STRAIGHT_JOIN` together with table-reference `STRAIGHT_JOIN`, persistence,
  temporary shadowing, and file preamble safety through the existing join test;
- runtime joined `DELETE` and joined `UPDATE` smoke coverage proving the
  operator reuses the existing inner-join DML path;
- MySQL 8.4.9 expectation script for every supported user-visible behavior.
