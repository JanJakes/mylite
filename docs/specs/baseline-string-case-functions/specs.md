# Baseline String Case Functions

## Summary

This phase adds a narrow, common MySQL string case-conversion family:

```sql
LOWER(expr)
LCASE(expr)
UPPER(expr)
UCASE(expr)
```

The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It deliberately
does not add general expression predicates, expression ordering, DML assignment
values, generated columns, defaults, or arbitrary nested expression planning.

The core behavior is:

- `LOWER()` and `LCASE()` convert ASCII letters `A..Z` to `a..z`;
- `UPPER()` and `UCASE()` convert ASCII letters `a..z` to `A..Z`;
- non-letter ASCII bytes are unchanged;
- `NULL` input returns `NULL`;
- successful supported calls produce no warnings.

Full Unicode case mapping and collation-dependent folding are not included in
this baseline. Non-ASCII text inputs are rejected deterministically rather than
silently returning SQLite's ASCII-only result as if it matched MySQL.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-field-function/specs.md`
  - `docs/specs/baseline-date-format-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_case_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- each function requires exactly one argument and wrong arity fails with
  `1582 / 42000`;
- function-name whitespace such as `LOWER ('ABC')` is accepted in default SQL
  mode;
- `LCASE()` is a synonym for `LOWER()` and `UCASE()` is a synonym for
  `UPPER()`;
- `LOWER('ABC') = 'abc'` and `UPPER('abc') = 'ABC'`;
- `LOWER(NULL)` and `UPPER(NULL)` return `NULL`;
- numeric and boolean arguments are converted to their visible string form
  before case conversion, so `LOWER(123) = '123'`, `UPPER(-7) = '-7'`,
  `LOWER(TRUE) = '1'`, and `UPPER(FALSE) = '0'`;
- under `utf8mb4_0900_ai_ci`, MySQL lowercases and uppercases at least some
  non-ASCII letters, such as `É` and `é`, using Unicode collation behavior;
- binary-string inputs are not case-converted by MySQL;
- `CHAR` trailing spaces are stripped in the current default SQL mode before
  case functions see the value;
- successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported string-case functions add no warnings.
- Lexer/parser/AST: admits the four one-argument function names and wrong-arity
  nodes while preserving source spans for result labels and diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers an internal scalar function through SQLite's public
  function API for ASCII case conversion and validation. No SQLite fork patch is
  required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT string_case_item[, string_case_item ...]
SELECT string_case_item[, string_case_item ...] FROM DUAL
```

`DO` form:

```sql
DO string_case_expr[, string_case_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
string-case function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted function expression shape is:

```sql
string_case_expr:
    LOWER ( string_case_value )
  | LCASE ( string_case_value )
  | UPPER ( string_case_value )
  | UCASE ( string_case_value )

