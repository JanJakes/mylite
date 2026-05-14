# Baseline Empty INSERT Values

## Summary

This phase adds the narrow MySQL all-default row syntax for the existing
descriptor-backed `INSERT ... VALUES` path:

```sql
INSERT INTO table_name () VALUES ();
INSERT INTO table_name VALUES ();
```

It also applies the same row-shape handling to the existing compatible
`INSERT IGNORE`, limited `INSERT ... ON DUPLICATE KEY UPDATE`, and no-key
`REPLACE ... VALUES` paths because those statements already share MyLite's
descriptor-driven insert planner.

The feature does not add general expression values, `DEFAULT(col_name)`, row
constructors, the `VALUE` synonym, arbitrary SQLite pass-through, or a wider
default-conversion matrix. It only lets an empty target row mean "omit every
target column and materialize descriptor defaults" in the already supported
insert-equivalent paths.

## Sources And Evidence

Compatibility authority:

- MySQL 8.4 Reference Manual, `INSERT`:
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
- MySQL 8.4 Reference Manual, data type default values:
  <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
- MySQL 8.4 Reference Manual, `REPLACE`:
  <https://dev.mysql.com/doc/refman/8.4/en/replace.html>
- MySQL 8.4 Reference Manual, `INSERT ... ON DUPLICATE KEY UPDATE`:
  <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
- Runtime probes against MySQL 8.4.9, captured by
  `packages/libmylite/tests/mysql_baseline_empty_insert_values_expectations.sh`.

The MySQL `INSERT` documentation states that when both the column list and
`VALUES` list are empty, the inserted row receives column defaults. Runtime
probes against MySQL 8.4.9 confirm these details for this slice:

- `INSERT INTO t () VALUES ()` and `INSERT INTO t VALUES ()` both insert one
  all-default row.
- `INSERT INTO t () VALUES (), ()` inserts two all-default rows.
- `INSERT ... VALUE ()` remains outside MyLite's current grammar even though
  MySQL accepts `VALUE` as a synonym for `VALUES`.
- `INSERT ... VALUES ROW()` also creates an all-default row in MySQL 8.4.9,
  but row-constructor syntax remains deferred in MyLite.
- Mixed empty and nonempty value rows fail with `ERROR 1136 (21S01)` at the
  first row whose value count does not match the statement target shape.
- `INSERT INTO t () SELECT 1` fails with `ERROR 1136 (21S01)`.
- Strict all-default insertion fails with `ERROR 1364 (HY000)` when a
  non-auto-increment `NOT NULL` column has no explicit default.
- `INSERT IGNORE INTO t () VALUES ()` demotes omitted no-default errors into
  warning `1364` and stores the same implicit values already used by MyLite's
  existing `INSERT IGNORE` omitted-column policy.
- Empty-row inserts generate auto-increment values for omitted indexed
  auto-increment columns and update `LAST_INSERT_ID()` like other generated
  insert rows.
- No-key `REPLACE INTO t () VALUES ()` and `REPLACE INTO t VALUES ()` behave as
  insert-equivalent all-default rows.
- Existing duplicate-key update semantics apply after the all-default row is
  planned: duplicate rows report affected rows `2`, no-op duplicate updates
  report `0`, and deprecated `VALUES(column)` still records warning `1287`.

## Supported Surface

Supported statements:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [IGNORE]
    [INTO] table_name [()] VALUES empty_insert_row[, ...]
    [ON DUPLICATE KEY UPDATE duplicate_assignment]

REPLACE [LOW_PRIORITY | DELAYED]
    [INTO] table_name [()] VALUES empty_insert_row[, ...]
