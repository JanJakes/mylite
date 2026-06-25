# Baseline JSON merge functions

## Scope

This slice implements the baseline MySQL 8.4.9 behavior for:

- `JSON_MERGE()`
- `JSON_MERGE_PATCH()`
- `JSON_MERGE_PRESERVE()`

The feature is limited to the same expression-context envelope as the recent
baseline JSON mutation functions: no-source `SELECT`, `SELECT ... FROM DUAL`,
`DO`, single-table row-scalar projection, and compatible non-key single-table
`UPDATE` assignments where the existing row-scalar JSON path accepts JSON value
producers.

The implementation uses MyLite's JSON DOM and existing MyLite/SQLite private
scalar function bridge. It does not require a SQLite fork hook.

Official reference: MySQL 8.4 Reference Manual,
`https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html`.
Runtime expectations are verified against MySQL 8.4.9.

## Syntax

MyLite accepts the function calls in expression positions:

```lemon
expression(A) ::= JSON_MERGE(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= JSON_MERGE_PATCH(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= JSON_MERGE_PRESERVE(T) LPAREN function_argument_list(B) RPAREN(R).

expression(A) ::= JSON_MERGE(T) LPAREN RPAREN(R).
expression(A) ::= JSON_MERGE_PATCH(T) LPAREN RPAREN(R).
expression(A) ::= JSON_MERGE_PRESERVE(T) LPAREN RPAREN(R).
```

Each function requires at least two arguments. Zero-argument and one-argument
calls return MySQL's native function parameter-count diagnostic.

The three names remain usable as identifiers when they are not parsed as
function calls.

## Argument model

Supported document arguments are SQL string literals, SQL `NULL`, JSON
descriptor columns, nonbinary string descriptor columns, and the current
baseline JSON value producers admitted by the scalar/row-scalar JSON document
pipeline. Nonbinary document strings are parsed as JSON text.

Unsupported argument classes are diagnosed consistently with the existing JSON
baseline:

- Numeric SQL literals report invalid JSON data type.
- Binary string expressions report the MySQL-shaped binary JSON charset error.
- NUL bytes in SQL string literals are rejected by the existing string literal
  decoder.
- Arbitrary expressions outside the documented envelope remain unsupported.

## Result Semantics

`JSON_MERGE_PRESERVE()` combines all non-`NULL` documents left to right:

- Arrays are concatenated.
- Duplicate object keys preserve both values by wrapping or extending arrays.
- Object values with the same key merge recursively.
- Scalar or mixed non-object values are autowrapped in arrays.

`JSON_MERGE()` uses the same result semantics as `JSON_MERGE_PRESERVE()` and
emits the MySQL deprecation warning:

```text
'JSON_MERGE' is deprecated and will be removed in a future release. Please use JSON_MERGE_PRESERVE/JSON_MERGE_PATCH instead
```

`JSON_MERGE_PATCH()` applies patch documents left to right:

- A patch document that is not an object replaces the current result.
- If the current result is not an object and the patch is an object, the current
  result is treated as an empty object.
- Object members whose patch value is JSON `null` remove that member.
- Object members whose current and patch values are both objects merge
  recursively.
- Other object members replace the current value.

Emitted JSON uses the same canonical writer and object key display ordering as
the existing MyLite JSON functions.

## SQL NULL And Validation

MySQL differentiates the NULL rules:

- `JSON_MERGE()` and `JSON_MERGE_PRESERVE()` short-circuit to SQL `NULL` when
  the first SQL `NULL` argument is encountered. Later arguments are not
  validated.
- `JSON_MERGE_PATCH()` returns SQL `NULL` when any SQL `NULL` argument is
  present, but validates every non-`NULL` document argument before returning.

JSON literal `null` is a normal JSON value, not SQL `NULL`.

## Errors And Warnings

The baseline preserves the existing MyLite JSON diagnostic families:

- Incorrect argument count: MySQL error `1582`, SQLSTATE `42000`.
- Invalid JSON text: MySQL error `3141`, SQLSTATE `22032`.
- Binary JSON charset input: MySQL error `3144`, SQLSTATE `22032`.
- Invalid SQL data type for JSON document input: MySQL error `3146`, SQLSTATE
  `22032`.
- `JSON_MERGE()` deprecation warning: warning `1287`, SQLSTATE `HY000`.

## Contexts

Supported contexts:

- No-source `SELECT`.
- `SELECT ... FROM DUAL`.
- `DO`.
- Single descriptor-backed table projection through the row-scalar JSON
  pipeline.
- Compatible non-key single-table `UPDATE` assignment when the assignment uses
  the documented row-scalar expression envelope.

Unsupported contexts remain unsupported rather than silently falling back to
SQLite behavior:

- General predicates, ordering, grouping, joins beyond the documented row-scalar
  envelope, and arbitrary expression arguments.
- Stored partial JSON updates.
- Binary JSON storage, generated/path indexes, JSON comparison or collation
  semantics beyond the current baseline.
- Protocol metadata beyond existing scalar and row-scalar JSON result-column
  descriptors.

## Runtime Probes

Observed MySQL 8.4.9 behavior includes:

- `JSON_MERGE_PRESERVE('[1,2]', '[true,false]')` returns
  `[1, 2, true, false]`.
- `JSON_MERGE_PATCH('[1,2]', '[true,false]')` returns `[true, false]`.
- `JSON_MERGE('1','true')` returns `[1, true]` and emits warning `1287`.
- `JSON_MERGE_PRESERVE('{"a":1}', '{"a":3}', '{"a":5}')` returns
  `{"a": [1, 3, 5]}`.
- `JSON_MERGE_PATCH('{"a":1,"b":2}', '{"b":null}')` returns `{"a": 1}`.
- `JSON_MERGE_PATCH(NULL, '{"a":1}')` returns SQL `NULL`.
- `JSON_MERGE_PATCH(NULL, '{bad}')` errors on invalid JSON text.
- `JSON_MERGE_PRESERVE(NULL, '{bad}')` returns SQL `NULL`.
- `JSON_MERGE_PRESERVE('{bad}', NULL)` errors on invalid JSON text.
- `JSON_MERGE_PATCH(1, '{"a":1}')` reports invalid JSON data type.
- `JSON_MERGE_PRESERVE(CAST('{"a":1}' AS BINARY), '{"b":2}')` reports the
  binary JSON charset error.

## Test Plan

Add MySQL-runtime expectation coverage and C runtime tests for:

- Basic preserve, patch, and deprecated merge outputs.
- Recursive object merge and duplicate-key preservation.
- Array concatenation and scalar autowrap.
- JSON literal `null` versus SQL `NULL`.
- The distinct SQL `NULL` validation rules for patch versus preserve.
- Deprecation warning count for `JSON_MERGE()`.
- Invalid JSON, invalid SQL type, binary string input, and argument-count
  errors.
- Table-backed projection and supported `UPDATE` assignment.
- Parser AST coverage for all three function names and argument-count markers.
