# Baseline Base64 Functions

## Goal

Add a narrow, MySQL-runtime-verified `TO_BASE64()` and `FROM_BASE64()` slice for
scalar and single-table row-scalar projection:

```sql
SELECT TO_BASE64(value), FROM_BASE64(value)
SELECT TO_BASE64(value), FROM_BASE64(value) FROM table_name
DO TO_BASE64(value), FROM_BASE64(value)
```

The feature covers common application use of Base64 text and binary payloads
without introducing a general expression engine, protocol binary metadata
overhaul, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 Reference Manual, string functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Official MySQL 8.4 Reference Manual, expression syntax and function-name
  parsing:
  <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_base64_functions_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the baseline expectations:

- `TO_BASE64(str)` returns the Base64 text for the argument after MySQL string
  conversion and returns `NULL` for `NULL`.
- `TO_BASE64()` uses the standard MySQL alphabet where values 62 and 63 are
  `+` and `/`, output is grouped in 4-character blocks, and incomplete final
  blocks are padded with `=`.
- Long `TO_BASE64()` output inserts `\n` after each 76 encoded characters when
  more output follows. Exactly 76 encoded characters have no trailing newline.
- `FROM_BASE64(str)` decodes the matching format and returns a binary string.
  It returns `NULL` for `NULL` and invalid Base64 input.
- `FROM_BASE64()` ignores ASCII space, tab, carriage return, and newline before
  validating the Base64 payload.
- Invalid `FROM_BASE64()` input returns `NULL` with warning count `0` for the
  verified forms.
- Wrong arity for either function raises MySQL error `1582` / SQLSTATE `42000`.
- MySQL accepts whitespace between the function name and `(` for these names in
  the default SQL mode.
- Successful `SELECT` returns one row and makes a following `ROW_COUNT()` return
  `-1`; successful `DO` returns no rows and makes `ROW_COUNT()` return `0`.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope: one
  descriptor-backed table source, optional existing `WHERE`, ordering, and
  limit support;
- `TO_BASE64(value)` and `FROM_BASE64(value)` with exactly one argument;
- scalar arguments from ordinary string literals, hexadecimal literals,
  integer literals with optional unary sign, `TRUE`, `FALSE`, `NULL`,
  supported session scalar values, supported system variables, and existing
  binary cast/convert forms where the current scalar runtime already exposes
  byte-safe values;
- descriptor-backed row arguments from integer-family, nonbinary string,
  baseline `TEXT`, binary string, and baseline BLOB-family columns;
- `TO_BASE64()` result text through existing public result APIs;
- `FROM_BASE64()` result bytes through existing public text and byte-safe result
  APIs; embedded `NUL` bytes are preserved in the value size;
- warning count `0` for supported in-range forms, including invalid
  `FROM_BASE64()` inputs that return `NULL`.

## Deferred Surface

This slice does not support:

- use in predicates, DML assignments, defaults, generated columns, check
  constraints, grouping expressions, aggregate arguments, joins beyond the
  existing row-scalar envelope, CTEs, views, stored programs, or arbitrary
  expression parents;
- nested `TO_BASE64()` / `FROM_BASE64()` inside broader functions or arithmetic
  expressions, except where an existing top-level scalar path already admits a
  scalar value that is then passed as this function's single argument;
- decimal, approximate, `BIT`, `ENUM`, `SET`, JSON, spatial, temporal, or other
  descriptor domains as row-backed arguments;
- full MySQL string conversion for every expression type;
- charset/collation metadata parity for the `TO_BASE64()` result beyond the
  existing scalar-result conventions;
- protocol binary result metadata parity for the `FROM_BASE64()` result.

## Grammar

MyLite extends the existing expression grammar with function-specific AST nodes:

