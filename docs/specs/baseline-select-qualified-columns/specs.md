# Baseline SELECT Qualified Columns

## Summary

This feature adds descriptor-backed qualified column references to the
single-table `SELECT` forms MyLite already supports. It builds directly on the
baseline table-alias slice.

The supported surface is:

```sql
SELECT [qualifier.]column_name FROM table_name [AS] alias ...
SELECT DISTINCT [qualifier.]column_name FROM table_name [AS] alias ...
SELECT COUNT([qualifier.]column_name) FROM table_name [AS] alias ...
SELECT COUNT(DISTINCT [qualifier.]column_name) FROM table_name [AS] alias ...
SELECT MIN([qualifier.]column_name) FROM table_name [AS] alias ...
SELECT MAX([qualifier.]column_name) FROM table_name [AS] alias ...
... WHERE [qualifier.]column_name comparison_or_null_predicate
... ORDER BY [qualifier.]column_name [ASC | DESC]
```

When the table source has an alias, `qualifier` must be that alias. When the
table source has no alias, `qualifier` may be the resolved table name, or the
resolved schema name followed by the table name. Unqualified descriptor column
references remain supported.

## Sources And Runtime Evidence

Normative source:

- MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>

The MySQL documentation states that columns may be referenced as `col_name`,
`tbl_name.col_name`, or `db_name.tbl_name.col_name`, and that table references
may be aliased with `tbl_name AS alias` or `tbl_name alias`. This slice admits
only qualified references against the single selected descriptor-backed table.

Observed MySQL 8.4.9 behavior, using local Docker runtime
`mylite-mysql-849`:

```sql
SELECT VERSION();
-- 8.4.9

CREATE DATABASE mylite_qualified_probe;
USE mylite_qualified_probe;
CREATE TABLE t(id INT NOT NULL, n INT NULL, nn INT NOT NULL);
INSERT INTO t VALUES (1,10,5),(2,NULL,6),(3,10,7);

DO 0;
SELECT t.n FROM t ORDER BY t.id;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT mylite_qualified_probe.t.n
FROM mylite_qualified_probe.t
ORDER BY mylite_qualified_probe.t.id;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT a.n
FROM t AS a
WHERE a.n IS NOT NULL
ORDER BY a.id DESC
LIMIT 1;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT DISTINCT a.n FROM t AS a ORDER BY a.n;
SELECT @@warning_count, ROW_COUNT();

DO 0;
SELECT COUNT(a.n), COUNT(DISTINCT a.n), MIN(a.n), MAX(a.n)
FROM t AS a;
SELECT @@warning_count, ROW_COUNT();
```

Observed results:

- Qualified selected column labels are the unqualified column names.
- `table.column` and `schema.table.column` work when no alias is supplied.
- `alias.column` works when an alias is supplied.
- Successful row-returning statements leave `@@warning_count = 0` and
  following `ROW_COUNT() = -1`.
- After a table is aliased, the original table name and
  `schema.table.column` no longer resolve. MySQL reports
  `1054 (42S22) Unknown column '<qualified-name>' in '<clause>'`.
- Unknown qualifiers and unknown qualified column names also report
  `1054 (42S22)` in the field, where, or order clause.
- MySQL accepts qualified wildcards such as `alias.*`. This slice deferred
  wildcard expansion; the later
  [baseline qualified wildcard SELECT](../baseline-qualified-wildcard-select/specs.md)
  slice admits limited descriptor-backed `table.*`, `schema.table.*`, and
  `alias.*` projection.

## Scope

In scope:

- unqualified column references already supported by prior slices;
- two-part `qualifier.column` references for selected columns, `DISTINCT`,
  predicates, ordering, `COUNT(column)`, `COUNT(DISTINCT column)`, `MIN()`, and
  `MAX()`;
- three-part `schema.table.column` references when the table source has no
  alias and the schema/table match the resolved single source table;
- optional table aliases from the previous slice;
- existing descriptor-backed `WHERE`, `ORDER BY`, `LIMIT`, `ALL`,
  `DISTINCTROW`, `SELECT *`, aggregate, schema-resolution, result, warning, and
  `ROW_COUNT()` behavior.

Out of scope:

- qualified wildcard forms such as `table.*` and `alias.*`, which are handled
  by the later baseline qualified wildcard slice;
- select-item aliases and `ORDER BY` select-item alias resolution;
- joins, multiple source tables, derived tables, CTEs, subqueries, grouping,
  having, windows, set operations, locking clauses, index hints, partitions,
  or arbitrary expression evaluation;
- qualified references in `DELETE`, `UPDATE`, `INSERT`, DDL, or SHOW
  statements;
- collations, privilege semantics, protocol-grade metadata, and optimizer
  behavior.

## Architecture

The public API and statement context are unchanged.

The parser already represents dotted names as nested
`MYLITE_SQL_AST_QUALIFIED_IDENTIFIER` nodes. This slice extends aggregate
argument grammar where needed so existing qualified identifier ASTs can reach
the analyzer.

The analyzer/runtime resolves each admitted column reference against the single
descriptor-backed table source:

- the table source is still resolved through MyLite's selected/default schema
  policy and catalog descriptors;
