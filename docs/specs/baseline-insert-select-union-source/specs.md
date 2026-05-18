# Baseline Insert Select Union Source

## Status

This feature extends the current descriptor-driven `INSERT ... SELECT` baseline
with a narrow compound source path:

```sql
INSERT [INTO] target [(columns)]
SELECT ...
UNION [DISTINCT | ALL] SELECT ...
[UNION [DISTINCT | ALL] SELECT ...] ...
```

It builds on `mylite_execute()`, statement context, the parser AST, the current
top-level `UNION` support, descriptor-backed and row-scalar `SELECT` planning,
the existing `INSERT ... SELECT` target path, MyLite-owned catalog descriptors,
file-backed `.mylite` storage, and SQLite temporary-table materialization.

This is not full MySQL query-expression support inside DML. The slice admits
unparenthesized `UNION` / `UNION ALL` chains as an `INSERT ... SELECT` source
only. Branches are restricted to the currently supported descriptor-backed
single-table column/wildcard `SELECT` source subset or the currently supported
no-source / `FROM DUAL` row-scalar source subset. Broader set operations,
global compound ordering/limiting, parenthesized query expressions, joins,
`TABLE`, `VALUES`, CTEs, and `ON DUPLICATE KEY UPDATE` remain out of scope.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline insert select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline insert select dual source:
  `docs/specs/baseline-insert-select-dual-source/specs.md`
- Baseline insert select keyed targets:
  `docs/specs/baseline-insert-select-keyed-targets/specs.md`
- Baseline union select lifecycle:
  `docs/specs/baseline-union-select-lifecycle/specs.md`
- SQL table DML compatibility:
  `docs/compatibility/sql-table-dml.md`
