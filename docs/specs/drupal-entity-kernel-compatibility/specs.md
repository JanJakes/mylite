# Drupal Entity Kernel Compatibility

## Summary

This slice covers three MySQL compatibility gaps exposed by Drupal kernel and
entity-query tests over the MyLite `mysqli` harness:

- `VARCHAR(...) BINARY` and equivalent character-column `BINARY` shorthand;
- joined row-scalar projection of simple `CASE value WHEN compare THEN result
  ELSE fallback END` expressions;
- `MIN()` and `MAX()` over nonbinary string descriptor columns.

The goal is to admit these application shapes without broadening MyLite into a
general expression engine. The implementation stays in MyLite-owned parser,
planner, descriptor, and SQLite SQL-generation layers and requires no SQLite
fork patch.

## Compatibility Authority

- MySQL 8.4 Reference Manual, string data type syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html>
- MySQL 8.4 Reference Manual, flow-control functions:
  <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
- MySQL 8.4 Reference Manual, aggregate function descriptions:
  <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
- Runtime probes against local MySQL `8.4.9` container `mylite-mysql-849`,
  captured in
  `packages/libmylite/tests/mysql_drupal_entity_kernel_compatibility_expectations.sh`.

This specification is independently authored from MyLite project documents,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
sources.

## MySQL 8.4.9 Observations

- `VARCHAR(n) BINARY` remains a character `varchar(n)` column. It chooses the
  binary collation for the effective column character set rather than changing
  the descriptor to `VARBINARY(n)`.
- With a table default `CHARSET=ascii`, `VARCHAR(n) BINARY` records
  `CHARACTER_SET_NAME = ascii` and `COLLATION_NAME = ascii_bin`.
- With a schema/table default `utf8mb4`, `VARCHAR(n) BINARY` records
  `CHARACTER_SET_NAME = utf8mb4` and `COLLATION_NAME = utf8mb4_bin`.
- `VARCHAR(n) CHARACTER SET ascii BINARY` records `ascii` / `ascii_bin`.
- Simple `CASE base.revision_id WHEN revision.revision_id THEN 1 ELSE 0 END`
  over joined descriptor tables evaluates row by row and returns `1` for equal
  non-`NULL` integer values and `0` for unequal or `NULL` comparisons.
- `MIN()` and `MAX()` accept string arguments. They ignore `NULL`, return
  `NULL` for empty/all-`NULL` groups, and return the minimum or maximum string
  according to the expression collation.

## Scope

Supported by this slice:

- standalone `BINARY` as a column attribute for admitted `CHAR`, `VARCHAR`,
  and bare or normalized `TEXT` family descriptors;
- interaction with explicit `CHARACTER SET` / `CHARSET` and table/schema
  defaults for admitted `utf8mb4` and `ascii` character sets;
- descriptor metadata and `SHOW CREATE TABLE` / `SHOW COLUMNS` /
  `INFORMATION_SCHEMA.COLUMNS` visibility through existing charset/collation
  rendering paths;
- joined row-scalar projection of simple `CASE` with an integer descriptor
  case value, integer descriptor or literal `WHEN` values, and integer,
  boolean, or `NULL` result values;
- `MIN(column)` and `MAX(column)` over descriptor-backed nonbinary string
  columns in the existing single-table and grouped aggregate envelopes.

Deferred:

- full Unicode collation weights for aggregate ordering beyond MyLite's current
  string comparison subset;
- simple `CASE` in predicates, DML assignment, grouping, or arbitrary nested
  expression positions;
- simple `CASE` over string, decimal, float, temporal, JSON, enum, set, or
  binary string operands;
- `BINARY` shorthand on `ENUM` and `SET`;
- descriptor-changing `CHARACTER SET binary` / `COLLATE binary` beyond the
  existing limited binary string normalization path.

## Grammar

The intended MyLite Lemon additions are:

```lemon
column_attribute(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_column_binary_collation_attribute(state, T);
}
```

Existing `CASE` and aggregate grammar nodes are reused. No new SQL syntax is
needed for simple `CASE` projection or string `MIN()` / `MAX()`.

## Semantics

`BINARY` as a character-column attribute is a binary-collation shorthand. It is
equivalent to choosing the `_bin` collation for the effective character set.
When the column does not name a character set, MyLite resolves the shorthand
after table options are known so `DEFAULT CHARSET=ascii` yields `ascii_bin`.

Simple row-scalar `CASE` is lowered to generated SQLite SQL over descriptor
columns. The supported shape preserves MySQL equality semantics for admitted
integer operands: comparisons involving `NULL` do not match.

String `MIN()` and `MAX()` use SQLite's aggregate execution over descriptor
owned physical columns. The values are returned through the existing result
builder as text or `NULL`. This follows the current MyLite string comparison
scope and does not introduce a new SQLite fork hook.

## Tests

- MySQL expectation script for the three observed shapes and metadata.
- Native runtime test for descriptor metadata, joined simple `CASE`, and string
  `MIN()` / `MAX()` behavior.
- Focused Drupal re-run of `tests/Drupal/KernelTests/Core/Entity/EntityQueryTest.php`.
