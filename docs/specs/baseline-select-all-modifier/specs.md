# Baseline SELECT ALL Modifier

## Summary

This feature adds MySQL's explicit `ALL` select modifier for the `SELECT` forms
MyLite already supports. `ALL` is the duplicate-preserving default, so this
slice normalizes it to the existing non-distinct select path.

The supported surface is:

```sql
SELECT ALL select_item_list [FROM DUAL]
SELECT ALL select_item_list FROM table_name [WHERE ...] [ORDER BY ...] [LIMIT ...]
SELECT ALL * FROM table_name [WHERE ...] [ORDER BY ...] [LIMIT ...]
```

`select_item_list` remains limited to the runtime forms MyLite already admits:
session scalar functions and variables, descriptor column references, and the
existing one-item aggregate forms. This feature does not add arbitrary literal
projection or any other new projection expression.

## Sources And Runtime Evidence

Normative source:

- MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>

The MySQL documentation lists `ALL`, `DISTINCT`, and `DISTINCTROW` after
`SELECT`; it states that `ALL` is the default and returns duplicate rows, while
`DISTINCT` removes duplicates.

Observed MySQL 8.4.9 behavior, using local Docker runtime
`mylite-mysql-849`:

```sql
SELECT VERSION();
-- 8.4.9

CREATE DATABASE mylite_all_probe;
USE mylite_all_probe;
CREATE TABLE t(id INT NOT NULL, n INT NULL, b BOOL NULL);
INSERT INTO t VALUES
  (1, NULL, TRUE),
  (2, 20, FALSE),
  (3, 20, FALSE),
  (4, 30, NULL),
  (5, NULL, TRUE);

DO 0;
SELECT n FROM t ORDER BY n LIMIT 10;
SELECT @@warning_count, ROW_COUNT();
DO 0;
SELECT ALL n FROM t ORDER BY n LIMIT 10;
SELECT @@warning_count, ROW_COUNT();
SELECT ALL * FROM t ORDER BY id LIMIT 2;
SELECT ALL n FROM t WHERE n IS NOT NULL ORDER BY n DESC LIMIT 2;
SELECT ALL VERSION();
SELECT ALL 1;
SELECT ALL COUNT(*) FROM t;
SELECT ALL COUNT(n), COUNT(DISTINCT n), MIN(n), MAX(n) FROM t;
SELECT ALL ALL 1;
SELECT ALL DISTINCT 1;
```

Observed results:

- `SELECT n ...` and `SELECT ALL n ...` both returned duplicates:
  `NULL`, `NULL`, `20`, `20`, `30`.
- Both successful row-returning statements left `@@warning_count = 0` and
  following `ROW_COUNT() = -1`.
- `SELECT ALL *` returned all visible columns, preserving duplicate row values.
- The filtered/ordered/limited statement returned `30`, `20`.
- `SELECT ALL VERSION()`, `SELECT ALL 1`, `SELECT ALL COUNT(*)`, and
  `SELECT ALL COUNT(n), COUNT(DISTINCT n), MIN(n), MAX(n)` were accepted with
  the same results as their default-modifier equivalents.
- MyLite does not add `SELECT ALL 1` runtime support in this slice because
  default-modifier literal projection is not supported yet.
- MySQL accepts repeated `ALL`, such as `SELECT ALL ALL 1`; this is outside
  this narrow slice.
- Mixing `ALL` with `DISTINCT` or `DISTINCTROW` fails with MySQL error
  `1221 (HY000) Incorrect usage of ALL and DISTINCT`; MyLite may return its
  generic unsupported/syntax diagnostic for mixed modifiers until repeated and
  mixed modifier handling is specified.

## Scope

In scope:

- one explicit `ALL` token immediately after `SELECT`;
- all currently supported non-distinct select shapes:
  - session scalar function and variable selects without a source;
  - supported session scalar function and variable selects from `DUAL`;
  - descriptor-backed table `SELECT` column lists and wildcard;
  - existing one-item aggregate forms, including table-backed aggregates with
    optional baseline `WHERE`;
  - existing descriptor-backed table `WHERE`, `ORDER BY`, and `LIMIT` subsets;
