# Baseline Insert Ignore Lifecycle

## Status

This feature specifies a narrow warning-demotion slice for descriptor-driven
`INSERT` statements. It builds on `mylite_execute()`, statement context,
diagnostics, parser scaffolding, durable catalog descriptors, existing
`INSERT ... VALUES`, existing `INSERT ... SET`, and the baseline
`LOW_PRIORITY` / `HIGH_PRIORITY` / `DELAYED` modifier support.

The slice admits `IGNORE` only for the currently supported `INSERT ... VALUES`
and `INSERT ... SET` forms. It does not add `INSERT IGNORE ... SELECT` because
that path currently materializes selected rows for validation and then inserts
from SQLite expressions; MySQL-compatible `IGNORE` for that form needs a
separate adjusted-row projection before the final insert.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline insert set lifecycle:
  `docs/specs/baseline-insert-set-lifecycle/specs.md`
- Baseline insert modifier lifecycle:
  `docs/specs/baseline-insert-modifier-lifecycle/specs.md`
- Baseline show warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline row count function:
  `docs/specs/baseline-row-count-function/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, server SQL modes and `IGNORE`:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_ignore_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INSERT IGNORE INTO ... VALUES` and `INSERT IGNORE INTO ... SET` succeed
  for ordinary InnoDB tables.
- `LOW_PRIORITY IGNORE`, `HIGH_PRIORITY IGNORE`, and `DELAYED IGNORE` are
  accepted when the priority or delayed word appears before `IGNORE`.
- `IGNORE LOW_PRIORITY` and repeated or mixed priority words remain syntax
  errors.
- `LOW_PRIORITY` and `HIGH_PRIORITY` remain no-ops when combined with
  `IGNORE`.
- `DELAYED IGNORE` records the delayed-conversion warning before value
  adjustment warnings.
- Assigning `NULL` to a numeric `NOT NULL` column inserts the numeric implicit
  default `0` and records warning `1048`.
- Omitting a numeric `NOT NULL` column with no explicit default inserts `0` and
  records one warning `1364` for that omitted column per statement, not per
  inserted row.
- Nullable omitted columns with the ordinary implicit `DEFAULT NULL` continue
  to receive `NULL` without a warning. Nullable columns whose default was
  explicitly dropped also receive `NULL`, but record one warning `1364` for
  that omitted column per statement.
- Out-of-range integer values are clipped to the MySQL endpoint for the target
  integer type and record warning `1264` with the affected row number.
- `TRUE` and `FALSE` are stored as `1` and `0` for currently supported integer
  targets and produce no warnings when in range.
- A successful adjusted multi-row `INSERT IGNORE ... VALUES` reports the number
  of inserted rows as `ROW_COUNT()` and as the public affected-row count.
- Non-ignorable name and shape errors still fail: unknown table, unknown
  column, duplicate target column, and value-count mismatch are errors.
- MySQL accepts `INSERT LOW_PRIORITY IGNORE ... SELECT`; MyLite defers that
  form in this slice.

## Scope

The implementation must add:

- parser and AST support for one optional `IGNORE` word after the existing
  optional priority/delayed modifier and before optional `INTO`;
- support for `IGNORE` on existing `INSERT ... VALUES` and `INSERT ... SET`
  forms only;
- combinations `LOW_PRIORITY IGNORE`, `HIGH_PRIORITY IGNORE`, and
  `DELAYED IGNORE` for those forms;
- `LOW_PRIORITY` and `HIGH_PRIORITY` as no-ops when combined with `IGNORE`;
- `DELAYED IGNORE` warning `3005` plus subsequent `IGNORE` conversion warnings;
- descriptor-driven schema, table, and column resolution unchanged;
- strict parse/name/shape diagnostics unchanged for non-ignorable errors;
- `NULL` into numeric `NOT NULL` converted to `0` with warning `1048`;
- omitted no-explicit-default columns warn with `1364`; numeric `NOT NULL`
  columns store `0`, while nullable dropped-default columns store `NULL`;
- out-of-range numeric values clipped to the current descriptor range with
  warning `1264`;
- affected rows equal to inserted rows for successful adjusted inserts;
- result warning count, `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`,
  `@@warning_count`, `@@error_count`, and `ROW_COUNT()` behavior consistent
  with the existing diagnostics and result conventions;
- descriptor-driven generated SQLite `INSERT` unchanged except for bound
  adjusted values;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  intentionally rejected wider forms.

## Non-Goals

This feature must not implement:

- `INSERT IGNORE ... SELECT`, `CREATE TABLE ... SELECT IGNORE`, `INSERT ...
  TABLE`, row constructors, `VALUE`, `ON DUPLICATE KEY UPDATE`, `DEFAULT`
  keyword values, row aliases, or partition selection;
