# Baseline Trim String Functions

## Summary

This phase adds a narrow MySQL-compatible string trimming family:

```sql
LTRIM(expr)
RTRIM(expr)
TRIM(expr)
TRIM([BOTH | LEADING | TRAILING] [remstr] FROM expr)
TRIM([remstr] FROM expr)
```

The supported slice covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It does not add
general expression predicates, expression ordering, DML assignment values,
generated columns, defaults, or arbitrary nested expression planning.

The core behavior is:

- `LTRIM()` removes leading ASCII space characters;
- `RTRIM()` removes trailing ASCII space characters;
- `TRIM(expr)` removes leading and trailing ASCII space characters;
- explicit `TRIM(... FROM expr)` removes exact prefix and/or suffix occurrences
  of the supplied remove string;
- `NULL` input or `NULL` remove string returns `NULL`;
- an empty remove string returns the original string;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-string-case-functions/specs.md`
  - `docs/specs/baseline-string-slice-functions/specs.md`
  - `docs/specs/baseline-charset-collation-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
  - character set and collation of string-function results:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_trim_string_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `LTRIM()` and `RTRIM()` require exactly one argument and wrong arity fails
  with `1582 / 42000`;
- malformed `TRIM()` syntax such as `TRIM()` and `TRIM('a', 'b')` fails with
  `1064 / 42000`;
- function-name whitespace such as `LTRIM ('  a')` is accepted in default SQL
  mode;
- default trimming removes only ASCII space byte `0x20`, not tabs or newlines;
- `TRIM(remstr FROM str)` defaults to trimming both sides;
- `TRIM(LEADING FROM str)`, `TRIM(TRAILING FROM str)`, and
  `TRIM(BOTH FROM str)` use the default ASCII-space remove string;
- multi-character remove strings are removed as exact repeated prefix/suffix
  sequences, not as a set of individual characters;
- partial suffix/prefix sequences are preserved;
- overlapping exact matches are removed greedily from each admitted side;
- numeric and boolean arguments are converted to their visible string form
  before trimming;
- `TRIM(NULL FROM 'abc')`, `TRIM('x' FROM NULL)`, `LTRIM(NULL)`, and
  `RTRIM(NULL)` return `NULL`;
- under `utf8mb4_0900_ai_ci`, explicit multibyte remove strings such as
  `TRIM('é' FROM 'ééabcé')` operate on the supplied character sequence;
- binary-string inputs are accepted by MySQL and preserve binary charset and
  collation, but MyLite defers binary result metadata for this baseline;
- successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported trim functions add no warnings.
- Lexer/parser/AST: admits the function names and explicit `TRIM ... FROM`
  forms while preserving source spans for labels and diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated SQLite
  SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers internal scalar functions through SQLite's public
  function API for trim semantics. No SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT trim_item[, trim_item ...]
SELECT trim_item[, trim_item ...] FROM DUAL
```

`DO` form:

```sql
DO trim_expr[, trim_expr ...]
```

Single-table row-backed forms, with at least one select item containing a trim
function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted function expression shape is:

```sql
trim_expr:
    LTRIM ( trim_value )
  | RTRIM ( trim_value )
  | TRIM ( trim_value )
  | TRIM ( trim_remove FROM trim_value )
  | TRIM ( LEADING trim_remove FROM trim_value )
  | TRIM ( TRAILING trim_remove FROM trim_value )
  | TRIM ( BOTH trim_remove FROM trim_value )
  | TRIM ( LEADING FROM trim_value )
  | TRIM ( TRAILING FROM trim_value )
  | TRIM ( BOTH FROM trim_value )

trim_value:
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
  | ( trim_value )

trim_remove:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | ( trim_remove )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for `trim_value` are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The remove-string expression is deliberately scalar-only in this baseline. It
cannot be another descriptor column, because column-to-column trim removes need
more result typing and generated-SQL coverage than this slice requires.

Binary string, `BIT`, approximate numeric, `ENUM`, `SET`, `JSON`, and spatial
values remain deferred for this function family. Binary strings are deferred
because MySQL preserves their bytes and result type, while MyLite's current
row-scalar string-result plumbing is text-oriented. Approximate numeric values
are deferred because MyLite's current row-scalar expression layer intentionally
avoids table-backed approximate formatting semantics outside dedicated slices.