```

`empty_insert_row` is exactly `()`.

This feature supports persistent base tables and shadowing session temporary
base tables in the same places where the surrounding insert path already
supports them. Schema-qualified and unqualified table resolution, selected
schema behavior, reserved `_mylite_*` rejection, unknown object diagnostics,
foreign-key checks, duplicate-key checks, delayed warnings, priority no-ops,
and public result conventions are inherited from the existing insert,
insert-ignore, duplicate-key, and replace features.

The target descriptor families are not expanded by this phase. All-default rows
materialize only descriptor defaults already supported by the current row-value
and default-value code: nullable effective `NULL`, explicit numeric/string/
binary/`BIT`/`YEAR`/temporal/JSON/enum/set defaults, current timestamp defaults,
integer expression defaults, and generated values for the current indexed
auto-increment subset.

## Non-Goals

This feature does not add:

- `INSERT ... VALUE ()`;
- `VALUES ROW()`, table value constructors, or `ROW()`;
- `INSERT ... DEFAULT VALUES` syntax;
- `DEFAULT(col_name)`;
- expressions in value rows;
- non-strict mode outside existing `INSERT IGNORE` warning demotion;
- `INSERT IGNORE ... SELECT`;
- warning demotion beyond existing `INSERT IGNORE` adjusted-value rules;
- key-bearing `REPLACE` delete-insert semantics;
- `REPLACE ... ON DUPLICATE KEY UPDATE`;
- new auto-increment behavior for temporary tables;
- generated columns, triggers, cascades, privileges, replication metadata, or
  changed-column protocol metadata;
- SQLite default-expression execution or a SQLite fork patch.

## Ownership Boundaries

- Public API: no ABI or public header changes. Successful statements return
  existing non-row DML result objects with exact affected rows and warning
  counts.
- Statement context: diagnostics, warnings, affected rows, transaction cleanup,
  and result ownership remain in the existing execution flow.
- Parser/AST: admits empty explicit insert column lists and empty insert rows
  while preserving the distinction between an omitted column list and an
  explicit `()`.
- Analyzer/planner: maps empty rows to zero explicit target columns only when
  MySQL's all-default row shape applies. It continues to resolve every table
  and explicit column through MyLite descriptors.
- Catalog: remains authoritative for logical defaults, nullability,
  auto-increment, keys, and foreign keys. This DML feature does not mutate
  catalog rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builders: expose existing DML result shape; no row result set is
  returned for successful inserts or replaces.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are preserved.
- SQLite physical storage: MyLite continues to generate descriptor-derived
  SQLite `INSERT` statements over stable physical table names and bind all
  planned values. SQLite `DEFAULT VALUES` syntax is not the compatibility
  authority for this feature.

## Grammar

MySQL-facing syntax admitted by this feature:

```text
insert_values_statement:
    INSERT insert_modifiers into_opt table_name insert_column_list_opt
        VALUES insert_row_list on_duplicate_opt

insert_column_list_opt:
    omitted
  | ( )
  | ( column_name [, column_name]... )

insert_row_list:
    insert_row [, insert_row]...

insert_row:
    ( )
  | ( insert_value [, insert_value]... )
```

MyLite Lemon-syntax snippets:

```lemon
insert_column_list_opt(A) ::= . {
    A = mylite_sql_parser_make_identifier_list(state, NULL);
}
insert_column_list_opt(A) ::= LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_empty_identifier_list(state, L, R);
}
insert_column_list_opt(A) ::= LPAREN identifier_list(L) RPAREN. {
    A = L;
}