- duplicate-key discard, primary/unique key descriptors, generated ids,
  auto-increment, `LAST_INSERT_ID()` mutation, protocol insert-id metadata, or
  storage-engine conflict algorithms;
- string, decimal, float, hex, bit, temporal, JSON, function, parameter,
  variable, column-to-column, arithmetic, or general expression conversion;
- table-qualified assignment targets, aliases, multiple target tables, joined
  inserts, arbitrary SQLite pass-through, or SQLite fork patches;
- warning demotion for `REPLACE`, `UPDATE IGNORE`, `DELETE IGNORE`, `LOAD
  DATA`, non-strict SQL modes, privileges, triggers, cascades, foreign keys, or
  generated columns.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement dispatch,
  result ownership, public misuse behavior, and failure cleanup.
- Lexer/parser/AST own syntax admission and source spans for the optional
  `IGNORE` node. Parser code remains independent of runtime, catalog, storage,
  and SQLite.
- Runtime planning owns detecting `IGNORE`, resolving descriptor targets, and
  performing MyLite-owned value adjustment before SQLite binding.
- Statement context and diagnostics own warning storage, warning counts,
  previous diagnostics, `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, and
  `@@warning_count`.
- Catalog descriptors remain authoritative for table names, column names,
  nullability, visibility, default values, integer families, signedness, and
  stable physical table names.
- SQLite remains physical row storage. MyLite binds adjusted integer/`NULL`
  values to prepared SQLite statements; SQLite does not own MySQL warning
  demotion, integer clipping policy, or metadata authority.
- Storage/VFS ownership does not change. The `.mylite` preamble and shifted
  SQLite payload invariants must be preserved.

## Supported SQL Grammar

Supported subset:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [IGNORE] [INTO]
    table_name [(column_name[, ...])]
    VALUES (value[, ...])[, ...]

INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [IGNORE] [INTO]
    table_name
    SET column_name = value[, ...]
```

`value` remains the existing decimal integer literal, optional unary sign,
`TRUE`, `FALSE`, or `NULL` subset. `table_name` remains the existing
unqualified or schema-qualified descriptor target name.

Unsupported examples for this slice include:

```sql
INSERT IGNORE LOW_PRIORITY INTO t VALUES (1)
INSERT LOW_PRIORITY HIGH_PRIORITY IGNORE INTO t VALUES (1)
INSERT IGNORE INTO t SELECT id FROM src
INSERT LOW_PRIORITY IGNORE INTO t SELECT id FROM src
INSERT IGNORE INTO t SET t.id = 1
INSERT IGNORE INTO t SET id = DEFAULT
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) insert_ignore_opt(G) INTO table_name(T)
    insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, G);
}
insert_values_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) insert_ignore_opt(G) table_name(T)
    insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R, M, G);
}

insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) insert_ignore_opt(G) INTO table_name(T)
    SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, G);
}
insert_set_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) insert_ignore_opt(G) table_name(T)
    SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S, M, G);
}

insert_ignore_opt(A) ::= . {
    A = NULL;
}
insert_ignore_opt(A) ::= IGNORE(T). {
    A = mylite_sql_parser_make_insert_ignore_modifier(state, T);
}
```

The existing `INSERT ... SELECT` grammar keeps `insert_modifier_opt` only in
this slice.

## Semantics

### Name Resolution

Unqualified targets use the selected/default schema. Schema-qualified targets
resolve the named schema. Missing default schema, unknown schema, unknown table,
reserved `_mylite_*` names, unsupported object kinds, duplicate target columns,
unknown target columns, and value-count mismatch keep the existing MySQL-shaped
or MyLite-specific diagnostics. `IGNORE` does not demote these errors.

### Adjustment and Warnings

`IGNORE` changes only the strict value/default errors included in this slice:

- `NULL` assigned to a numeric `NOT NULL` column stores `0` and appends warning
  `1048`, SQLSTATE `23000`, message `Column '<column>' cannot be null`.
- An omitted column without an explicit default appends warning `1364`,
  SQLSTATE `HY000`, message `Field '<column>' doesn't have a default value`.
  Numeric `NOT NULL` columns store `0`; nullable dropped-default columns store
  `NULL`.
- A decimal integer outside the target descriptor range stores the closest
  supported endpoint and appends warning `1264`, SQLSTATE `22003`, message
  `Out of range value for column '<column>' at row <n>`.

