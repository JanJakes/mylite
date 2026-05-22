# Baseline Replace Select Row-Scalar Source

## Status

This feature expands the current descriptor-driven `REPLACE ... SELECT` path
with the row-scalar source envelope already supported by `INSERT ... SELECT`:
`REPLACE [LOW_PRIORITY | DELAYED] [INTO] target [(columns)] SELECT ...` with
no `FROM`, or with `FROM DUAL` and an optional `WHERE [NOT] EXISTS (...)`
filter. The source produces zero or one row. When a row exists, execution
reuses the existing MyLite `REPLACE` row path so primary keys, unique indexes,
foreign keys, auto-increment, defaults, conversion, and affected rows remain
descriptor-owned.

This is not full MySQL `REPLACE ... SELECT`. It does not add compound sources,
joined sources, `TABLE`, `PARTITION`, general `DUAL WHERE` predicates, row
constructors, target aliases, generated-column non-`DEFAULT` writes, or general
expression conversion beyond the row-scalar projection subset already admitted
by MyLite.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `REPLACE ... SELECT`:
  `docs/specs/baseline-replace-select-lifecycle/specs.md`
- Baseline keyed `REPLACE ... SELECT`:
  `docs/specs/baseline-replace-select-keyed-targets/specs.md`
- Baseline `INSERT ... SELECT` row-scalar source:
  `docs/specs/baseline-insert-select-dual-source/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `REPLACE`:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_replace_select_row_scalar_source_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `REPLACE INTO t(cols) SELECT scalar_items` and
  `REPLACE INTO t(cols) SELECT scalar_items FROM DUAL` are accepted.
- Successful statements return no result set, update `ROW_COUNT()`, and leave
  `@@warning_count == 0` for supported in-range values.
- `FROM DUAL WHERE EXISTS (...)` inserts or replaces one row when the predicate
  is true and zero rows when it is false.
- `FROM DUAL WHERE NOT EXISTS (...)` inverts that row decision.
- A zero-row source succeeds with `ROW_COUNT() == 0` and does not check omitted
  `NOT NULL` no-default target columns.
- A one-row source checks omitted target columns, so omitted `NOT NULL`
  no-default columns fail with `1364 / HY000`.
- Selected `NULL` into a `NOT NULL` target fails with `1048 / 23000`.
- `SELECT * FROM DUAL` used as a replace source fails with
  `1096 / HY000`.
- On a target with no primary or unique key, repeated row-scalar replacements
  insert independent rows.
- On a primary-key target, a fresh row reports one affected row, a changed
  replacement reports deleted plus inserted rows, and an exact replacement
  reports one affected row in the verified row-scalar case.
- If one row conflicts with two existing rows through two different unique
  indexes, MySQL deletes both old rows, inserts the new row, and reports three
  affected rows.
- Unique-key tuples containing `NULL` do not conflict.
- Generated `AUTO_INCREMENT` row-scalar replacements use the same insert-side
  generation rules and update `LAST_INSERT_ID()` when a generated value is
  inserted.
- `LOW_PRIORITY` is accepted as a no-op, while `DELAYED` executes normally and
  records warning `3005`.
- Schema-qualified targets work without a selected default schema. Missing
  target schemas/tables are reported before row-scalar source planning.

Official MySQL documentation describes `REPLACE` as an insert with
delete-before-insert behavior for primary or unique-key conflicts, and states
that the affected-row count is inserted rows plus deleted rows. The MySQL
`SELECT` documentation allows selecting computed rows without a table and
allows `DUAL` as a dummy table name for tableless expressions.

## Scope

The implementation must add:

- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] target [(columns)]
  SELECT select_item_list`;
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] target [(columns)]
  SELECT select_item_list FROM DUAL [WHERE exists_predicate]`;
- row filtering for `FROM DUAL` only when the predicate is exactly the current
  `[NOT] EXISTS (select_statement)` subset, including parenthesized forms;
- row-scalar projection using the current MyLite planner for scalar literals,
  session values, scalar functions, and scalar expressions;
- target resolution, target column-list mapping, invisible-column behavior,
  omitted target defaults, conversion, duplicate-key replacement,
  auto-increment generation, foreign-key checks, and final physical mutation
  through the existing descriptor-owned `REPLACE` row path;
- key-bearing targets supported by the existing `REPLACE ... VALUES` / `SET`
  and table-backed `REPLACE ... SELECT` paths, including current primary-key,
  unique-index, prefix-key, nullable unique-key, multiple-conflict,
  auto-increment, and foreign-key behavior;
- exact zero-row, one-row, affected-row, warning-count, and `LAST_INSERT_ID()`
  behavior covered by MySQL 8.4.9 probes.

## Non-Goals

This feature must not implement:

- row-scalar `INSERT IGNORE ... SELECT` or any `REPLACE IGNORE` syntax;
- compound `UNION`, joined, table-backed multi-source, CTE, `TABLE`,
  parenthesized query-expression, or subquery source envelopes for
  `REPLACE ... SELECT`;
- source `ORDER BY`, `LIMIT`, locking clauses, grouping, `HAVING`, or
  wildcard projection for row-scalar sources;
- `FROM DUAL WHERE` predicates other than the limited `[NOT] EXISTS` subset;
- `PARTITION`, target aliases, target-qualified columns, row aliases,
  `RETURNING`, privileges, triggers, cascades beyond current descriptor-owned
  direct foreign-key checks/actions, binary-log safety warnings, or protocol
  metadata beyond current public result conventions;
- general expression conversion beyond what the current row-scalar projection
  and target descriptor conversion paths already support;
- SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` returns the existing non-row result
  object for successful replacement and uses existing error cleanup rules.
