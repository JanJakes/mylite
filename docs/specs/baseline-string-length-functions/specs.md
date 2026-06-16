# Baseline String Length Functions

## Summary

This phase adds a narrow, commonly used MySQL string-length function family:

```sql
LENGTH(expr)
OCTET_LENGTH(expr)
BIT_LENGTH(expr)
CHAR_LENGTH(expr)
CHARACTER_LENGTH(expr)
```

The supported slice covers no-source, `FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection contexts. A later
`baseline-string-function-predicates` slice reuses this row-scalar expression
for narrow descriptor-backed `WHERE` predicates. This phase deliberately does
not add general expression predicates, expression ordering, DML assignments,
generated columns, or arbitrary nested expression planning.

The core behavior is:

- `LENGTH()` and `OCTET_LENGTH()` return byte length;
- `BIT_LENGTH()` returns byte length multiplied by 8;
- `CHAR_LENGTH()` and `CHARACTER_LENGTH()` return character count for MyLite
  UTF-8 text values and byte count for binary string and `BIT` values;
- `NULL` input returns `NULL`;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-scalar-expression-projection/specs.md`
  - `docs/specs/baseline-cast-binary/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-binary-string-types/specs.md`
  - `docs/specs/baseline-bit-type/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_length_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 probes establish the behavior used by this phase:

- each function requires exactly one argument and wrong arity fails with
  `1582 / 42000`;
- function-name whitespace such as `LENGTH ('a')` is accepted in default SQL
  mode;
- `LENGTH('abc')`, `OCTET_LENGTH('abc')`, `CHAR_LENGTH('abc')`,
  `CHARACTER_LENGTH('abc')`, and `BIT_LENGTH('abc')` return `3`, `3`, `3`,
  `3`, and `24`;
- with a UTF-8 client/session, `LENGTH('é')` returns `2`, while
  `CHAR_LENGTH('é')` returns `1`; `LENGTH('🙂')` returns `4`, while
  `CHAR_LENGTH('🙂')` returns `1`;
- numeric and boolean arguments are converted to their visible string form
  before measuring, so `LENGTH(123) = 3`, `CHAR_LENGTH(-7) = 2`, and
  `BIT_LENGTH(TRUE) = 8`;
- `LENGTH(NULL)`, `CHAR_LENGTH(NULL)`, and `BIT_LENGTH(NULL)` return `NULL`;
- `CHAR` trailing spaces are stripped in the current default SQL mode before
  length functions see the value;
- binary string and `BIT` columns are measured in bytes for both byte-length
  and character-length functions;
- successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text integers or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported string-length functions add no warnings.
- Lexer/parser/AST: admits the five one-argument function names and wrong-arity
  nodes while preserving source spans for result labels and diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated SQLite
  SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to SQLite expressions
  over stable physical table names and quoted physical column names. MyLite uses
  SQLite's public expression execution and value APIs; no SQLite fork patch is
  required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT string_length_item[, string_length_item ...]
SELECT string_length_item[, string_length_item ...] FROM DUAL
```

`DO` form:

```sql
DO string_length_expr[, string_length_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
string-length function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted function expression shape is:

```sql
string_length_expr:
    LENGTH ( string_length_value )
  | OCTET_LENGTH ( string_length_value )
  | BIT_LENGTH ( string_length_value )
  | CHAR_LENGTH ( string_length_value )
  | CHARACTER_LENGTH ( string_length_value )

