# Baseline HEX Function

## Summary

This phase adds a narrow MySQL-compatible `HEX(expr)` scalar string function.
The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts.

Core supported behavior:

- `HEX(str)` returns uppercase hexadecimal text for each byte in `str`;
- multibyte UTF-8 text is encoded by bytes, not characters;
- ordinary string literals and binary hex literals may contain embedded NUL
  bytes;
- integer, signed integer, boolean, and `NULL` arguments follow MySQL's numeric
  `BIGINT`/`longlong` form, with negative values rendered as unsigned 64-bit
  two's-complement hexadecimal text;
- supported `CAST(value AS BINARY)`, `CONVERT(value, BINARY)`, and
  `CONVERT(value USING charset)` scalar values are hexed from their visible
  bytes;
- successful supported calls produce no warnings.

`UNHEX()` remains outside this phase because it returns binary strings and needs
separate result-metadata and embedded-NUL coverage.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-left-right-functions/specs.md`
  - `docs/specs/baseline-convert-syntax/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_hex_function_expectations.sh` and
  verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `HEX()` requires exactly one argument; zero or two arguments fail with
  `1582 / 42000` and native-function parameter-count text;
- `HEX('abc') = '616263'`, `HEX('é') = 'C3A9'`, and `HEX('') = ''`;
- ordinary string escape decoding happens before byte hexing, so
  `HEX('a\\0b') = '610062'`;
- `HEX(X'0061') = '0061'` and `HEX(0x6162) = '6162'`;
- `HEX(CAST('ABC' AS BINARY)) = '414243'`;
- `HEX(255) = 'FF'`, `HEX(-1) = 'FFFFFFFFFFFFFFFF'`,
  `HEX(TRUE) = '1'`, and `HEX(FALSE) = '0'`;
- `HEX(NULL)` returns `NULL`;
- `CHAR` trailing spaces are stripped in the current default SQL mode before
  `HEX()` sees the value;
- `BINARY(N)` values are padded with `0x00` and `HEX()` includes those bytes;
- successful supported scalar and table-backed calls produce
  `@@warning_count = 0`; a preceding `DO` followed by `ROW_COUNT()` reports
  `0`, while scalar `SELECT` makes `ROW_COUNT()` report `-1`.

MySQL also accepts deferred behavior such as decimal and approximate numeric
rounding, `BIT` display, `HEX()` in predicates, DML assignments, ordering,
grouping, generated columns, defaults, and nested row expressions. Those forms
remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Non-`NULL` values are exposed through the existing
  public result object as text. `NULL` returns a SQL `NULL` cell.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits exact one-argument `HEX()` expressions and
  wrong-arity AST nodes while preserving source spans for labels and
  diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite uses SQLite's public `hex()` / `printf()` expression execution
  for row-backed values. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT hex_item[, hex_item ...]
SELECT hex_item[, hex_item ...] FROM DUAL
```

`DO` form:

```sql
DO hex_expr[, hex_expr ...]
```

Single-table row-backed forms, with at least one select item containing
`HEX()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
hex_expr:
    HEX ( hex_value )

hex_value:
    string_literal
  | hex_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | binary_cast_or_convert
  | descriptor_column_reference        -- table-backed SELECT only
  | ( hex_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for this phase are:

- integer-family columns stored in the current signed 64-bit physical range;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `BINARY`, `VARBINARY`, and baseline `BLOB` family.

Exact `DECIMAL`, approximate numeric values, `BIT`, `YEAR`, temporal values,
`ENUM`, `SET`, `JSON`, and spatial values are deferred for row-backed `HEX()`.

The following remain outside this phase:

- `WHERE HEX(column) ...`, `HAVING HEX(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = HEX(v)`;
- nested row functions such as `HEX(CONCAT(v, '-'))`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- `UNHEX()`, binary result metadata, collations, character-set conversion
  metadata, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= HEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_HEX_FUNCTION, B, R);
}
expression(A) ::= HEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= HEX(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR, C, R);
}
```

`HEX` is a nonreserved function keyword and remains available as an identifier
where MyLite's current identifier productions admit function keywords.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the argument:
   - ordinary string literal: decoded bytes, including embedded NUL bytes;
   - binary hex literal: decoded bytes;
   - integer literal: signed 64-bit value formatted as unsigned 64-bit
     hexadecimal text;
   - `TRUE` / `FALSE`: numeric `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value, system variable, `CAST(... AS BINARY)`,
     or `CONVERT(...)`: existing visible byte string or SQL `NULL`.
3. Return SQL `NULL` if the argument is `NULL`.
4. Return uppercase hexadecimal text. Empty byte strings return the empty
   string.

Table-backed row-scalar projection lowers descriptor-backed values:

- string and binary string descriptors use SQLite `hex()` over the descriptor
  value expression;
- integer-family descriptors use a `CASE` expression to preserve `NULL`, then
  SQLite `printf('%llX', ...)` for non-`NULL` physical integer values;
- scalar literal/session arguments in row-scalar `SELECT` are evaluated once by
  the existing planner and bound as text/`NULL` parameters.

Generated SQL quotes every descriptor identifier and binds scalar parameters
through prepared statements. No generated SQL interpolates user literals.

## Diagnostics

Supported successful calls return through existing `SELECT`/`DO` result
conventions and set `warning_count == 0`.

Diagnostics:

- wrong `HEX()` arity: MySQL-compatible native-function parameter-count error
  `1582 / 42000`;
- unknown no-source identifier: existing unknown-column diagnostic;
- unknown row-backed descriptor column: existing unknown-column diagnostic;
- unsupported scalar argument: deterministic MyLite unsupported diagnostic;
- unsupported row-backed descriptor family: deterministic MyLite unsupported
  diagnostic naming the supported row-backed column families;
- allocation failure: existing `MYLITE_NOMEM` path;
- physical SQLite failure: existing runtime error wrapping.

No public API surface changes are introduced.

## Tests

Coverage for this phase:

- MySQL-runtime expectation script for scalar, `DO`, row-backed, label, arity,
  and deferred-shape behavior;
- parser tests for `HEX()` nodes, source spans, identifiers, `DO`, and
  wrong-arity AST nodes;
- C runtime tests for no-source/`DUAL`/`DO` values, row counts, warnings,
  table-backed integer/text/binary/blob columns, labels, row envelope reuse,
  reopen persistence, `.mylite` preamble preservation, and diagnostics;
- existing parser/runtime/check workflows remain required.