- Statement context: owns diagnostics, warning count, affected rows,
  `ROW_COUNT()`, transaction cleanup, and successful result finalization.
- Lexer/parser/AST: already admit the needed statement shape through the
  current `replace_select_statement` and row-scalar `select_statement`
  productions. This feature does not add grammar.
- Analyzer/planner: classifies no-source and `FROM DUAL` source statements as
  row-scalar sources for `REPLACE ... SELECT`, resolves the target first, and
  reuses the existing row-scalar source planner and target descriptor planner.
- Runtime: materializes at most one row from the row-scalar SQLite statement,
  validates it against target descriptors, then calls the existing one-row
  `REPLACE` executor by preserving `replace_existing_rows = true`.
- Catalog: remains authoritative for schemas, tables, columns, keys, foreign
  keys, defaults, and auto-increment counters. This feature mutates only normal
  row data plus auto-increment/table updated-time side effects already owned by
  current DML paths.
- SQLite: evaluates the generated row-scalar source statement and performs the
  descriptor-built physical row insert/delete statements using quoted
  identifiers and bound values. SQLite schema text is not metadata authority.
- Storage/VFS: unchanged. Row writes occur only in the shifted SQLite payload;
  the `.mylite` preamble is not touched.

## Supported Grammar

No new grammar is needed. The existing MyLite grammar already admits:

```sql
REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name
    [(column_name[, column_name] ...)]
    SELECT select_item[, select_item ...]

REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name
    [(column_name[, column_name] ...)]
    SELECT select_item[, select_item ...]
    FROM DUAL
    [WHERE exists_predicate]
```

`exists_predicate` is the current row-scalar insert source subset:

```sql
EXISTS (select_statement)
NOT EXISTS (select_statement)
(exists_predicate)
```

MyLite Lemon-syntax sketch for the already-admitted statement shape:

```lemon
replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}

replace_select_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S, M);
}
```

## Semantics

Target resolution, reserved `_mylite_*` rejection, `information_schema` write
rejection, unknown target schema/table diagnostics, target column-list mapping,
duplicate target column checks, generated-column target checks, and
read-only-target checks follow the existing `REPLACE ... SELECT` target path.