- normal descriptor resolution, catalog authority, generated SQL, result
  metadata conventions, warning counts, and `ROW_COUNT()` behavior for the
  underlying default-modifier select.

Out of scope:

- repeated `ALL`;
- mixed `ALL DISTINCT` / `ALL DISTINCTROW` modifiers and exact error `1221`;
- arbitrary literal projection such as `SELECT ALL 1` until the default
  modifier form is supported;
- combining `ALL` with unsupported modifiers such as `HIGH_PRIORITY`,
  `STRAIGHT_JOIN`, `SQL_SMALL_RESULT`, `SQL_BIG_RESULT`, `SQL_BUFFER_RESULT`,
  `SQL_NO_CACHE`, or `SQL_CALC_FOUND_ROWS`;
- `ALL` on otherwise unsupported expressions, joins, grouping, set operations,
  locking clauses, or table-value statements.

## Architecture

`ALL` is parser compatibility sugar. The parser maps `ALL` to a select
statement with `MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT`; no runtime-specific
`ALL` state is needed.

The analyzer/runtime continues to own all MySQL compatibility semantics:

- statement context and public API behavior are unchanged;
- descriptor-backed table and column resolution remain catalog-driven;
- reserved `_mylite_*` names are rejected by the existing table-resolution path;
- generated SQLite SQL remains the existing non-distinct `SELECT ...` shape;
- identifiers remain quoted and predicate/limit values remain bound parameters;
- SQLite continues to execute filtering, sorting, limiting, aggregate
  computation, and row scans; and
- MyLite does not mutate catalog rows, descriptor generations, file-format
  preamble bytes, VFS state, or the SQLite fork.

## Syntax

MyLite grammar snippets, independently authored for this slice:

```lemon
select_statement ::= SELECT ALL select_item_list.
select_statement ::= SELECT ALL select_item_list FROM DUAL.
select_statement ::=
    SELECT ALL select_item_list FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt.

select_statement ::= SELECT ALL STAR.
select_statement ::= SELECT ALL STAR FROM DUAL.
select_statement ::=
    SELECT ALL STAR FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt.
```

Each production calls the same parser builder as the existing default-modifier
production. The AST select modifier remains `DEFAULT`. The no-source and
`FROM DUAL` wildcard productions are parser compatibility with the existing
default-modifier grammar; this slice does not add runtime support for wildcard
selects without descriptor-backed table columns.

## Semantics

For every supported statement, `SELECT ALL ...` is equivalent to omitting the
modifier.

- Duplicate rows are preserved.
- Table-backed `SELECT ALL *` expands visible descriptor columns exactly like
  `SELECT *`.
- Explicit descriptor columns, predicates, order keys, aggregate arguments, and
  invisible-column behavior are unchanged.
- Successful row-returning statements return result rows, `affected_rows == 0`,
  `warning_count == 0`, and following `ROW_COUNT() == -1`.
- Unsupported shapes fail through existing parser or runtime diagnostics.

## Tests

Add MySQL-runtime-verified expectations for:

- duplicate-preserving table selects compared to default `SELECT`;
- wildcard table selects;
- predicate/order/limit composition;
- no-source and `FROM DUAL` session scalar selects;
- supported aggregate forms;
- warning count and following `ROW_COUNT()`;
- repeated `ALL` and mixed `ALL DISTINCT` observations, documented as out of
  scope.

Add fast C coverage by extending the existing parser and runtime lifecycle
tests:

- parse `SELECT ALL` scalar, `FROM DUAL`, table-backed, wildcard, and aggregate
  forms into the default select modifier;
- execute table-backed `SELECT ALL` column lists and wildcard with duplicate
  preservation;
- execute session scalar and aggregate forms where current runtime support
  already exists;
- cover schema-qualified table names, reopen persistence, rename/drop behavior,
  and diagnostics by sampling representative `ALL` forms; and
- preserve existing `SELECT`, `DISTINCT`, `DISTINCTROW`, aggregate, parser,
  storage, and VFS tests.