- SQL query expression compatibility:
  `docs/compatibility/sql-query-expressions.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, set operations:
  https://dev.mysql.com/doc/refman/8.4/en/set-operations.html
- MySQL 8.4 Reference Manual, `UNION`:
  https://dev.mysql.com/doc/refman/8.4/en/union.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_select_union_source_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- A compound `SELECT` may be the source of `INSERT ... SELECT`.
- `UNION` and `UNION DISTINCT` remove duplicate source rows before insertion;
  `NULL` values compare equal for duplicate elimination.
- Under MySQL's default `utf8mb4_0900_ai_ci` collation, string values such as
  `'a'` and `'A'` are duplicate-equivalent for this slice; the first observed
  representative is inserted.
- `UNION ALL` preserves duplicate source rows.
- Mixed `UNION ALL` and distinct `UNION` chains follow the same accumulated
  left-side behavior as top-level `UNION`.
- A successful statement returns no result set, sets `ROW_COUNT()` to the
  number of inserted rows, and leaves `@@warning_count == 0` for supported
  in-range statements.
- A compound source producing zero rows succeeds with `ROW_COUNT() == 0` and
  does not check omitted target columns with no explicit default.
- Target/compound-source column-count mismatches fail with `1136 / 21S01`.
- Branch column-count mismatches fail with `1222 / 21000` before target
  assignment validation.
- `NULL` into a `NOT NULL` target fails with `1048 / 23000`.
- Omitted `NOT NULL` no-default target columns fail with `1364 / HY000` when
  the source produces at least one row.
- Duplicate-key failures roll the whole statement back.
- The target table may appear in a compound source branch; MySQL materializes
  the source before inserting into the target.
- MySQL also supports global compound `ORDER BY` / `LIMIT`, parenthesized
  branch `ORDER BY` / `LIMIT`, query-block locking clauses, `IGNORE`,
  partitions, `TABLE`, row constructors, expressions, joins, and
  `ON DUPLICATE KEY UPDATE`. They are out of scope for this slice unless
  already admitted by a narrower rule below.

## Scope

The implementation must add:

- parser and AST support allowing an existing `compound_select_statement` as
  the source child of `INSERT ... SELECT`;
- runtime source classification for plain table-backed, plain row-scalar, and
  compound `INSERT ... SELECT` sources;
- support for unparenthesized compound chains beginning with one `SELECT` and
  followed by one or more `UNION`, `UNION DISTINCT`, or `UNION ALL` terms;
- branch planning for the existing descriptor-backed single-table source
  subset used by table-backed `INSERT ... SELECT`, excluding joins;
- branch planning for the existing no-source and `FROM DUAL` row-scalar source
  subset used by row-scalar `INSERT ... SELECT`;
- target resolution, target column mapping, duplicate target-column rejection,
  invisible-column behavior, omitted target default behavior, duplicate-key
  checks, auto-increment handling, foreign-key child checks, and read-only
  persistent-write checks through the existing insert target path;
- target/source column-count validation before mutation;
- branch descriptor compatibility checks for descriptor-backed source columns;
- MyLite-owned validation of materialized source values against target
  descriptors before physical insertion;
- SQLite-side materialization of the compound source into an internal temporary
  table, followed by validation and insertion through the existing
  `INSERT ... SELECT` temporary-table path;
- all-or-nothing statement behavior for conversion, nullability, duplicate-key,
  auto-increment, and foreign-key failures;
- affected-row and warning-count behavior matching MySQL for supported
  successful statements;
- MySQL 8.4.9 expectation artifacts, focused C tests, and compatibility docs.

## Non-Goals

This feature must not implement:

- `REPLACE ... SELECT` compound sources;
- `INSERT IGNORE ... SELECT` compound sources;
- `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE` with a compound source;
- `PARTITION`, `TABLE`, standalone `VALUES`, CTEs, derived tables,
  parenthesized query expressions, `INTERSECT`, `EXCEPT`, or recursive query
  expressions;
- global compound `ORDER BY` / `LIMIT`;
- parenthesized branch-local `ORDER BY` / `LIMIT`;
- branch locking clauses;
- `SQL_CALC_FOUND_ROWS` or other branch select options in compound insert
  sources;
- joined branch sources, grouped/aggregate branch sources, table-backed
  expression branch projections, or wildcard `DUAL`;
- target aliases, source aliases beyond the existing single-table source alias
  policy, table-qualified target columns, duplicate target tables, or
  user-visible temporary tables;
- broad type aggregation across branches, collation aggregation,
  protocol-grade metadata merging, or MySQL optimizer parity;
- selected-row warning demotion, non-strict conversion, triggers, generated
  columns, recursive cascades beyond the current direct FK subset, privileges,
  or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()` state, and successful non-row result finalization.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code owns source classification, target resolution before
  source resolution, target column mapping, branch planning, branch
  column-count validation, descriptor compatibility checks, and deterministic
  unsupported-shape diagnostics.
- The insert runtime owns source materialization, value validation, physical
  row insertion, duplicate-key handling, auto-increment counters, foreign-key
  checks, affected-row reporting, and rollback on failure.
- Catalog descriptors remain authoritative for target and descriptor-backed
  source metadata. This feature must not read SQLite schema text as metadata
  authority and must not mutate catalog rows except for normal auto-increment
  counter advancement caused by successful generated inserts.
- SQLite owns physical row storage, scans, generated filter execution,
  internal temporary storage, and final row insertion. MyLite owns the
  compound duplicate-elimination semantics admitted by this slice. MyLite
  builds all SQLite SQL from descriptors and stable physical names, quotes
  generated identifiers, and binds predicate, scalar, limit, and insert values.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only through ordinary physical row storage inside the
  shifted SQLite payload.

## Supported SQL Grammar

Supported subset:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED]
    [INTO] table_name [(column_name[, column_name] ...)]
    select_union_source

select_union_source:
    select_statement UNION [DISTINCT | ALL] select_statement
    [UNION [DISTINCT | ALL] select_statement] ...
```

`LOW_PRIORITY` and `HIGH_PRIORITY` remain accepted no-ops. `DELAYED` remains
accepted only as the existing immediate insert with MySQL's deprecated-delayed
warning. `IGNORE` is rejected for compound sources in this slice.

The `select_statement` branches are limited as described in this spec. The
plain non-compound `INSERT ... SELECT` source grammar and behavior remain
unchanged.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL);
}
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) table_name(T) insert_column_list_opt(C)
    insert_select_source_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL);
}

insert_select_source_statement(A) ::= select_statement(S). {
    A = S;
}
insert_select_source_statement(A) ::= compound_select_statement(S). {
    A = S;
}
```

