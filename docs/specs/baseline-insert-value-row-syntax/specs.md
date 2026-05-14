# Baseline INSERT VALUE And ROW Syntax

## Summary

This phase admits MySQL's alternate values-list spellings on the already
supported descriptor-driven values DML paths:

```sql
INSERT INTO table_name VALUE (...);
INSERT INTO table_name VALUE (...), (...);
INSERT INTO table_name VALUES ROW(...);
INSERT INTO table_name VALUES ROW(...), ROW(...);
REPLACE INTO table_name VALUE (...);
REPLACE INTO table_name VALUES ROW(...);
```

The feature is syntax normalization only. Parsed rows feed the same
descriptor-owned `INSERT ... VALUES`, `INSERT IGNORE`, limited
`INSERT ... ON DUPLICATE KEY UPDATE`, and no-key `REPLACE ... VALUES`
planners used by ordinary `VALUES (...)` rows. It does not add table value
constructors, `VALUES` statements, `INSERT ... TABLE`, `REPLACE ... TABLE`,
general row constructors in expressions, or new literal/expression support.

## References And Evidence

Normative documentation:

- MySQL 8.4 Reference Manual, `INSERT`:
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
- MySQL 8.4 Reference Manual, `REPLACE`:
  <https://dev.mysql.com/doc/refman/8.4/en/replace.html>

Observed MySQL 8.4.9 behavior:

- `VALUE` is accepted where ordinary `VALUES (...)` row lists are accepted for
  `INSERT` and `REPLACE`.
- `VALUE` may introduce one row or multiple parenthesized rows.
- `VALUES ROW(...)` accepts one or multiple `ROW(...)` constructors for
  `INSERT` and `REPLACE`.
- `VALUES ROW()` inserts an all-default row when the omitted target and empty
  row semantics from `baseline-empty-insert-values` apply.
- `INSERT ... VALUE ROW()` is a syntax error. `VALUE` is the synonym for the
  ordinary parenthesized row-list branch, not the `ROW(...)` constructor branch.
- Mixed ordinary rows and row constructors in the same values source are syntax
  errors.
- Row-shape mismatches in admitted row-constructor lists use the same
  `ERROR 1136 (21S01)` diagnostics as ordinary values rows.

The executable expectation artifact for this phase is:

```sh
packages/libmylite/tests/mysql_baseline_insert_value_row_syntax_expectations.sh
```

It must run against MySQL 8.4.9.

## Supported SQL

This phase extends only the current `INSERT ... VALUES` and no-key
`REPLACE ... VALUES` paths.

Supported `INSERT` shapes:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [IGNORE]
    [INTO] table_name [(column_name[, ...])]
    VALUE insert_row[, ...]
    [ON DUPLICATE KEY UPDATE column_name = duplicate_value]

INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [IGNORE]
    [INTO] table_name [(column_name[, ...])]
    VALUES row_constructor[, ...]
    [ON DUPLICATE KEY UPDATE column_name = duplicate_value]
```

Supported `REPLACE` shapes:

```sql
REPLACE [LOW_PRIORITY | DELAYED]
    [INTO] table_name [(column_name[, ...])]
    VALUE insert_row[, ...]

REPLACE [LOW_PRIORITY | DELAYED]
    [INTO] table_name [(column_name[, ...])]
    VALUES row_constructor[, ...]
