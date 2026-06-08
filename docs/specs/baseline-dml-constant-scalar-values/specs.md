# Baseline DML Constant Scalar Values

This phase admits a narrow constant-scalar expression subset in
descriptor-backed `INSERT`, `REPLACE`, supported duplicate-key assignments, and
single-table `UPDATE` assignment values.

The target is compatibility for common application and MySQL test-suite shapes
such as:

```sql
INSERT INTO t VALUES (_utf8mb4'abc')
INSERT INTO t VALUES (_latin1 0x4142)
INSERT INTO t VALUES (CONVERT('abc' USING utf8mb4))
INSERT INTO t VALUES (CONCAT('a', REPEAT('b', 2)))
INSERT INTO t VALUES (STR_TO_DATE('2024-05-06', '%Y-%m-%d'))
UPDATE t SET c = SEC_TO_TIME(3661)
```

It does not introduce a general table-backed DML expression executor.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "INSERT Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
- MySQL 8.4 Reference Manual, "UPDATE Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- MySQL 8.4 Reference Manual, "Cast Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- MySQL 8.4 Reference Manual, "Character Set Introducers":
  <https://dev.mysql.com/doc/refman/8.4/en/charset-introducer.html>
- MySQL 8.4 Reference Manual, "Date and Time Functions":
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- MySQL 8.4 Reference Manual, "String Functions":
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

Runtime probes verify that MySQL evaluates scalar assignment expressions first
and then applies target-column storage conversion. For example,
`CONVERT(_ucs2 0x0041 USING utf8mb4)` assigned to `VARCHAR` stores `A`,
`CONVERT('42' USING utf8mb4)` assigned to `INT` stores `42`,
`STR_TO_DATE('2024-05-06 07:08:09','%Y-%m-%d %H:%i:%s')` assigned to
`DATETIME` stores `2024-05-06 07:08:09`, and `SEC_TO_TIME(3661)` assigned to
`TIME` stores `01:01:01`.

## Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: add a DML-only constant scalar value nonterminal. This reuses
  existing scalar expression AST nodes.
- Runtime: evaluate admitted constant scalar values through the existing
  no-source scalar evaluator, append its warnings, then route the resulting
  text or bytes through the normal descriptor conversion helpers.
- Catalog/storage/SQLite: no descriptor format, SQLite SQL, SQLite fork, VFS, or
  `.mylite` file-format changes.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
insert_value(A) ::= dml_constant_scalar_value(B).
update_value(A) ::= dml_constant_scalar_value(B).

dml_constant_scalar_value(A) ::= charset_introducer STRING(T).
dml_constant_scalar_value(A) ::= charset_introducer HEX_LITERAL(T).
dml_constant_scalar_value(A) ::= charset_introducer BIT_LITERAL(T).
dml_constant_scalar_value(A) ::= TEMPORAL_LITERAL_INTRODUCER STRING(T).
dml_constant_scalar_value(A) ::= cast_convert_expression(B).
dml_constant_scalar_value(A) ::= CONCAT(T) LPAREN function_argument_list(B) RPAREN(R).
dml_constant_scalar_value(A) ::= REPEAT(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
dml_constant_scalar_value(A) ::= STR_TO_DATE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
dml_constant_scalar_value(A) ::= SEC_TO_TIME(T) LPAREN expression(B) RPAREN(R).
```

The production is intentionally not `insert_value ::= expression` or
`update_value ::= expression`; the broader expression grammar has table-backed
and ambiguous forms that require a separate DML expression planning slice.

## Semantics

- Accepted DML scalar values are evaluated once per statement row-value
  expression, before descriptor storage conversion.
- `NULL` scalar results follow the current DML `NULL` storage rules.
- String-like scalar results enter normal `CHAR`, `VARCHAR`, `TEXT`, binary
  string, JSON, `ENUM`, `SET`, numeric, `YEAR`, `DATE`, `TIME`, `DATETIME`,
  `TIMESTAMP`, and `BIT` conversion where that target already supports
  compatible literal conversion.
- `INSERT IGNORE`, `REPLACE`, duplicate-key assignment, non-strict SQL mode, and
  `UPDATE IGNORE` continue to determine whether descriptor conversion errors
  are fatal or warning-adjusted.
- Scalar warnings, such as deprecation warnings from supported charset aliases,
  are appended before storage conversion warnings.

## Compatibility Limits

- No table-column references, subqueries, aggregates, windows, predicates,
  side-effecting expressions, user-variable assignment, or general expression
  assignments are admitted by this slice.
- Character-set introducers and `CONVERT(... USING charset)` reuse the existing
  MyLite scalar charset surface. MyLite does not yet implement a general
  character-set transcoder; unsupported or unknown character sets produce
  deterministic diagnostics.
- Spatial constructors such as `POINT()` and `ST_GeomFromText()` remain
  unsupported DML values.
- `DEFAULT(column_name)` remains limited to the existing descriptor-compatible
  baseline.

## Tests

Add MySQL-runtime expectations and focused parser/runtime tests for:

- charset introducer string and hex values in `INSERT`, `REPLACE`, duplicate-key
  update, and `UPDATE` assignment slots;
- `CONVERT(... USING utf8mb4)`, `CONVERT(... USING BINARY)`, `CONCAT()`,
  `REPEAT()`, `STR_TO_DATE()`, and `SEC_TO_TIME()` values;
- target conversion into nonbinary strings, binary strings, integers,
  `DATETIME`, and `TIME`;
- ordinary strict conversion diagnostics and `IGNORE`/non-strict warning
  adjustment still occurring after scalar evaluation;
- parser rejection of table-backed DML expressions outside this narrow subset.

Verification before marking done:

1. Focused parser/runtime CTest entries.
2. `packages/libmylite/tests/mysql_baseline_dml_constant_scalar_values_expectations.sh`
3. Parse-corpus benchmark comparison.
4. `git diff --check`
5. `cmake --workflow --preset check`
