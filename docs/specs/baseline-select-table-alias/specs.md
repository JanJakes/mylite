# Baseline SELECT Table Alias

## Summary

This feature accepts a single table alias on the descriptor-backed
single-table `SELECT` forms MyLite already supports. The alias is parser and
statement-shape compatibility only in this slice: selected columns, predicates,
ordering columns, and aggregate arguments remain unqualified descriptor column
names.

The supported surface is:

```sql
SELECT select_item_list FROM table_name [AS] alias [WHERE ...] [ORDER BY ...] [LIMIT ...]
SELECT ALL select_item_list FROM table_name [AS] alias [WHERE ...] [ORDER BY ...] [LIMIT ...]
SELECT DISTINCT column_name FROM table_name [AS] alias [WHERE ...] [ORDER BY column_name] [LIMIT ...]
SELECT DISTINCTROW column_name FROM table_name [AS] alias [WHERE ...] [ORDER BY column_name] [LIMIT ...]
SELECT * FROM table_name [AS] alias [WHERE ...] [ORDER BY ...] [LIMIT ...]
SELECT COUNT(...) FROM table_name [AS] alias [WHERE ...]
SELECT MIN(column_name) FROM table_name [AS] alias [WHERE ...]
SELECT MAX(column_name) FROM table_name [AS] alias [WHERE ...]
```

`table_name` may be unqualified or schema-qualified. `alias` is one MyLite
identifier, including quoted identifiers already accepted by the parser.

## Sources And Runtime Evidence

Normative source:

- MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>

The MySQL documentation describes table references in `SELECT` and states that
each named table may have an optional alias, using either `tbl_name AS alias` or
`tbl_name alias`. It also documents that `db_name.tbl_name` may be used for an
explicit schema and that column references may be qualified. This slice admits
only the table-reference alias part.

Observed MySQL 8.4.9 behavior, using local Docker runtime
`mylite-mysql-849`:

```sql
SELECT VERSION();
-- 8.4.9

CREATE DATABASE mylite_alias_probe;
USE mylite_alias_probe;
CREATE TABLE t(id INT NOT NULL, n INT NULL);
INSERT INTO t VALUES (1,10),(2,NULL),(3,10);

DO 0;
SELECT n FROM t AS a ORDER BY id;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT n FROM t a ORDER BY id;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT * FROM t AS a ORDER BY id LIMIT 2;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT n FROM mylite_alias_probe.t AS a
WHERE n IS NOT NULL ORDER BY n DESC LIMIT 1;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT DISTINCT n FROM t AS a ORDER BY n LIMIT 10;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT COUNT(*) FROM t AS a;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT MIN(n), MAX(n) FROM t AS a;
SELECT @@warning_count, ROW_COUNT();
```

Observed results:

- `AS` and bare aliases are accepted and do not change row values.
- Schema-qualified aliased table references are accepted.
- Successful row-returning statements leave `@@warning_count = 0` and
  following `ROW_COUNT() = -1`.
- `SELECT DISTINCT`, `SELECT *`, `COUNT(*)`, `MIN()`, and `MAX()` accept aliased
  single-table sources.
- `SELECT a.n FROM t AS a` and alias-qualified `WHERE` / `ORDER BY` references
  are accepted by MySQL. MyLite defers alias-qualified column resolution in this
  slice.
- After a table is aliased, `SELECT t.n FROM t AS a` fails in MySQL with
  `1054 (42S22) Unknown column 't.n' in 'field list'`. MyLite already rejects
  table-qualified selected columns for supported selects, so it does not add
  this original-name shadowing behavior yet.
- `SELECT n FROM t AS t` is accepted.
- Unquoted reserved words such as `SELECT` are not accepted as aliases, while
  quoted reserved aliases such as `` `select` `` are accepted.
- Missing default schema, unknown schema, and unknown table diagnostics are the
  same as unaliased table references.

## Scope

In scope:

- one optional table alias after the single descriptor-backed table source;
- optional `AS`;
- bare alias syntax with an ordinary MyLite identifier token;
- quoted alias identifiers;
- aliases equal to the table name;
- unqualified and schema-qualified table names;
- current default, explicit `ALL`, limited one-column `DISTINCT` /
  `DISTINCTROW`, wildcard, `COUNT`, `MIN`, and `MAX` table-backed select forms;
- current baseline `WHERE`, `ORDER BY`, `LIMIT`, descriptor resolution,
  generated SQL, result metadata, warning count, and `ROW_COUNT()` behavior for
  the underlying select form.

Out of scope:

- alias-qualified selected columns, predicates, order keys, aggregate
  arguments, or wildcard references;
- original-table-qualified column semantics after an alias is supplied;
- select-item aliases;
- joins, multiple table references, derived tables, CTEs, lateral references,
  subqueries, table functions, `TABLE`, and set operations;
- duplicate alias detection beyond existing unrelated DML paths;
- index hints, partitions, lock clauses, optimizer hints, query modifiers not
  already supported, grouping, having, windows, and collations;
- protocol-grade alias metadata beyond the existing public result conventions.

## Architecture

