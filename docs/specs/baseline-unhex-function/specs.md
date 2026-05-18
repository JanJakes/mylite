# Baseline UNHEX Function

## Summary

This phase adds a narrow MySQL-compatible `UNHEX(expr)` scalar function. The
supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`,
and single-table row-scalar `SELECT` projection contexts.

Core supported behavior:

- `UNHEX(str)` interprets ASCII hexadecimal digits and returns binary bytes;
- odd-length inputs behave as though a leading zero nibble exists, so
  `UNHEX('F')` returns `X'0F'`;
- empty input returns an empty non-`NULL` binary string;
- `NULL` input returns `NULL` and does not warn;
- non-hex input returns `NULL` and records MySQL warning `1411 / HY000`;
- integer and boolean inputs use their visible decimal text before decoding;
- negative integer inputs are invalid and return `NULL` with warning `1411`;
- table-backed row-scalar projection is descriptor-driven and uses a private
  MyLite SQLite scalar helper for row values.

This phase intentionally does not add `UNHEX()` in predicates, DML assignments,
ordering/grouping expressions, generated columns, defaults, parameters,
subqueries, arbitrary expression trees, or complete binary-string metadata.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Related baseline expression slices:
  - `docs/specs/baseline-hex-function/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-convert-syntax/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_unhex_function_expectations.sh` and
  verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `UNHEX()` requires exactly one argument; zero or two arguments fail with
  `1582 / 42000` and native-function parameter-count text;
- `HEX(UNHEX('4D7953514C')) = '4D7953514C'`;
- `HEX(UNHEX('1267')) = '1267'`, `HEX(UNHEX('F')) = '0F'`,
  `HEX(UNHEX('ABC')) = '0ABC'`, and `HEX(UNHEX('')) = ''`;
- uppercase and lowercase ASCII hex digits are accepted;
- any non-hex byte in the argument returns `NULL` and appends warning
  `1411 / HY000`;
- `UNHEX(NULL)` returns `NULL` and appends no warning;
- `UNHEX('GG')`, `UNHEX('41G')`, `UNHEX(' 41')`, `UNHEX('41 ')`, and
  `UNHEX('é')` return `NULL` with warning `1411`;
- `HEX(UNHEX(1267)) = '1267'`, `HEX(UNHEX(255)) = '0255'`,
  `HEX(UNHEX(TRUE)) = '01'`, and `HEX(UNHEX(FALSE)) = '00'`;
- `UNHEX(-15)` and `UNHEX(1.5)` return `NULL` with warning `1411`;
- warnings produced by `UNHEX()` are not visible through `@@warning_count`
  within the same scalar select list; `@@warning_count` reports the previous
  statement diagnostics and the `UNHEX()` warning becomes visible after the
  statement completes;
- `CHAR` values have trailing pad spaces stripped before `UNHEX()` sees them,
  while `BINARY(N)` values retain trailing `0x00` bytes and therefore often
  return `NULL` with warning `1411`.

MySQL also accepts broader behavior such as decimal/float coercion, expression
arguments, use in predicates and DML assignments, collations, character-set
metadata, and full binary-string metadata. Those forms remain outside this
baseline.

## Ownership Boundaries

- Public API: unchanged. Result bytes are exposed through the existing public
  byte-safe result accessors. `mylite_result_value_text()` remains available
  for compatibility but binary callers must use `mylite_result_value_bytes()`
  and `mylite_result_value_size()` when embedded NUL bytes are possible.
- Statement context: owns diagnostics, warning counts, affected-row state, and
  result finalization. Scalar `UNHEX()` invalid-input warnings are staged until
  after the scalar result row is materialized, preserving MySQL's
  same-select-list `@@warning_count` behavior.
- Lexer/parser/AST: admits exact one-argument `UNHEX()` expressions and
  wrong-arity AST nodes while preserving source spans for labels and
  diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: row-backed projection lowers to generated SQLite SQL that
  calls a private MyLite scalar helper over quoted descriptor expressions and
  bound parameters. The helper uses SQLite's public scalar-function API; no
  SQLite fork patch is required.
- Result builder: uses byte rows for binary scalar values and existing row
  result conventions for labels and aliases.
- Storage/VFS/file format: row reads only. The `.mylite` preamble and shifted
  SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT unhex_item[, unhex_item ...]
SELECT unhex_item[, unhex_item ...] FROM DUAL
```