For no-source or `FROM DUAL` row-scalar sources:

- the source produces at most one row;
- a zero-row result reports `affected_rows == 0`, leaves row storage unchanged,
  and does not materialize omitted-column defaults or missing-default errors;
- a present row is validated and converted exactly as a one-row
  `REPLACE ... VALUES` row assembled from the selected values and omitted
  target defaults;
- selected `NULL` for an auto-increment target generates a value unless the
  current `NO_AUTO_VALUE_ON_ZERO` and explicit-zero rules say otherwise;
- duplicate primary or unique keys delete conflicting rows and retry the
  insert through the existing descriptor-owned replacement loop;
- exact replacement, changed replacement, multi-unique conflict replacement,
  nullable unique-key no-conflict, prefix unique-key replacement, and child or
  parent foreign-key checks follow the existing one-row `REPLACE` behavior;
- successful supported statements report `warning_count == 0` except for
  already-supported `DELAYED` deprecation warnings.

## SQLite Handling

For the row-scalar source, MyLite generates the same SQLite shape used by
row-scalar `INSERT ... SELECT`:

```sql
SELECT ?, ...
```

or, for an accepted `DUAL` `EXISTS` filter:

```sql
SELECT ?, ...
WHERE EXISTS (SELECT 1 FROM "physical_source" AS "_mylite_s1" WHERE ...)
```

Every source value and predicate value is bound as a parameter. `DUAL` is not
created as a SQLite table. When a source row exists, MyLite copies only that one
row into a `planned_insert_row` and calls the existing physical `REPLACE` row
executor. No result row set is materialized in C memory beyond that single row.

No SQLite fork hook is required.

## Diagnostics

Diagnostics must cover:

- syntax errors and unsupported optional clauses through the current parser;
- unsupported wildcard `DUAL` sources with MySQL-compatible `1096 / HY000`;
- target missing default schema, unknown schema, unknown table, reserved names,
  `information_schema` writes, unsupported object kind, read-only persistent
  writes, duplicate target columns, unknown target columns, and generated
  column writes through existing target planning;
- unsupported row-scalar source expressions and unsupported `FROM DUAL WHERE`
  predicates through current row-scalar planner diagnostics;
- target/source column-count mismatch with `1136 / 21S01`;
- omitted `NOT NULL` no-default target columns when a row exists with
  `1364 / HY000`;
- selected `NULL` into `NOT NULL` with `1048 / 23000`;
- target conversion/range/length/temporal/binary/`BIT`/`ENUM`/`SET`/JSON,
  duplicate-key, auto-increment, and foreign-key diagnostics through the
  existing replace row path;
- allocation, SQLite physical, and public API misuse failures through existing
  runtime policies.

The former `REPLACE ... SELECT does not support row-scalar sources` rejection
is removed for this supported subset. `REPLACE ... SELECT ... UNION ...`
remains outside this feature and is rejected deterministically at the current
parser or planner boundary for the attempted shape.

## Test Plan

- MySQL expectation script covering no-source, `FROM DUAL`, `WHERE EXISTS`,
  `WHERE NOT EXISTS`, zero-row sources, omitted defaults, omitted no-default
  errors, `NULL` into `NOT NULL`, `SELECT * FROM DUAL`, no-key repeated
  inserts, primary-key replacement, exact replacement, multiple unique
  conflicts, nullable unique keys, representative row-scalar function
  projection, auto-increment and `LAST_INSERT_ID()`, row-scalar
  `LOW_PRIORITY`/`DELAYED` modifiers, and schema-qualified targets.
- Runtime C tests in the existing `runtime_replace_select_lifecycle` binary for
  the same successful and diagnostic paths, plus persistence and preamble
  safety already covered by that lifecycle test.
- Regression checks that table-backed `REPLACE ... SELECT`, keyed replacement,
  `INSERT ... SELECT` row-scalar sources, parser tests, and unsupported
  `REPLACE ... SELECT ... UNION ...` behavior remain stable.