The public API and statement context are unchanged. `mylite_execute()` continues
to parse SQL, plan from the AST, execute prepared SQLite statements, and return
the existing result object.

The parser stores the optional alias as a second child of
`MYLITE_SQL_AST_FROM_TABLE`. Child `0` remains the schema/table name used by all
existing analyzers. Child `1`, when present, is the alias identifier. This keeps
the AST explicit without changing current table resolution.

The runtime continues to treat MyLite catalog descriptors as authoritative:

- table resolution uses only the table-name child and the existing
  selected/default schema policy;
- reserved `_mylite_*` schema and table names are rejected by the existing
  target-resolution path before SQLite SQL is generated;
- the alias is not a catalog object and is not checked against reserved
  MyLite physical names in this slice;
- selected columns, predicate columns, order columns, and aggregate arguments
  still resolve only as unqualified descriptor columns;
- unknown column diagnostics remain tied to the same field, where, or order
  clause contexts as the unaliased statement;
- generated SQLite SQL continues to reference the stable physical table name
  such as `_mylite_user_table_<table_id>`;
- every generated SQLite identifier remains quoted, and predicate and limit
  values remain bound parameters;
- SQLite still executes row scans, filtering, sorting, limiting, duplicate
  removal, and aggregate computation; and
- no catalog rows, descriptor generations, `sqlite_schema_generation`, file
  preamble bytes, storage/VFS behavior, or SQLite fork code change.

Since the alias is not emitted into generated SQLite SQL, this slice adds no
additional SQLite planning layer and no row materialization beyond existing
public result construction.

## Syntax

MyLite grammar snippets, independently authored for this slice:

```lemon
table_alias_opt ::= .
table_alias_opt ::= AS identifier.
table_alias_opt ::= identifier.

select_statement ::=
    SELECT select_item_list FROM table_name table_alias_opt
    where_clause_opt order_clause_opt limit_clause_opt.

select_statement ::=
    SELECT ALL select_item_list FROM table_name table_alias_opt
    where_clause_opt order_clause_opt limit_clause_opt.

select_statement ::=
    SELECT DISTINCT select_item_list FROM table_name table_alias_opt
    where_clause_opt order_clause_opt limit_clause_opt.

select_statement ::=
    SELECT DISTINCTROW select_item_list FROM table_name table_alias_opt
    where_clause_opt order_clause_opt limit_clause_opt.

select_statement ::=
    SELECT STAR FROM table_name table_alias_opt
    where_clause_opt order_clause_opt limit_clause_opt.
```

The wildcard, `ALL`, `DISTINCT`, and `DISTINCTROW` table productions pass the
alias to the `FROM_TABLE` builder. Aggregate expressions reuse the
`select_item_list` productions and therefore use the same table-alias syntax.

## Semantics

For supported statements, the alias has no visible effect except accepting the
SQL shape MySQL accepts.

- Row values, row order, duplicate handling, aggregate results, `LIMIT`, and
  `OFFSET` behavior are exactly the same as the corresponding unaliased
  supported statement.
- `SELECT *` still expands visible descriptor columns in catalog ordinal order.
- Explicit descriptor columns and aggregate arguments may still name invisible
  columns.
- Successful row-returning statements return result rows, `affected_rows == 0`,
  `warning_count == 0`, and following `ROW_COUNT() == -1`.
- Missing default schema, unknown schema, unknown table, reserved schema/table
  target names, unknown selected columns, unknown predicate columns, unknown
  ordering columns, unsupported qualified column references, unsupported
  projection expressions, physical SQLite failures, allocation failures, and
  public API misuse continue through the existing diagnostics.
- Unsupported alias-qualified references fail through current deterministic
  MyLite diagnostics, typically the same "unqualified descriptor column"
  messages used for table-qualified references.

## Tests

Add MySQL-runtime-verified expectations for:

- `AS` and bare table aliases;
- schema-qualified aliased table sources;
- wildcard, `ALL`, `DISTINCT`, `DISTINCTROW`, `COUNT`, `MIN`, and `MAX` forms;
- current `WHERE`, `ORDER BY`, and `LIMIT` composition;
- warning count and following `ROW_COUNT()`;
- alias-qualified column references accepted by MySQL but deferred by MyLite;
- original-table-qualified references after aliasing;
- aliases equal to table names;
- quoted reserved aliases and unquoted reserved alias syntax errors; and
- missing default schema, unknown schema, and unknown table diagnostics.

Add fast C coverage by extending existing parser and runtime lifecycle tests:

- parse `FROM table AS alias` and `FROM table alias` for supported default,
  `ALL`, `DISTINCT`, `DISTINCTROW`, wildcard, schema-qualified, and aggregate
  select forms;
- ensure `FROM_TABLE` child `0` remains the table name and child `1` stores the
  alias when present;
- execute representative aliased select forms and compare row values, metadata,
  affected rows, warnings, and following `ROW_COUNT()`;
- cover schema-qualified aliases, reopen persistence, rename/drop behavior,
  independent handles, and diagnostics by sampling representative forms; and
- preserve existing parser, descriptor select, aggregate, storage, VFS, and
  workflow tests.