The following remain outside this phase:

- `WHERE TRIM(column) ...`, `HAVING TRIM(...) ...`, expression `ORDER BY`,
  grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = TRIM(v)`;
- row-backed remove-string columns such as `TRIM(rem_col FROM value_col)`;
- scalar subqueries, correlated subqueries, CTEs, parameters, user variables,
  and stored functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, arbitrary expressions outside the
  supported nested row-scalar value subset, and full expression metadata;
- binary-string result typing and collation-sensitive metadata propagation.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
%type trim_direction { enum mylite_sql_ast_node_kind }

expression(A) ::= LTRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LTRIM_FUNCTION, B, R);
}
expression(A) ::= RTRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_RTRIM_FUNCTION, B, R);
}
expression(A) ::= TRIM(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(
        state, T, MYLITE_SQL_AST_TRIM_FUNCTION, NULL, B, R);
}
expression(A) ::= TRIM(T) LPAREN expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(
        state, T, MYLITE_SQL_AST_TRIM_FUNCTION, B, C, R);
}
expression(A) ::= TRIM(T) LPAREN trim_direction(D) expression(B) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(state, T, D, B, C, R);
}
expression(A) ::= TRIM(T) LPAREN trim_direction(D) FROM expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_trim_function(state, T, D, NULL, C, R);
}

trim_direction(A) ::= LEADING. {
    A = MYLITE_SQL_AST_TRIM_LEADING_FUNCTION;
}
trim_direction(A) ::= TRAILING. {
    A = MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION;
}
trim_direction(A) ::= BOTH. {
    A = MYLITE_SQL_AST_TRIM_FUNCTION;
}
```

Wrong-arity `LTRIM()` and `RTRIM()` forms produce function-argument-count AST
nodes so runtime can emit the native-function parameter-count diagnostic.
Malformed `TRIM()` forms stay syntax errors, matching observed MySQL behavior
for the admitted grammar surface.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the admitted value and optional remove string to text:
   - ordinary string literal: decoded UTF-8 bytes, provided it is NUL-free;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value or system variable: its existing visible
     string value or SQL `NULL`.
3. Return SQL `NULL` when the value or remove string is `NULL`.
4. If the remove string is empty, return the original value unchanged.
5. Remove exact repeated prefix and/or suffix byte sequences according to the
   function kind.
6. Return the trimmed string through existing result APIs.

Default-space trim removes only byte `0x20`. MyLite uses byte-sequence matching
for explicit remove strings. That matches MySQL's observed exact-sequence
behavior for valid UTF-8 strings while avoiding a dependency on SQLite's
`trim()` semantics, which treat a multi-character remove string as a set of
characters and are therefore not MySQL-compatible.

Table-backed projection planning lowers to SQLite expressions using
MyLite-registered scalar functions:

```sql
_mylite_ltrim(value, remstr)
_mylite_rtrim(value, remstr)
_mylite_trim(value, remstr)
```

`LTRIM(value)` becomes `_mylite_ltrim(value, ' ')`, `RTRIM(value)` becomes
`_mylite_rtrim(value, ' ')`, and `TRIM(value)` becomes
`_mylite_trim(value, ' ')`.

The internal SQLite functions:

- return SQL `NULL` for SQL `NULL` in either argument;
- read the input as SQLite text for admitted descriptor families;
- reject embedded-NUL values with a deterministic MyLite runtime error;
- remove exact prefix/suffix byte sequences and return text using SQLite's
  public `sqlite3_result_text()` API.

Generated SQL must quote every descriptor identifier and bind scalar literals,
session values, and the default remove string as parameters. The generated
expressions operate over physical MyLite table names and descriptor columns;
they do not use SQLite schema text as authority.

## Diagnostics

Supported calls produce no warnings.

Diagnostics for this phase:

- syntax errors: existing parser syntax-error path;
- wrong arity for `LTRIM()` or `RTRIM()`: MySQL-compatible `1582 / 42000`
  native-function parameter-count diagnostic naming the called function;
