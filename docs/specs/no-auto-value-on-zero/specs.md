# NO_AUTO_VALUE_ON_ZERO SQL Mode

## Scope

This feature adds MyLite support for the MySQL 8.4 `NO_AUTO_VALUE_ON_ZERO`
SQL mode across the currently executable auto-increment insert surfaces:

- `SET [SESSION|LOCAL] sql_mode = 'NO_AUTO_VALUE_ON_ZERO'`
- canonical `SHOW VARIABLES LIKE 'sql_mode'` and `@@sql_mode` exposure
- `INSERT ... VALUES`
- `INSERT ... SET`
- `REPLACE ... VALUES`
- `REPLACE ... SET`
- current expression-valued insert paths that resolve to numeric zero

## Sources

- MySQL 8.4 Reference Manual, Server SQL Modes:
  https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- MySQL 8.4 Reference Manual, Numeric Type Attributes:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-attributes.html

This specification is independently authored from official documentation and
the existing MyLite auto-increment design.

## Semantics

Without `NO_AUTO_VALUE_ON_ZERO`, MySQL treats explicit `0` for an
`AUTO_INCREMENT` column like `NULL`: it allocates the next sequence value.
MyLite already implements that default behavior.

With `NO_AUTO_VALUE_ON_ZERO` enabled:

- explicit numeric zero stores `0` in an `AUTO_INCREMENT` column
- `NULL`, omitted values, and `DEFAULT` still allocate generated values
- stored `0` is an explicit value, so it does not update last insert id
- stored `0` participates in primary/unique duplicate checks normally
- explicit zero does not advance the auto-increment sequence beyond the normal
  "advance after accepted explicit high values" rule

The mode is session state. Global assignment remains unsupported, matching the
existing `sql_mode` implementation. The default MySQL 8.4 mode list does not
include `NO_AUTO_VALUE_ON_ZERO`.

## SQL Mode Canonicalization

`NO_AUTO_VALUE_ON_ZERO` becomes a recognized `sql_mode` token. It canonicalizes
case-insensitively, de-duplicates with other modes, and appears in the stored
mode list using the same canonical ordering as the rest of MyLite's recognized
mode catalog.

The `TRADITIONAL` and `ANSI` combination modes do not imply
`NO_AUTO_VALUE_ON_ZERO`.

## Implementation Notes

The insert value resolver should decide whether zero allocates through a single
helper that checks both the target column and the current connection SQL mode.
All supported literal, text-converted, and expression-converted numeric zero
paths should use the same helper so `INSERT ... VALUES`, `INSERT ... SET`, and
`REPLACE` remain consistent.

## Deferred

- protocol OK packet insert-id behavior
- exact conversion warnings for non-integer values that coerce to zero
- broad type range and clipping behavior
- interaction with unsupported insert-from-query sources
- replication-specific auto-increment modes

## Tests

Runtime coverage must verify:

- `SET SESSION sql_mode = 'NO_AUTO_VALUE_ON_ZERO'` succeeds and is visible
- default mode still treats explicit `0` as generated
- enabled mode stores explicit `0` in `INSERT ... VALUES`
- enabled mode stores explicit `0` in `INSERT ... SET`
- enabled mode still generates for `NULL` and `DEFAULT`
- explicit `0` leaves `mylite_last_insert_id()` unchanged
- duplicate explicit `0` fails through the existing duplicate-key path
- clearing/restoring `sql_mode` restores default zero-as-generated behavior
