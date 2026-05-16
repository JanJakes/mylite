# Baseline CHARSET and COLLATION Functions

## Summary

This phase adds a narrow MySQL-compatible `CHARSET(expr)` and
`COLLATION(expr)` scalar surface. The supported slice covers no-source scalar
`SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table row-scalar `SELECT`
projection contexts.

The feature is metadata-oriented. For descriptor-backed columns, MyLite returns
the character set and collation recorded in MyLite catalog descriptors. For
literal and admitted scalar arguments, MyLite returns the connection defaults or
`binary` according to the selected argument class. The phase does not add full
collation coercibility, charset conversion, expression result metadata,
collation-aware comparison, or SQLite fork changes.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing charset/collation and row-scalar slices:
  - `docs/specs/baseline-table-charset-collation-surface/specs.md`
  - `docs/specs/baseline-column-charset-collation-attributes/specs.md`
  - `docs/specs/baseline-result-column-metadata/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-hex-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
  - character set and collation of function results:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html>
  - cast functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_charset_collation_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `CHARSET()` and `COLLATION()` require exactly one argument. Zero or multiple
  arguments are syntax errors in MySQL for the probed statement shapes.
- With `SET NAMES utf8mb4`, ordinary string literals report `utf8mb4` and
  `utf8mb4_0900_ai_ci`.
- `CAST('ABC' AS BINARY)`, `CONVERT('ABC', BINARY)`, and
  `CONVERT('ABC' USING BINARY)` report `binary` / `binary`.
- `NULL`, integer, and `RAND()` scalar arguments report `binary` / `binary`.
- `CONCAT('a','b')` reports the connection character set and collation.
  `CONVERT('ABC' USING utf8mb4)` reports `utf8mb4` and the target character
  set default collation `utf8mb4_0900_ai_ci`, even when
  `@@collation_connection` differs. MySQL also reports connection metadata for
  `CAST('ABC' AS CHAR)`, but that grammar is deferred in MyLite.
- `DATABASE()` reports `utf8mb3` / `utf8mb3_general_ci`, even when the
  selected schema value is `NULL`.
- Descriptor-backed `CHAR`, `VARCHAR`, `TEXT`, `ENUM`, and `SET` columns report
  the effective column/table `utf8mb4` charset and collation, including for
  `NULL` row values.
- Descriptor-backed `VARBINARY`, `BLOB`, integer, and `DATE` columns report
  `binary` / `binary`, including for `NULL` row values.
- Successful supported calls produce `@@warning_count = 0`. Scalar `SELECT`
  makes `ROW_COUNT()` report `-1`; `DO` makes it report `0`.

MySQL also supports many deferred cases: explicit string introducers,
`COLLATE`, non-`utf8mb4` charsets, complete collation aggregation for nested
expressions, user variables, stored functions, prepared parameters, generated
columns, predicates, grouping, ordering, DML assignments, views, and protocol
metadata. Those are outside this baseline.

## Ownership Boundaries

- Public API: unchanged. The existing public result object exposes the returned
  character set or collation as text. Successful `DO` returns through existing
  non-row result conventions.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits exact one-argument `CHARSET()` and `COLLATION()`
  expressions while preserving source spans for labels and diagnostics.
- Analyzer/planner: resolves descriptor-backed row-scalar arguments from
  MyLite catalog descriptors. Unsupported expression shapes are rejected before
  generated SQLite SQL is built.
- Catalog: read-only for table and column descriptor metadata. This feature
  does not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- SQLite execution: table-backed projections lower descriptor-derived metadata
  constants to bound SQLite parameters over stable physical table names. MyLite
  does not inspect SQLite schema text or rely on SQLite collation metadata.
- Result builder: uses existing row result conventions and source-span labels
  or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT charset_collation_item[, charset_collation_item ...]
SELECT charset_collation_item[, charset_collation_item ...] FROM DUAL
```

`DO` form:

```sql
DO charset_collation_expr[, charset_collation_expr ...]
```

Single-table row-backed forms, with at least one selected expression containing
`CHARSET()` or `COLLATION()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
charset_collation_expr:
    CHARSET ( charset_collation_value )
  | COLLATION ( charset_collation_value )