```lemon
expression(A) ::= TO_BASE64(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_TO_BASE64_FUNCTION, B, R);
}
expression(A) ::= TO_BASE64(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= TO_BASE64(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= FROM_BASE64(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FROM_BASE64_FUNCTION, B, R);
}
expression(A) ::= FROM_BASE64(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= FROM_BASE64(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= TO_BASE64(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FROM_BASE64(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's supported grammar. They are not copied from
MySQL grammar text.

## Runtime Semantics

Planning and execution:

1. Detect supported top-level scalar and row-scalar calls during the existing
   expression analysis.
2. Preserve native-function arity diagnostics before generic unsupported-form
   diagnostics.
3. Convert admitted scalar literals and supported scalar/session values to
   byte sequences with existing MyLite conversion helpers.
4. Resolve row-backed column arguments through MyLite descriptors, never
   SQLite schema text.
5. For `TO_BASE64()`, encode bytes with MySQL's Base64 alphabet, `=` padding,
   and newline insertion after each complete 76-character output line that has
   more output after it.
6. For `FROM_BASE64()`, remove ASCII space, tab, carriage return, and newline,
   validate the remaining padded Base64 text, and return decoded bytes or
   `NULL` for invalid text.
7. Generate row-scalar SQLite SQL only from descriptor-planned expressions,
   quoted identifiers, numbered parameters, and registered MyLite scalar
   functions:

```sql
_mylite_to_base64(argument_sql)
_mylite_from_base64(argument_sql)
```

The generated SQL stays in the physical row path. MyLite does not materialize
source rows in memory to evaluate Base64 functions.

## Ownership Boundaries

- Public API: unchanged. Callers use `mylite_execute()` and existing result
  accessors.
- Statement context: unchanged. Successful `SELECT` and `DO` preserve existing
  row-count, warning-count, and diagnostics snapshot behavior.
- Lexer/parser/AST: adds `TO_BASE64` / `FROM_BASE64` tokens and AST nodes.
  Parser source spans remain authoritative for default result labels.
- Analyzer/planner: owns supported-shape validation, descriptor column
  resolution, and generated SQL construction.
- Runtime helper module: owns Base64 encode/decode and SQLite callback
  registration.
- Catalog: read-only descriptor authority. No descriptors, catalog generation,
  or SQLite schema generation are mutated.
- Result builder: exposes text or binary bytes through existing result object
  conventions.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: uses public scalar function registration only. No SQLite fork patch
  is required.

## Diagnostics

Required diagnostics:

- parser syntax errors through existing parse diagnostics;
- wrong argument count: `1582 / 42000` native-function parameter-count errors
  for `TO_BASE64` and `FROM_BASE64`;
- missing default schema, unknown schema/table, reserved target names, and
  unsupported object kinds through existing row-scalar source diagnostics;
- unknown descriptor column arguments through existing unknown-column
  diagnostics in field-list context;
- unsupported scalar or row-backed argument shapes:
  `TO_BASE64() supports only string, hex, integer, boolean, NULL, supported session scalar, supported system variable, binary cast/convert, and supported descriptor column arguments`
  or
  `FROM_BASE64() supports only string, hex, integer, boolean, NULL, supported session scalar, supported system variable, binary cast/convert, and supported descriptor column arguments`;
- allocation failure through existing `MYLITE_NOMEM` / `HY001` behavior;
- physical SQLite callback failures as statement execution failures through the
  existing physical SQLite error path.

## Tests

Fast C tests must cover:

- scalar `TO_BASE64()` for empty, one-, two-, three-, four-, and six-byte
  strings, `NULL`, integer, boolean, and unary integer values;
- scalar `TO_BASE64()` newline insertion at the 76-character boundary and after
  the first wrapped line;
- scalar `FROM_BASE64()` for padded and unpadded invalid forms, whitespace
  removal, `NULL`, empty input, and embedded-`NUL` output;
- `DO` status and warning-count behavior;
- row-scalar projection over descriptor integer, `VARCHAR`, `TEXT`,
  `VARBINARY`, and BLOB-family columns;
- unknown column and unsupported descriptor diagnostics;
- wrong arity diagnostics;
- independent handles and file preamble preservation for row-backed reads;
- MySQL 8.4.9 expectation script covering the same user-visible results.

Compatibility docs must mark `TO_BASE64()` and `FROM_BASE64()` as limited
support and must not claim general expression, DML, predicate, or protocol
metadata coverage.
