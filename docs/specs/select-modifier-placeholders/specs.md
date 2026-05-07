# SELECT Modifier Parser Placeholders

## Scope

This feature accepts MySQL SELECT-level optimizer and execution-strategy
modifiers that appear between `SELECT` and the projection list:

- `HIGH_PRIORITY`
- `SQL_SMALL_RESULT`
- `SQL_BIG_RESULT`
- `SQL_BUFFER_RESULT`
- `STRAIGHT_JOIN`

The modifiers are parser/runtime placeholders over the SELECT surfaces MyLite
already executes: no-table scalar SELECTs, current table-backed SELECTs,
current joined SELECTs, and current duplicate-mode interactions. They do not
broaden row-source, expression, aggregate, query-expression, or optimizer
support.

Out of scope:

- table-reference `STRAIGHT_JOIN` join syntax
- optimizer join-order forcing or result materialization strategy
- warnings or diagnostics for accepted modifiers
- `SELECT ... INTO`, file export variants, views, derived tables, CTEs, and
  deferred row-source surfaces

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- Runtime probes against local `mysql:8.4.9` container
  `mylite-mysql-849` on 2026-05-07.

This specification is independently authored from official documentation and
observed MySQL runtime behavior.

## Observed MySQL Behavior

The following probes all return the same rows as the equivalent unmodified
SELECT and do not produce warnings:

| Query shape | Observed result |
| --- | --- |
| `SELECT HIGH_PRIORITY 1 AS x` | one row, `x = 1` |
| `SELECT SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT 1 AS x` | one row, `x = 1` |
| `SELECT DISTINCT HIGH_PRIORITY id FROM p ORDER BY id` | distinct rows |
| `SELECT SQL_BUFFER_RESULT DISTINCT id FROM p ORDER BY id` | distinct rows |
| `SELECT HIGH_PRIORITY STRAIGHT_JOIN ... FROM p INNER JOIN m ...` | joined rows |

MySQL permits these SELECT-level modifiers to appear before or after the
duplicate-mode tokens accepted by MyLite's current parser. They do not create
visible result metadata differences in the covered cases.

## Syntax

MyLite Lemon grammar shape:

```lemon
select_duplicate_mode_item ::= ALL.
select_duplicate_mode_item ::= DISTINCT.
select_duplicate_mode_item ::= DISTINCTROW.
select_duplicate_mode_item ::= SQL_CALC_FOUND_ROWS.
select_duplicate_mode_item ::= HIGH_PRIORITY.
select_duplicate_mode_item ::= SQL_SMALL_RESULT.
select_duplicate_mode_item ::= SQL_BIG_RESULT.
select_duplicate_mode_item ::= SQL_BUFFER_RESULT.
select_duplicate_mode_item ::= STRAIGHT_JOIN.
```

The placeholder modifiers reuse the duplicate-mode list position because MySQL
places all of these tokens in the SELECT option area before the projection
list. Placeholder items do not set explicit duplicate mode, do not increment
the duplicate-mode modifier count, and do not conflict with `ALL`,
`DISTINCT`, or `DISTINCTROW`.

`SQL_CALC_FOUND_ROWS` keeps its existing behavior and warning semantics.

## Runtime Semantics

For the supported SELECT surfaces, placeholder modifiers are ignored after
parsing. They must not:

- change row ordering, row count, duplicate elimination, or expression values
- add warnings
- change result metadata
- affect `FOUND_ROWS()` state except through already implemented
  `SQL_CALC_FOUND_ROWS` semantics

This is a compatibility placeholder for applications that emit MySQL optimizer
or priority modifiers around otherwise supported SELECT statements.

## Deferred

- optimizer-specific materialization, buffering, and result-size strategy
- SELECT-level priority scheduling
- table-reference `STRAIGHT_JOIN`
- unsupported SELECT surfaces listed in scope

## Tests

Coverage includes:

- parser acceptance for scalar SELECT modifier combinations
- parser acceptance with `DISTINCT` before and after placeholder modifiers
- preservation of `SQL_CALC_FOUND_ROWS` parsing alongside placeholders
- runtime scalar SELECT no-op behavior and warning count preservation
- runtime table-backed SELECT no-op behavior and warning count preservation
- runtime joined SELECT no-op behavior for SELECT-level `STRAIGHT_JOIN`