- the optional table alias is read from the second `FROM_TABLE` child;
- a one-part reference resolves as today's unqualified descriptor column;
- a two-part reference resolves only when the qualifier matches the alias, or
  when no alias exists and the qualifier matches the resolved table name;
- a three-part reference resolves only when no alias exists and the first two
  parts match the resolved schema and table names;
- names are compared using the current descriptor catalog name matching policy;
- mismatched qualifiers and missing descriptor columns produce deterministic
  unknown-column diagnostics in the active clause context;
- generated SQLite SQL remains built from stable physical table and column
  names, not from user-supplied qualifiers;
- every generated SQLite identifier remains quoted and predicate/limit values
  remain bound parameters; and
- no catalog rows, descriptor generations, `sqlite_schema_generation`, storage
  format, VFS behavior, or SQLite fork code change.

This feature adds no result-set materialization beyond the existing public
`mylite_result` object. SQLite still performs row scans, filtering, sorting,
limiting, duplicate removal, and aggregate computation.

## Syntax

MyLite grammar snippets, independently authored for this slice:

```lemon
qualified_identifier ::= identifier.
qualified_identifier ::= qualified_identifier DOT identifier.

select_item ::= qualified_identifier.
predicate_atom ::= qualified_identifier comparison_operator predicate_value.
predicate_atom ::= qualified_identifier IS NULL.
predicate_atom ::= qualified_identifier IS NOT NULL.
order_clause_opt ::= ORDER BY qualified_identifier order_direction_opt.

expression ::= COUNT LPAREN qualified_identifier RPAREN.
expression ::= COUNT LPAREN DISTINCT qualified_identifier RPAREN.
expression ::= MIN LPAREN qualified_identifier RPAREN.
expression ::= MAX LPAREN qualified_identifier RPAREN.
```

The parser may admit qualified identifiers in more expression positions than
this runtime slice supports. Unsupported shapes continue to fail in the
analyzer with deterministic diagnostics.

## Semantics

For supported statements, a qualified reference identifies the same descriptor
column that the equivalent unqualified reference identifies.

- Result values, ordering, distinctness, aggregate results, `LIMIT`, and
  `OFFSET` behavior are unchanged.
- Result column labels remain the unqualified descriptor column or aggregate
  expression label used by the current result builder. For simple qualified
  selected columns, the visible column name is the descriptor column name.
- Explicit qualified references may name invisible descriptor columns in the
  same positions where unqualified explicit references may name them.
- `SELECT DISTINCT` still admits exactly one descriptor column, and if `ORDER
  BY` is present the order column must resolve to that same descriptor column.
- Successful row-returning statements return result rows, `affected_rows == 0`,
  `warning_count == 0`, and following `ROW_COUNT() == -1`.

## Diagnostics

Expected diagnostics for this slice:

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted grammar | existing parse error `1064` / `42000` |
| Missing default schema for unqualified source table | existing `1046` / `3D000` |
| Unknown schema or unknown source table | existing table-resolution diagnostics |
| Reserved source schema/table name | existing reserved-name diagnostics |
| Unknown selected column or wrong selected-column qualifier | `1054` / `42S22`, unknown column in field list |
| Unknown predicate column or wrong predicate qualifier | `1054` / `42S22`, unknown column in where clause |
| Unknown order column or wrong order qualifier | `1054` / `42S22`, unknown column in order clause |
| Qualified aggregate argument mismatch | `1054` / `42S22`, unknown column in field list |
| Qualified wildcard | later limited qualified wildcard slice expands visible descriptor columns |
| Unsupported projection expression or select-item alias | existing parse or unsupported diagnostic |
| Physical SQLite failure | existing internal SQLite row-operation diagnostic |
| Allocation failure | existing allocation diagnostic |

For wrong qualifiers, MyLite should preserve the user-visible qualified token
text in the diagnostic when practical, for example `Unknown column 'a.missing'
in 'field list'`.

## Tests

Add MySQL-runtime-verified expectations for:

- unaliased `table.column` selected columns, predicates, and order keys;
- unaliased `schema.table.column` forms;
- aliased `alias.column` selected columns, predicates, order keys, `DISTINCT`,
  `DISTINCTROW`, `COUNT(column)`, `COUNT(DISTINCT column)`, `MIN()`, and
  `MAX()`;
- result labels, warning count, and following `ROW_COUNT()`;
- original table or schema qualifiers after aliasing;
- unknown qualifiers and unknown qualified columns in field, where, and order
  clauses; and
- qualified wildcard behavior documented as deferred to the later baseline
  qualified wildcard slice.

Add fast C coverage by extending existing parser and runtime lifecycle tests:

- parse qualified aggregate arguments and representative qualified selected,
  predicate, and order references;
- execute qualified selected columns, `WHERE`, `ORDER BY`, `LIMIT`,
  `DISTINCT`, `DISTINCTROW`, and aggregate forms;
- cover alias and no-alias qualifier rules, schema-qualified sources, invisible
  explicit columns, reopen persistence, rename/drop, independent handles,
  public result conventions, and file-format preservation through
  representative statements; and
- preserve existing parser, descriptor select, aggregate, storage, VFS, and
  workflow tests.