```

`insert_row` is the existing MyLite parenthesized insert row:

```sql
(insert_value[, ...])
()
```

`row_constructor` is:

```sql
ROW(insert_value[, ...])
ROW()
```

Each `insert_value` is exactly the value subset already admitted by the
ordinary values-list planner for the target statement. This includes supported
integer, decimal, approximate, string, hex, bit, boolean, `NULL`, and
`DEFAULT` values only where already supported by the current descriptor
conversion rules. This phase does not broaden expression support.

## Unsupported SQL

Unsupported in this phase:

- `VALUE ROW(...)`;
- mixing ordinary parenthesized rows and `ROW(...)` constructors in the same
  values source;
- standalone `VALUES` statements or table value constructors;
- `INSERT ... TABLE` or `REPLACE ... TABLE`;
- `ROW(...)` expressions outside the admitted DML values source;
- `INSERT ... DEFAULT VALUES`;
- wider insert/replace literals, expressions, parameters, subqueries, or
  conversion behavior beyond the existing values path;
- key-bearing `REPLACE` duplicate delete-insert behavior, which remains
  outside the current no-key `REPLACE` slice.

## Parser And AST

The parser should normalize alternate syntax into the existing
`MYLITE_SQL_AST_INSERT_ROW_LIST` / `MYLITE_SQL_AST_INSERT_ROW` structure. The
runtime should not need to know whether a row came from `(1, 2)` or
`ROW(1, 2)`.

Independently authored Lemon-shape snippet:

```lemon
insert_values_statement ::=
    INSERT insert_modifier_opt into_opt table_name insert_column_list_opt
    insert_values_source on_duplicate_key_update_opt.

replace_values_statement ::=
    REPLACE replace_modifier_opt into_opt table_name insert_column_list_opt
    replace_values_source.

insert_values_source ::= insert_values_keyword insert_row_list.
insert_values_source ::= VALUES insert_row_constructor_list.

replace_values_source ::= insert_values_keyword insert_row_list.
replace_values_source ::= VALUES insert_row_constructor_list.

insert_values_keyword ::= VALUES.
insert_values_keyword ::= VALUE.

insert_row_constructor_list ::= insert_row_constructor.
insert_row_constructor_list ::= insert_row_constructor_list COMMA insert_row_constructor.

insert_row_constructor ::= ROW LPAREN insert_value_list RPAREN.
insert_row_constructor ::= ROW LPAREN RPAREN.
```

The `ROW(...)` parser action must build the same insert-row AST kind as the
ordinary parenthesized row. Empty `ROW()` should build an empty insert row, so
the existing all-default target planning continues to apply.

## Runtime Semantics

Runtime behavior is inherited from the existing values planners:

- target table, schema, descriptor, and reserved-name resolution are unchanged;
- omitted nonempty rows still map to visible descriptor columns;
- omitted empty rows and explicit empty target lists still map zero explicit
  targets and materialize descriptor defaults;
- `INSERT IGNORE` warning demotion, duplicate-key update behavior,
  auto-increment, `LAST_INSERT_ID()`, foreign-key checks, no-key `REPLACE`,
  file-backed persistence, and statement atomicity are unchanged;
- row-shape validation uses the same `1136 / 21S01` diagnostics as ordinary
  values rows.

No catalog rows, descriptor versions, descriptor caches, file preamble bytes,
or SQLite schema text are changed by admitting this syntax.

## SQLite And Performance

This phase is MyLite parser normalization plus existing runtime planning. The
generated SQLite remains the same prepared physical `INSERT` shape used by
ordinary values rows over stable MyLite physical table names. Identifiers and
values continue through the existing descriptor-driven quoting and binding
paths. No SQLite fork patch or optional SQLite grammar is needed.

The alternate syntax does not add materialization beyond the existing per-row
planning and binding performed for supported values inserts/replaces.

## Diagnostics

Expected diagnostics:

- unsupported or mixed syntax rejected by the parser with `1064 / 42000`;
- row-shape mismatches rejected by the existing values row-count diagnostic
  `1136 / 21S01`;
- target/column/literal/default/range/nullability/key/foreign-key errors are
  inherited from the existing values planners;
- physical SQLite, allocation, and public API misuse errors are unchanged.

## Tests

Add MySQL-runtime-verified expectations and C tests for:

- `INSERT ... VALUE ()`;
- `INSERT ... VALUE (...), (...)`;
- `INSERT ... VALUES ROW()`;
- `INSERT ... VALUES ROW(...), ROW(...)`;
- `INSERT IGNORE` with row constructors and all-default rows;
- limited `ON DUPLICATE KEY UPDATE` with `VALUE` and row constructors;
- no-key `REPLACE ... VALUE` and `REPLACE ... VALUES ROW(...)`;
- row-shape mismatch diagnostics;
- parser rejection for `VALUE ROW(...)` and mixed row forms;
- persistence through the existing file-backed values path.