The existing `IGNORE` alternatives may parse a compound source, but runtime
planning must reject them with the documented unsupported diagnostic.

## Semantics

### Resolution And Diagnostics Ordering

Target resolution follows the existing insert target path:

- unqualified targets require a selected default schema;
- schema-qualified targets resolve without a selected schema;
- unknown target schemas fail with `1049 / 42000`;
- unknown target tables fail with `1146 / 42S02`;
- synthetic `information_schema` write targets fail with `1044 / 42000`;
- reserved `_mylite_*` target schemas or table names are rejected before any
  generated SQLite SQL;
- only persistent or shadowing session temporary base-table targets are
  supported.

The target descriptor and target column list are resolved before compound
source planning. Source branch resolution then follows the existing
descriptor-backed and row-scalar `SELECT` policies for each admitted branch.

### Branch Shape

Each branch must be a supported `SELECT` query block with the same column
count as the first branch. MyLite rejects branch `ORDER BY`, `LIMIT`,
locking clauses, `SQL_CALC_FOUND_ROWS`, and select options using deterministic
unsupported diagnostics, matching the existing top-level `UNION` boundary.

Descriptor-backed branches use the existing single-table source subset from
`INSERT ... SELECT`: descriptor column lists or `*`, optional source alias,
optional `WHERE`, and no joins. `SELECT *` expands visible source columns.
Explicit descriptor columns may name invisible columns.

Row-scalar branches use the existing no-source or `FROM DUAL` scalar source
subset from `INSERT ... SELECT`, including the current `FROM DUAL WHERE
[NOT] EXISTS (...)` filter. They produce ordinary SQLite scalar values that
are validated against the target descriptors.

### Duplicate Handling And Row Order

MyLite materializes all compound source branches into SQLite temporary storage
with `UNION ALL`, an internal branch marker, descriptor-built projection SQL,
and bound parameters. For distinct `UNION` terms, MyLite then scans that
temporary table in insertion order and filters duplicate rows with generated
SQL before validation and insertion. This avoids relying on SQLite's choice of
which duplicate representative survives.

Duplicate comparison for the supported subset treats `NULL` values as equal.
For text-family branch outputs, MyLite applies the registered
`utf8mb4_0900_ai_ci` collation when comparing duplicates, so case-only
duplicates collapse while the first observed representative is retained.
`UNION ALL` chains skip the duplicate filter and preserve every materialized
row. Mixed chains apply distinct terms to the accumulated prefix and preserve
later `UNION ALL` terms.

Without a global `ORDER BY`, MySQL does not promise a broad row order. Tests
must not depend on unspecified row order except for values whose final order is
made deterministic by a separate verification `SELECT ... ORDER BY`.

### Target Mapping And Value Validation

The target column list is descriptor-driven and follows existing insert rules:

- an omitted target list maps to visible target columns in descriptor order;
- an explicit target list may name invisible columns;
- duplicate target columns fail before mutation;
- omitted target columns are filled from descriptor defaults or effective
  nullable `NULL` only when at least one source row is inserted;
- a zero-row compound source succeeds without validating omitted no-default
  target columns.

For descriptor-backed branch projections, source descriptors must be compatible
with their mapped target descriptors according to the current `INSERT ...
SELECT` compatibility policy. For row-scalar branch projections, runtime
source values are validated directly against target descriptors.

Materialized source values are checked before insertion for supported
nullability, range, string, binary, temporal, `YEAR`, `BIT`, decimal,
approximate, JSON, `ENUM`, and `SET` behavior already owned by the current
insert path. `NULL` into `NOT NULL` fails with the existing MySQL-compatible
diagnostic. Out-of-range or unsupported conversion failures abort the whole
statement before target mutation.

### Physical SQLite Handling

MyLite must not call the public top-level compound result builder and then
reparse display text for insertion. Instead it materializes the compound source
inside SQLite temporary storage:

```sql
CREATE TEMP TABLE "temp_name" AS
SELECT 0 AS "_mylite_union_branch", ... FROM "source_physical"
UNION ALL
SELECT 1, ...
```