string_case_value:
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
  | ( string_case_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

Binary string, `BIT`, approximate numeric, `ENUM`, `SET`, `JSON`, and spatial
values remain deferred for this function family. Binary strings are deferred
because MySQL preserves their bytes and result type, while MyLite's current
row-scalar string-result plumbing is text-oriented. Approximate numeric values
are deferred because MyLite's current row-scalar expression layer intentionally
avoids table-backed approximate formatting semantics outside the existing
dedicated slices.

The following remain outside this phase:

- `WHERE LOWER(column) ...`, `HAVING LOWER(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = LOWER(v)`;
- nested row functions such as `LOWER(CONCAT(v, '-'))`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata;
- full Unicode case mapping, collation-sensitive folding, and connection
  collation effects.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= LOWER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOWER_FUNCTION, B, R);
}
expression(A) ::= LCASE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LCASE_FUNCTION, B, R);
}
expression(A) ::= UPPER(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UPPER_FUNCTION, B, R);
}
expression(A) ::= UCASE(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UCASE_FUNCTION, B, R);
}
```

Wrong-arity forms produce function-argument-count AST nodes so runtime can emit
the native-function parameter-count diagnostic. These snippets describe
MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the admitted argument to a text value:
   - ordinary string literal: decoded UTF-8 bytes, provided every byte is ASCII
     and NUL-free;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value or system variable: its existing visible
     string value or SQL `NULL`, provided every byte is ASCII and NUL-free.
3. Return SQL `NULL` for `NULL` input.
4. Apply ASCII case conversion for the chosen function family.
5. Return the converted string through existing result APIs.

Table-backed projection planning lowers to SQLite expressions using a
MyLite-registered scalar function:

```sql
_mylite_lower_ascii(expr)
_mylite_upper_ascii(expr)
```

`LCASE()` uses `_mylite_lower_ascii()`. `UCASE()` uses
`_mylite_upper_ascii()`.

The internal SQLite functions:

- return SQL `NULL` for SQL `NULL`;
- read the input as SQLite text for admitted descriptor families;
- reject non-ASCII or embedded-NUL values with a deterministic MyLite runtime
  error;
- convert ASCII letters and return text using SQLite's public
  `sqlite3_result_text()` API.

Generated SQL must quote every descriptor identifier and bind scalar literals
or session values as parameters. The generated expressions operate over
physical MyLite table names and descriptor columns; they do not use SQLite
schema text as authority.

## Diagnostics

Supported calls produce no warnings.

Diagnostics for this phase:

- syntax errors: existing parser syntax-error path;
- wrong arity for any of the four native functions: MySQL-compatible
  `1582 / 42000` native-function parameter-count diagnostic naming the called
  function;
- unknown descriptor column in table-backed projection: MySQL-compatible
  unknown-column diagnostic in field-list context;
- descriptor column reference without a source table: MySQL-compatible
  unknown-column diagnostic;
- unsupported argument expression: deterministic MyLite unsupported-expression
  diagnostic;
- unsupported descriptor column family: deterministic MyLite unsupported
  diagnostic for this slice;
- non-ASCII or embedded-NUL input: deterministic MyLite unsupported-value
  diagnostic explaining that this baseline supports ASCII string case
  conversion only;
- integer literal outside the existing signed-64 scalar envelope: existing
  scalar integer diagnostic;
- allocation failures: existing `MYLITE_NOMEM` behavior;
- physical SQLite failures: existing runtime diagnostics;
- public API misuse: no public API changes.

## Result Metadata

Default column labels use source spans:

```sql
LOWER('ABC')
LCASE(v)
UPPER(DATABASE())
UCASE(v)
```

Explicit aliases override the default label. Result values are text strings or
SQL `NULL`. Successful no-source and `DUAL` `SELECT` statements report one row,
warning count `0`, and the existing scalar-`SELECT` affected-row convention.
Successful `DO` reports no rows, affected rows `0`, and warning count `0`.
Successful row-backed `SELECT` follows the existing row-scalar source envelope,
including the already supported `WHERE`, `ORDER BY`, and `LIMIT` behavior.

## Performance And SQLite Integration

This feature stays in SQLite's row execution path for table-backed projection.
MyLite resolves descriptors, builds a generated projection expression, binds
parameters, and lets SQLite scan/filter/order/limit rows. MyLite does not
materialize all source rows in C to apply case conversion.

The SQLite integration uses public `sqlite3_create_function_v2()` registration
through MyLite's existing bootstrap surface. No SQLite fork patch is required.
The function is intentionally MyLite-owned so non-ASCII values can fail
deterministically instead of using SQLite's ASCII-only built-ins while claiming
MySQL Unicode behavior.

## Tests

Add MySQL-runtime expectation coverage for:

- core lower/upper/synonym behavior over ASCII strings;
- numeric, boolean, session-scalar, `NULL`, whitespace-after-name, and alias
  behavior;
- `DO`, `ROW_COUNT()`, and warning-count behavior;
- table-backed `VARCHAR`, `CHAR`, `TEXT`, integer, decimal, year, and temporal
  descriptor values with `WHERE`, `ORDER BY`, and `LIMIT`;
- wrong-arity errors for zero and multiple arguments;
- MySQL-accepted but deferred non-ASCII and binary-input behavior.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_string_case_functions`, plus parser coverage in `parser_test.c`.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-string.md`;
- `docs/compatibility/sql-query-expressions.md` if row-scalar wording needs
  the new projection family.

Use limited wording. Do not claim full `LOWER()` / `UPPER()`, Unicode case
mapping, binary-string result typing, predicates, DML assignments,
ordering/grouping expressions, generated columns, defaults, or general
expression evaluation.