insert_row(A) ::= LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(
        state,
        L,
        mylite_sql_parser_make_insert_row_values(state, NULL),
        R
    );
}
insert_row(A) ::= LPAREN(L) insert_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, L, V, R);
}
```

The parser must not collapse omitted column lists and explicit empty column
lists into the same runtime shape. The planner needs that distinction because
`INSERT INTO t VALUES (1)` maps omitted columns to visible descriptor columns,
while `INSERT INTO t () VALUES ()` maps zero explicit targets and fills every
column from defaults.

## Semantics

Target mapping for `VALUES` statements:

| Column-list shape | First row shape | Target columns |
| --- | --- | --- |
| omitted | nonempty row | visible descriptor columns |
| omitted | empty row | zero explicit target columns |
| explicit nonempty list | any row | listed descriptor columns |
| explicit empty `()` | any row | zero explicit target columns |

After target mapping, every values row must have exactly the target count. This
preserves MySQL's deterministic `1136 / 21S01` diagnostics for mixed empty and
nonempty row lists.

When the target count is zero, every descriptor column is omitted. MyLite
materializes omitted values through existing descriptor-owned default logic:

- explicit descriptor defaults bind the descriptor value;
- nullable implicit or explicit `DEFAULT NULL` columns bind SQL `NULL`;
- integer expression defaults bind their stored evaluated value or `NULL`;
- current timestamp defaults bind the current statement timestamp;
- indexed persistent auto-increment columns generate the next value;
- strict omitted `NOT NULL` no-default columns fail with `1364 / HY000`;
- `INSERT IGNORE` demotes omitted no-default columns into warning `1364` and
  stores the existing MySQL-compatible implicit adjusted value for that
  descriptor family.

`INSERT ... ON DUPLICATE KEY UPDATE` receives a fully planned all-default row.
If that row conflicts with the current supported primary or unique-key subset,
the existing duplicate assignment planner runs. `VALUES(column)` observes the
planned default/generated value for the row, subject to the current duplicate
feature's same-target limitation.

No-key `REPLACE ... VALUES` uses the same all-default row planner and remains
insert-equivalent. Key-bearing `REPLACE` targets stay unsupported until
delete-insert replacement semantics are specified.

## Diagnostics

Expected diagnostics:

- syntax error for `INSERT ... VALUE ()` and `VALUES ROW()` in MyLite because
  those broader MySQL spellings remain outside the current grammar;
- `1136 / 21S01` for empty/nonempty row-shape mismatches;
- `1136 / 21S01` for explicit empty target list with a nonempty `SELECT`
  source;
- `1364 / HY000` for strict omitted `NOT NULL` no-default columns;
- warning `1364` for omitted no-default columns under `INSERT IGNORE`;
- existing duplicate-key, foreign-key, generated auto-increment, string-key,
  delayed warning, unknown schema/table/column, unsupported object kind,
  reserved-name, allocation, and physical SQLite diagnostics from the
  surrounding insert planner.

Supported in-range all-default rows report `warning_count == 0` unless the
surrounding statement already records warnings, such as `INSERT DELAYED`,
`INSERT IGNORE` adjusted values, or deprecated `VALUES(column)` in duplicate
assignments.

## Physical SQLite Handling And Performance

The runtime continues to build one prepared SQLite `INSERT` statement over all
physical descriptor columns and binds the planned row values. Empty target rows
do not cause table scans or C-side materialization of existing rows. The work is
proportional to the number of inserted rows and descriptor columns, matching the
existing insert planner.

This phase does not rely on SQLite table defaults and does not require
`INSERT ... DEFAULT VALUES` support. It uses MyLite descriptor defaults as the
only compatibility authority.

## Tests

Add MySQL-runtime-verified expectations and C tests covering:

- parser acceptance for `INSERT INTO t () VALUES ()`, `INSERT INTO t VALUES ()`,
  multi-row empty rows, and no-key `REPLACE` empty rows;
- parser/runtime rejection for `VALUE`, `VALUES ROW()`, explicit empty target
  plus nonempty value row, and mixed empty/nonempty row lists;
- successful all-default inserts over descriptor defaults including integer,
  decimal, string, temporal, expression-default, and nullable columns;
- strict `NOT NULL` no-default diagnostics;
- `INSERT IGNORE` omitted no-default warnings and adjusted stored values;
- auto-increment generation, affected rows, warning count, and
  `LAST_INSERT_ID()`;
- duplicate-key update interaction for default key values and `VALUES(column)`;
- no-key `REPLACE` all-default rows;
- explicit empty target list with `INSERT ... SELECT` mismatch;
- persistence across close/reopen, independent file-backed handles, and
  `.mylite` preamble preservation;
- zero-initialized cleanup for new parser/planner states if any are added;
- existing parser, insert, insert-ignore, duplicate-key, replace, defaults,
  auto-increment, foreign-key, runtime, file-backed, VFS, and catalog tests.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-dml.md`;
- default/literal compatibility docs only if the implementation changes the
  documented default materialization surface.

Do not overclaim full MySQL `INSERT`, `VALUE`, row constructors, `DEFAULT
VALUES`, full non-strict mode, key-bearing `REPLACE`, or general expression
conversion.