No source SQL may use catalog names supplied by the user directly. Generated
SQL must use descriptor-owned physical names, quoted identifiers, and unique
numbered parameters across all compound branches. MyLite binds all branch
predicate, row-scalar, `EXISTS`, and limit values through prepared statements.

After materialization, MyLite validates rows from the temporary table through a
generated validation scan. If any distinct `UNION` term is present, that scan
uses the internal branch marker and SQLite rowid insertion order to keep the
first duplicate representative in the distinct prefix; later `UNION ALL`
branches remain visible. MyLite then uses the existing descriptor-owned
physical insert path. The temporary table is dropped on success and best-effort
dropped on failure. A statement transaction wraps materialization, validation,
insertion, auto-increment counter updates, foreign-key checks, and
temporary-table cleanup so failures leave no partial target rows.

This keeps the feature close to the current `INSERT ... SELECT` performance
model: the selected row set is materialized once in SQLite temporary storage,
not copied into a C-side result buffer. It is still a baseline path, not a full
set-operation optimizer.

## Result Behavior

Successful supported statements:

- return through the existing non-row statement result convention;
- return no result columns and no result rows;
- set `affected_rows` and `ROW_COUNT()` to the number of rows actually
  inserted;
- leave `warning_count == 0`, except for the existing `DELAYED` deprecation
  warning when that modifier is used;
- preserve `LAST_INSERT_ID()` behavior from the existing insert path, including
  the first generated auto-increment value from successful generated inserts.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors, including compound sources after `REPLACE ... SELECT` and
  compound insert sources followed by `ON DUPLICATE KEY UPDATE`;
- unsupported `INSERT IGNORE ... SELECT` compound source;
- unsupported branch `ORDER BY`, `LIMIT`, locking clauses, select options, or
  `SQL_CALC_FOUND_ROWS`;
- unsupported joined, grouped, aggregate, parenthesized, `TABLE`, `VALUES`,
  `INTERSECT`, `EXCEPT`, CTE, or derived-table sources;
- missing default schema;
- unknown schema;
- unknown target or source table;
- synthetic `information_schema` write targets;
- reserved `_mylite_*` schema/table names;
- unsupported object kinds;
- duplicate or unknown target columns;
- unknown source columns;
- branch column-count mismatch;
- target/source column-count mismatch;
- unsupported descriptor compatibility or value conversion;
- integer, decimal, temporal, `YEAR`, string, binary, `BIT`, JSON, `ENUM`, or
  `SET` range/format failures inherited from the current insert path;
- `NULL` into `NOT NULL`;
- duplicate primary-key or unique-key rows;
- missing foreign-key parents;
- physical SQLite failures;
- allocation failures;
- public API misuse through existing public surface behavior.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name if a new binary is clearer than extending existing insert
tests. Tests must cover:

- scalar compound source with `UNION`, `UNION DISTINCT`, `UNION ALL`, and a
  mixed chain;
- descriptor table compound source, including `SELECT *` visible-column
  expansion;
- schema-qualified target and source branches without a selected default
  schema;
- zero-row compound source and omitted no-default target behavior;
- target/source column-count mismatch and branch column-count mismatch;
- `NULL` into nullable and `NOT NULL` targets;
- supported integer and string-compatible descriptor values through current
  insert conversion;
- duplicate-key rollback;
- auto-increment generation for compound source rows;
- same-table source/target materialization;
- rejected `IGNORE`, `ON DUPLICATE KEY UPDATE`, branch `ORDER BY`, branch
  `LIMIT`, joins, and `REPLACE` compound sources, with existing parser/runtime
  boundaries covering locking clauses, global compound `ORDER BY` / `LIMIT`,
  parenthesized branch expressions, `TABLE`, `VALUES`, and CTEs;
- affected rows, warning count, absence of result rows, persistence after
  close/reopen, independent file-backed handles, and `.mylite` preamble safety;
- zero-initialized cleanup for new planner/runtime objects;
- existing parser, runtime insert-select, union, key-target, file-format,
  storage, statement-context, and lifecycle tests.