charset_collation_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | RAND()
  | RAND(seed_literal)
  | DATABASE()
  | SCHEMA()
  | CONCAT(literal_or_session_scalar[, ...])
  | CAST(value AS BINARY)
  | CONVERT(value, BINARY)
  | CONVERT(value USING BINARY)
  | CONVERT(value USING utf8mb4)
  | descriptor_column_reference        -- table-backed SELECT only
  | ( charset_collation_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for this phase are:

- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `ENUM` and `SET`;
- `BINARY`, `VARBINARY`, and baseline `BLOB` family;
- integer family, exact `DECIMAL`, approximate numeric, `BIT`, `YEAR`, `DATE`,
  `TIME`, `DATETIME`, `TIMESTAMP`, and `JSON`.

For row-backed descriptor columns:

- nonbinary string, `ENUM`, and `SET` descriptors return the effective
  character set and collation from `column_effective_character_set_name()` and
  `column_effective_collation_name()`;
- national `CHAR` / `VARCHAR` descriptors return `utf8mb3` and
  `utf8mb3_general_ci`;
- all other admitted descriptor families return `binary` and `binary`;
- the result depends on descriptor type metadata, not the current row value, so
  a `NULL` value in a string column still returns the column charset/collation.

The following remain outside this phase:

- explicit character-set introducers such as `_utf8mb4 'x'`;
- explicit `COLLATE` expressions and full collation aggregation/coercibility;
- non-`utf8mb4` conversion, `CONVERT(... USING charset)` beyond `BINARY` and
  `utf8mb4`, `CAST(... AS CHAR)`, and
  `CAST(... AS CHAR CHARACTER SET ...)`;
- row-backed nested functions other than descriptor columns and constants
  listed above;
- `WHERE CHARSET(column) ...`, expression `ORDER BY`, grouping, distinct
  expression rows, aggregate arguments, DML assignment values, generated
  columns, defaults, views, parameters, user variables, stored functions,
  subqueries beyond existing no-source scalar subquery limits, and protocol
  metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CHARSET(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHARSET_FUNCTION, B, R);
}
expression(A) ::= COLLATION(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_COLLATION_FUNCTION, B, R);
}
```

No zero-argument or multi-argument productions are added for this phase, so
those forms remain syntax errors as observed in MySQL 8.4.9.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Classify the argument expression:
   - ordinary string literal: session `character_set_connection` and
     `collation_connection`;
   - `CAST(... AS BINARY)`, `CONVERT(..., BINARY)`, and
     `CONVERT(... USING BINARY)`: `binary` / `binary`;
   - integer, signed integer, boolean, `NULL`, `RAND()`, and `RAND(seed)`:
     `binary` / `binary`;
   - `DATABASE()` and `SCHEMA()`: `utf8mb3` / `utf8mb3_general_ci`;
   - supported `CONCAT(...)`: session `character_set_connection` and
     `collation_connection`;
   - `CONVERT(... USING utf8mb4)`: the target `utf8mb4` character set and
     MyLite's admitted `utf8mb4` default collation `utf8mb4_0900_ai_ci`.
3. Return the character set name for `CHARSET()` and the collation name for
   `COLLATION()`.

For row-backed descriptor columns, planning derives a constant text value from
the column descriptor and table default descriptor. The generated SQLite
projection binds that value as a parameter for every matched row. This preserves
the existing row envelope for `WHERE`, `ORDER BY`, and `LIMIT` without
materializing full result sets in MyLite memory.

## Diagnostics

- Syntax errors: zero-argument, multi-argument, malformed parentheses, and
  grammar not admitted by this phase use existing parser diagnostics.
- Unknown descriptor columns use existing MySQL-shaped unknown-column
  diagnostics.
- Unsupported expression shapes fail deterministically with MyLite unsupported
  diagnostics naming the limited `CHARSET()` / `COLLATION()` surface.
- Allocation failures use existing `MYLITE_NOMEM` diagnostics.
- Physical SQLite failures are reported through existing physical row-read
  diagnostics.

Successful supported calls set `warning_count == 0`.