`DO` form:

```sql
DO unhex_expr[, unhex_expr ...]
```

Single-table row-backed forms, with at least one select item containing
`UNHEX()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
unhex_expr:
    UNHEX ( unhex_value )

unhex_value:
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
  | ( unhex_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for this phase are:

- integer-family columns stored in the current signed 64-bit physical range;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `BINARY`, `VARBINARY`, and baseline `BLOB` family.

Exact `DECIMAL`, approximate numeric values, `BIT`, `YEAR`, temporal values,
`ENUM`, `SET`, `JSON`, and spatial values are deferred for row-backed
`UNHEX()`.

The following remain outside this phase:

- `WHERE UNHEX(column) ...`, `HAVING UNHEX(...) ...`, expression `ORDER BY`,
  grouping, aggregate arguments, and distinct expression rows;
- DML assignment values such as `UPDATE t SET c = UNHEX(v)`;
- nested row functions such as `UNHEX(CONCAT(v, '0'))`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- complete binary result metadata, collations, character-set conversion
  metadata, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= UNHEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UNHEX_FUNCTION, B, R);
}
expression(A) ::= UNHEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= UNHEX(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR, C, R);
}
```

`UNHEX` is a nonreserved function keyword and remains available as an
identifier where MyLite's current identifier productions admit function
keywords.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the argument to input bytes:
   - ordinary string literal: decoded bytes, including embedded NUL bytes;
   - binary hex literal: decoded bytes;
   - integer literal and `TRUE` / `FALSE`: decimal text;
   - `NULL`: SQL `NULL`;
   - supported scalar/session/system values and supported binary
     cast/convert values: their existing visible bytes or SQL `NULL`.
3. Return SQL `NULL` if the argument is `NULL`.
4. Validate that every input byte is an ASCII hex digit.
5. If invalid, return SQL `NULL` and stage one warning
   `1411 / HY000 / Incorrect string value: '<value>' for function unhex`.
6. Decode two nibbles per output byte. Odd input length decodes the first
   character as the low nibble of the first byte.
7. Return binary bytes. Empty input returns a non-`NULL` zero-length byte
   value.

Table-backed row-scalar projection lowers descriptor-backed values:

- descriptor columns and scalar parameters are passed to `_mylite_unhex()`;
- `_mylite_unhex()` treats SQLite `NULL` as SQL `NULL`;
- SQLite integer values are converted to decimal text before decoding;
- SQLite text/blob values are decoded by raw bytes;
- invalid input returns SQLite `NULL` and appends warning `1411 / HY000` to the
  MyLite connection diagnostics.

Generated SQL quotes every descriptor identifier and binds scalar parameters
through prepared statements. No generated SQL interpolates user literals.

## Diagnostics

Supported successful calls return through existing `SELECT`/`DO` result
conventions.

Diagnostics:

- wrong arity: `1582 / 42000 / Incorrect parameter count in the call to native
  function 'UNHEX'`;
- unsupported scalar expression: MyLite-specific unsupported-feature error;
- unsupported row-backed descriptor family: MyLite-specific unsupported-feature
  error;
- unknown descriptor column: existing descriptor column-resolution diagnostic;
- invalid non-`NULL` hex input: warning `1411 / HY000` and SQL `NULL`;
- allocation failure: existing `MYLITE_NOMEM` handling;
- physical SQLite callback misuse/failure: existing physical SQLite error
  translation.

## Compatibility Impact

`COMPATIBILITY.md` and `docs/compatibility/functions-string.md` mark `UNHEX()`
as partially supported. The partial status is deliberate: this phase implements
binary byte decoding in a small expression envelope without overclaiming full
expression semantics, binary metadata, collation behavior, DML assignment
support, or general expression planning.
