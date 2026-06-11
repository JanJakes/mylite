# Parser Corpus Nonreserved Identifier Residuals

## Status

This parser-corpus slice admits MySQL 8.4.9 nonreserved keyword identifiers
that appear in ordinary table, column, projection, DML, and table-maintenance
positions.

Primary behavior was verified against the local MySQL 8.4.9 runtime. The
relevant official MySQL reference sections are:

- https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html
- https://dev.mysql.com/doc/refman/8.4/en/optimize-table.html

This specification is independently authored from MyLite project documents,
official MySQL 8.4 documentation, and observed MySQL 8.4.9 behavior.

## Scope

MyLite accepts these corpus residuals as normal executable SQL:

```sql
CREATE TABLE t1 (current INT, diagnostics INT, number INT, returned_sqlstate INT);
INSERT INTO t1 (current, diagnostics, number, returned_sqlstate) VALUES (1,2,3,4);
SELECT current, diagnostics, number, returned_sqlstate FROM t1 WHERE number = 3;

CREATE TABLE t0 (skip INT, locked INT, nowait INT);

CREATE TABLE diag_non_reserved (
    diagnostics INT,
    current INT,
    stacked INT,
    exception INT
);

CREATE TABLE SESSION_USER(a INT);
CREATE TABLE SYSTEM_USER(a INT);

OPTIMIZE TABLES columns_priv, db, user;
```

MySQL 8.4.9 reports `CURRENT`, `DIAGNOSTICS`, `NUMBER`,
`RETURNED_SQLSTATE`, `STACKED`, `USER`, `SKIP`, `LOCKED`, and `NOWAIT` as
nonreserved in `INFORMATION_SCHEMA.KEYWORDS`. MyLite should therefore admit
the same names where the grammar expects an identifier.

`SESSION_USER()` and `SYSTEM_USER()` remain no-whitespace built-in function
calls in expression positions. In `CREATE TABLE SESSION_USER(a INT)`, however,
the parser is not resolving an expression function call; MySQL accepts the name
as a table identifier. MyLite keeps the existing no-space function-name guard
for function names such as `DATE_ADD` and `COUNT`, but excludes
`SESSION_USER` and `SYSTEM_USER` from that table-name guard.

`ANALYZE TABLES` and `OPTIMIZE TABLES` plural spelling is still handled by the
existing parser retry path. That retry path must treat `USER` and the newly
admitted nonreserved keyword tokens as identifier-like table-name components.

## Grammar Notes

The normal MyLite Lemon identifier production is extended with:

```lemon
identifier ::= CURRENT.
identifier ::= SKIP.
identifier ::= LOCKED.
identifier ::= NOWAIT.
```

Other names in this slice are already ordinary identifiers or already admitted
nonreserved keywords.

The plural table-maintenance retry parser has equivalent token classification
for identifier-like keyword table names.

## Runtime Semantics

These forms use existing runtime behavior:

- DDL creates ordinary table and column descriptors.
- DML and `SELECT` resolve the descriptors through existing name resolution.
- `OPTIMIZE TABLES` returns the existing MyLite table-maintenance result rows,
  matching the MySQL-shaped note/status row pattern already used for
  `OPTIMIZE TABLE`.
- No new data type, privilege, stored-function, or table-maintenance side
  effects are introduced.

## Tests

Coverage includes:

- MySQL 8.4.9 expectation script for the accepted identifier positions;
- parser tests for all residual SQL shapes;
- runtime tests for DDL, DML, projection, table-name function aliases, and
  plural `OPTIMIZE TABLES` with `user`;
- parser-corpus benchmark verification.

## Non-Goals

- No broad reserved-word overhaul beyond the verified residual tokens.
- No `ANSI_QUOTES` double-quoted identifier support in this slice.
- No implementation of stored diagnostics, `SIGNAL`, `RESIGNAL`, stored
  routines, or condition-area semantics.
- No legacy removed MySQL syntax such as `SHOW MASTER STATUS` or
  `SHOW SLAVE STATUS`.