- malformed `TRIM()` syntax: MySQL-compatible syntax error path;
- unsupported argument shape: deterministic MyLite unsupported-feature
  diagnostic;
- unsupported row-backed descriptor column family: deterministic MyLite
  unsupported-feature diagnostic;
- unknown descriptor column: existing `1054 / 42S22` unknown-column diagnostic;
- embedded NUL in string literal or row-backed value: deterministic MyLite
  runtime diagnostic;
- physical SQLite callback, preparation, binding, or stepping failure: existing
  SQLite-to-MyLite runtime diagnostic path;
- allocation failure: existing `MYLITE_NOMEM` / out-of-memory diagnostic path;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

## Result Metadata And Labels

This phase uses the existing row-result conventions. Successful trim calls
return one text column per select item or no rows for `DO`.

Labels follow existing expression-label behavior:

- unaliased select items use the original SQL expression span;
- explicit aliases override the expression label;
- `DO` returns no result columns or rows.

`affected_rows` and `warning_count` follow existing statement conventions:

- successful scalar `SELECT`: `affected_rows == -1`, `warning_count == 0`;
- successful `DO`: `affected_rows == 0`, `warning_count == 0`.

## Storage, Catalog, And Performance

This feature is read-only. It does not modify the catalog, table descriptors,
descriptor versions, descriptor caches, SQLite schema generation, `.mylite`
preamble, shifted SQLite payload, or VFS behavior.

No-source and `DUAL` calls are evaluated directly in MyLite. Row-backed calls
stay close to the existing SQLite execution path: MyLite plans descriptor
columns, emits a scalar expression around the physical column, binds scalar
remove-string parameters, and lets SQLite scan, filter, sort, and limit rows
for the already-supported row-scalar `SELECT` envelope. MyLite does not
materialize the table to trim values in memory.

No SQLite fork patch is needed. The only SQLite integration is through the
public scalar-function registration API.

## Test Plan

Add a MySQL-runtime expectation script covering:

- scalar `LTRIM`, `RTRIM`, and `TRIM`;
- explicit `TRIM` direction and remove-string forms;
- default-space behavior for tabs/newlines;
- exact multi-character remove strings, partial sequences, overlapping
  sequences, empty remove string, numeric remove string, and multibyte remove
  string;
- `NULL`, integer, signed integer, boolean, session scalar, and system variable
  arguments;
- labels, aliases, `DUAL`, `DO`, `ROW_COUNT()`, and `@@warning_count`;
- MySQL-accepted but deferred binary behavior;
- syntax and wrong-arity errors.

Add C parser tests for:

- all admitted function forms;
- argument-count nodes for `LTRIM` and `RTRIM`;
- syntax errors for malformed `TRIM`;
- identifier fallback for nonreserved `LTRIM`, `RTRIM`, and `TRIM`.

Add C runtime tests for:

- no-source, `DUAL`, and `DO` scalar behavior;
- table-backed projection behavior over integer, exact `DECIMAL`, nonbinary
  string, baseline `TEXT`, `YEAR`, and temporal descriptor columns;
- explicit remove strings and default-space trimming on descriptor values;
- reopen persistence and unchanged `.mylite` preamble;
- row envelope integration with `WHERE`, `ORDER BY`, and `LIMIT`;
- deterministic diagnostics for unsupported row-backed columns, unknown
  columns, wrong arity, malformed syntax, unsupported expressions outside the
  supported nested row-scalar value subset, and unsupported row-backed
  remove-string columns.

Verification commands:

```sh
cmake --build --preset dev
ctest --preset dev -R 'libmylite\.(parser|runtime\.trim_string_functions|string_(length|case|slice)_functions)' --output-on-failure
packages/libmylite/tests/mysql_baseline_trim_string_functions_expectations.sh
cmake --workflow --preset check
```

## Known Deferred Work

- Full binary-string result metadata and byte-preserving binary inputs.
- Predicate, grouping, aggregate, and expression-ordering uses.
- DML assignment, generated column, default-expression, and check-constraint
  uses.
- Nested row functions and row-backed remove-string columns.
- Parameters, user variables, stored routines, scalar subqueries, CTEs, and
  correlated subqueries inside trim arguments.
- Full expression metadata and collation/coercibility propagation.