Omitted nullable columns with an implicit `DEFAULT NULL` store `NULL` without a
warning. Omitted columns with explicit integer defaults store that descriptor
default. `TRUE` and `FALSE` convert to `1` and `0` before range checks.

Warnings are appended in descriptor/value processing order. For
`DELAYED IGNORE`, the delayed warning is appended before ordinary insert
planning, so it appears before adjustment warnings.

### Integer Ranges

The target range is the same descriptor range used by strict `INSERT`,
`REPLACE`, and `UPDATE` conversion:

| Type | Supported Range |
| --- | --- |
| `TINYINT` | `-128..127` |
| `TINYINT UNSIGNED` | `0..255` |
| `SMALLINT` | `-32768..32767` |
| `SMALLINT UNSIGNED` | `0..65535` |
| `MEDIUMINT` | `-8388608..8388607` |
| `MEDIUMINT UNSIGNED` | `0..16777215` |
| `INT` / `INTEGER` | `-2147483648..2147483647` |
| `INT UNSIGNED` / `INTEGER UNSIGNED` | `0..4294967295` |
| `BIGINT` | `-9223372036854775808..9223372036854775807` |
| `BIGINT UNSIGNED` | `0..9223372036854775807` in the current physical storage slice |

The `BIGINT UNSIGNED` cap is a known MyLite baseline limitation inherited from
the signed-64 SQLite physical integer representation. MySQL can store unsigned
`BIGINT` values above this cap. MyLite clips to its current descriptor range
when `IGNORE` is active and documents that as a reduced-fidelity baseline gap.

### Result Behavior

Successful adjusted inserts return through the existing non-query result
conventions:

- no result rows;
- column count `0`;
- `affected_rows` equal to the number of inserted rows;
- `warning_count` equal to stored warnings for the statement;
- `ROW_COUNT()` after the statement returns the affected-row count;
- supported in-range `INSERT IGNORE` statements can have `warning_count == 0`.

`INSERT IGNORE` does not mutate catalog rows, descriptor versions, descriptor
caches, catalog generation, or SQLite schema generation.

## Physical SQLite Handling

MyLite continues to generate physical `INSERT INTO "<physical_table>" (...)`
statements from descriptors and stable physical table names. Every generated
SQLite identifier is quoted. Values are bound to prepared statements after
MyLite conversion, clipping, and warning generation.

The implementation must not use SQLite conflict clauses, triggers, generated
columns, optional SQLite syntax, or fork hooks for `IGNORE` semantics.

## Diagnostics

Supported warnings:

| Condition | Code | SQLSTATE | Message Shape |
| --- | ---: | --- | --- |
| `NULL` into numeric `NOT NULL` | `1048` | `23000` | `Column '<column>' cannot be null` |
| omitted no-explicit-default column | `1364` | `HY000` | `Field '<column>' doesn't have a default value` |
| integer out of descriptor range | `1264` | `22003` | `Out of range value for column '<column>' at row <n>` |
| `DELAYED` conversion | `3005` | `HY000` | existing delayed-conversion message |

Errors that remain errors include syntax errors, unsupported syntax, missing
default schema, unknown schema, unknown table, reserved target names,
unsupported object kind, unknown target column, duplicate target column, column
count mismatch, unsupported assignment target shape, unsupported value syntax,
physical SQLite failures, allocation failures, and public API misuse.

## Tests

The implementation tests must cover:

- parser support for `INSERT IGNORE`, `INSERT LOW_PRIORITY IGNORE`,
  `INSERT HIGH_PRIORITY IGNORE`, and `INSERT DELAYED IGNORE` on `VALUES` and
  `SET`;
- parser/runtime rejection for `IGNORE` before priority modifiers, repeated
  priority modifiers, `INSERT IGNORE ... SELECT`, qualified assignment targets,
  unsupported values, and ordinary shape/name errors;
- successful multi-row `VALUES` adjustment over signed and unsigned integer
  families in the current physical range;
- successful `SET` adjustment;
- `NULL` into `NOT NULL`, omitted no-default columns including nullable
  dropped-default descriptors, explicit defaults, ordinary nullable omitted
  columns, out-of-range clipping, and boolean inputs;
- delayed warning plus adjustment warnings and warning order;
- affected rows, warning count, `SHOW WARNINGS`, absence of result rows, and
  `ROW_COUNT()`;
- schema-qualified and unqualified targets, missing default schema, unknown
  schema, unknown table, and reserved names;
- reopen persistence, physical preamble preservation, independent handles, and
  zero-initialized cleanup for new planner state;
- regression coverage for existing parser, diagnostics, row values,
  insert-set, insert-select, insert-modifier, replace, update, delete, catalog,
  storage, and VFS tests.