string_length_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( string_length_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- binary string family;
- `BIT`.

Approximate numeric columns remain deferred because MyLite's current
row-scalar expression layer intentionally avoids table-backed approximate
formatting semantics outside the existing dedicated slices.

The following remain outside this phase:

- length-function predicate shapes outside the later
  `baseline-string-function-predicates` subset, `HAVING LENGTH(...) ...`,
  expression `ORDER BY`, grouping, distinct expression rows, and aggregate
  arguments;
- DML assignment values such as `UPDATE t SET c = LENGTH(v)`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- string introducers, national strings, arbitrary binary literals outside the
  supported binary value subset, spatial values, JSON values, arbitrary
  expressions outside the supported nested row-scalar value subset, and full
  expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LENGTH_FUNCTION, B, R);
}
expression(A) ::= OCTET_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION, B, R);
}
expression(A) ::= BIT_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIT_LENGTH_FUNCTION, B, R);
}
expression(A) ::= CHAR_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION, B, R);
}
expression(A) ::= CHARACTER_LENGTH(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION, B, R);
}
```

Wrong-arity forms produce function-argument-count AST nodes so runtime can emit
the native-function parameter-count diagnostic. These snippets describe
MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the admitted argument to a measured value:
   - ordinary string literal: decoded UTF-8 bytes, provided they are NUL-free;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value or system variable: its existing visible
     string value or SQL `NULL`.
3. Return SQL `NULL` for `NULL` input.
4. For byte-length functions, return the byte count.
5. For `BIT_LENGTH()`, return byte count multiplied by 8.
6. For character-length functions, validate/count UTF-8 code points for text
   values in the admitted scalar surface.

Table-backed projection planning lowers to SQLite expressions:

- byte length for text-like values uses SQLite `length(CAST(expr AS BLOB))`;
- character length for text-like values uses SQLite `length(expr)`;
- binary string and `BIT` values use SQLite byte length directly;
- `BIT_LENGTH()` multiplies the byte length expression by `8`;
- SQL `NULL` propagates through SQLite's expression semantics.

Generated SQL must quote every descriptor identifier and bind scalar literals
or session values as parameters. The generated expressions operate over
physical MyLite table names and descriptor columns; they do not use SQLite
schema text as authority.

## Diagnostics

Supported calls produce no warnings.

Diagnostics for this phase:

- syntax errors: existing parser syntax-error path;
- wrong arity for any of the five native functions: MySQL-compatible
  `1582 / 42000` native-function parameter-count diagnostic naming the called
  function;
- unknown descriptor column in table-backed projection: MySQL-compatible
  unknown-column diagnostic in field-list context;
- descriptor column reference without a source table: MySQL-compatible
  unknown-column diagnostic;
- unsupported argument expression: deterministic MyLite unsupported-expression
  diagnostic;
- approximate numeric descriptor column: deterministic MyLite unsupported
  diagnostic for this slice;
- string literal containing embedded NUL if such a token reaches this
  evaluator: deterministic MyLite unsupported literal diagnostic;
- invalid UTF-8 in a value that MyLite had previously accepted should be
  treated as storage/runtime corruption and fail through the existing runtime
  error path;
- allocation failure: existing `MYLITE_NOMEM` / SQLSTATE `HY001` path;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

## Result And Metadata

Successful supported `SELECT` statements:

- return one text integer value per function call, or SQL `NULL`;
- use the source expression text as the default column label;
- honor existing select-item alias handling;
- report `warning_count == 0`;
- make a following `ROW_COUNT()` return `-1`.

Successful supported `DO` statements return no row result set, report
`warning_count == 0`, and make a following `ROW_COUNT()` return `0`.

The phase does not add protocol-grade integer metadata, charset metadata,
collation metadata, result flags, or origin metadata. Those remain part of the
broader result-metadata baseline.

## Performance And SQLite Fit

This slice stays close to SQLite's execution path. Table-backed length
functions are translated into SQLite scalar expressions evaluated while SQLite
scans the row envelope. MyLite does not materialize source rows to count bytes
or characters itself. A SQLite fork patch is not needed because public SQLite
SQL expressions and value APIs provide the required behavior for this limited
surface.

## Test Plan

Add a new `runtime_string_length_functions` C test and register it as
`libmylite.runtime.string_length_functions`. Cover:

- parser AST and labels for each function name;
- no-source, `FROM DUAL`, and `DO` successful calls;
- `LENGTH()` / `OCTET_LENGTH()` bytes, `BIT_LENGTH()` bits, and
  `CHAR_LENGTH()` / `CHARACTER_LENGTH()` UTF-8 character counts;
- `NULL`, integer, boolean, string, and session scalar arguments;
- row-backed projections over `CHAR`, `VARCHAR`, `TEXT`, binary string, `BIT`,
  integer, decimal, and temporal descriptors;
- aliases, explicit invisible column references, existing `WHERE`/`ORDER BY`/
  `LIMIT` row envelope reuse, and reopen persistence;
- wrong arity diagnostics for all function names;
- unknown column and unsupported expression diagnostics;
- unsupported predicate/order/DML/nested forms rejected deterministically;
- preservation of existing lexer, parser, scalar expression, row-scalar,
  string, binary string, `BIT`, temporal, runtime lifecycle, file-backed, and
  full check suites.

The MySQL expectation script for this feature records MySQL 8.4.9 behavior for
the admitted surface plus representative broader forms that remain deferred.
