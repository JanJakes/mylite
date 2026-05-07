# JSON Scalar Foundation

## Scope

This slice adds the first JSON scalar foundation needed by common application
code:

- `JSON_VALID(val)`
- `JSON_TYPE(json_val)`
- `JSON_QUOTE(str)`
- `JSON_UNQUOTE(json_val)`
- `JSON_ARRAY([val[, val] ...])`
- `JSON_OBJECT([key, val[, key, val] ...])`

The slice intentionally does not implement JSON columns, JSON path evaluation,
the `->` / `->>` operators, JSON search functions, JSON mutation functions,
JSON aggregate functions, or JSON comparison semantics.

## Sources

The behavior is specified from the MySQL 8.4 Reference Manual JSON function
documentation and MySQL 8.4.9 runtime observations. The implementation is
independently authored and does not use external implementation sources.

Official references:

- https://dev.mysql.com/doc/refman/8.4/en/json-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/json-creation-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/json-attribute-functions.html

## MySQL 8.4.9 Observations

The following representative probes were run against `mysql:8.4.9`:

```sql
SELECT
  JSON_VALID('{"a":1}'),
  JSON_VALID('hello'),
  JSON_VALID('"hello"'),
  JSON_VALID(NULL);

SELECT
  JSON_TYPE('{"a":[10,true]}'),
  JSON_TYPE('[1,2]'),
  JSON_TYPE('true'),
  JSON_TYPE('null'),
  JSON_TYPE('123'),
  JSON_TYPE('123.45'),
  JSON_TYPE('"x"'),
  JSON_TYPE(NULL);

SELECT
  JSON_QUOTE('null'),
  JSON_QUOTE('"null"'),
  JSON_QUOTE('[1, 2, 3]'),
  JSON_QUOTE(NULL);

SELECT
  JSON_UNQUOTE('"abc"'),
  JSON_UNQUOTE('123'),
  JSON_UNQUOTE('true'),
  JSON_UNQUOTE('null'),
  JSON_UNQUOTE('[1,2]');

SELECT
  JSON_ARRAY(),
  JSON_ARRAY('a', 1, NULL, TRUE, FALSE),
  JSON_ARRAY(JSON_OBJECT('x', 1), JSON_ARRAY(2, 3));

SELECT
  JSON_OBJECT(),
  JSON_OBJECT('id', 87, 'name', 'carrot'),
  JSON_OBJECT('b', 1, 'a', 2, 'b', 3);
```

Results:

```text
1 0 1 NULL
OBJECT ARRAY BOOLEAN NULL INTEGER DOUBLE STRING NULL
"null" "\"null\"" "[1, 2, 3]" NULL
abc 123 true null [1,2]
[] ["a", 1, null, true, false] [{"x": 1}, [2, 3]]
{} {"id": 87, "name": "carrot"} {"a": 2, "b": 3}
```

Additional runtime observations:

- `JSON_VALID()` returns `NULL` for SQL `NULL`, `1` for valid JSON text, and
  `0` for invalid text or non-string scalar arguments.
- `JSON_TYPE()` returns `NULL` for SQL `NULL`. Non-string, non-JSON scalar
  arguments return error `3146`; invalid JSON text returns error `3141`.
- `JSON_TYPE()` reports `OBJECT`, `ARRAY`, `BOOLEAN`, `NULL`, `INTEGER`,
  `DOUBLE`, or `STRING` for the text JSON values covered by this slice.
- `JSON_QUOTE()` accepts string arguments, returns SQL `NULL` for SQL `NULL`,
  escapes JSON string special characters, and returns error `3064` for
  non-string scalar arguments.
- `JSON_UNQUOTE()` accepts string arguments, returns SQL `NULL` for SQL
  `NULL`, unescapes JSON string literals, and returns non-string JSON text
  unchanged. Invalid string escapes return error `3141`; non-string scalar
  arguments return error `3064`.
- `JSON_ARRAY()` returns a JSON value. SQL strings become JSON strings, SQL
  numbers become JSON numbers, SQL `NULL` becomes JSON `null`, boolean
  literals become JSON booleans, and direct nested JSON creation-function
  results are inserted as JSON values rather than quoted strings.
- `JSON_OBJECT()` returns a JSON object. It accepts zero arguments or an even
  number of key/value arguments. Odd arity returns error `1582`; SQL `NULL`
  keys return error `3158`.
- Object keys are normalized by MySQL's binary JSON order. Observed order is by
  key byte length and then byte value. Duplicate keys keep only the last value.
- `JSON_QUOTE()` is not treated as a JSON-typed creation result when nested in
  `JSON_ARRAY()` or `JSON_OBJECT()`; its text is quoted like any other SQL
  string.

Observed metadata for a projection containing all six functions:

- `JSON_VALID()`: nullable `LONGLONG`, display length `21`, binary collation,
  decimals `0`, numeric and binary flags.
- `JSON_TYPE()`, `JSON_QUOTE()`, and plain-string `JSON_UNQUOTE()`:
  nullable `VAR_STRING`, decimals `31`, binary flag, connection-compatible
  character metadata, and MySQL-derived display lengths based on the argument
  shape.
- `JSON_UNQUOTE(JSON_EXTRACT(...))`: nullable `LONG_BLOB`, decimals `31`,
  binary flag, connection-compatible character metadata, and MySQL-derived
  long-text display length.
- `JSON_ARRAY()` and `JSON_OBJECT()`: nullable JSON field type, byte-scaled
  JSON document display length, decimals `31`, binary flag,
  connection-compatible character metadata.

MyLite exposes a public `MYLITE_FIELD_TYPE_JSON` field type for JSON-typed
result metadata.

## Syntax

No new grammar production is required. The functions use MyLite's ordinary
function-call syntax:

```lemon
scalar_expression(A) ::= function_name(B) LP opt_function_arguments(C) RP. {
    A = mylite_sql_parser_make_function_call(state, B, C);
}
```

Arity rules:

- `JSON_VALID`, `JSON_TYPE`, `JSON_QUOTE`, `JSON_UNQUOTE`: exactly one
  argument.
- `JSON_ARRAY`: zero or more arguments.
- `JSON_OBJECT`: zero or any positive even number of arguments.

## Semantics

The JSON parser accepts RFC 8259 JSON values with MySQL-compatible top-level
scalars. It validates complete consumption of the input after trailing
whitespace. Strings decode the JSON escapes `"`, `\`, `/`, `b`, `f`, `n`, `r`,
`t`, and `uXXXX`. Unicode escapes, including surrogate pairs, decode to UTF-8
for valid Unicode scalar values.

`JSON_VALID(val)`:

1. Evaluate the argument.
2. Return SQL `NULL` if the argument is SQL `NULL`.
3. Return `0` if the argument is not a string.
4. Parse the string as JSON and return `1` or `0`.

`JSON_TYPE(json_val)`:

1. Evaluate the argument.
2. Return SQL `NULL` if the argument is SQL `NULL`.
3. Reject non-string scalar arguments with error `3146`.
4. Parse the string as JSON, reject invalid JSON with error `3141`, and return
   the root JSON type name.

`JSON_QUOTE(str)`:

1. Evaluate the argument.
2. Return SQL `NULL` if the argument is SQL `NULL`.
3. Reject non-string scalar arguments with error `3064`.
4. Return a JSON string literal containing the argument text.

`JSON_UNQUOTE(json_val)`:

1. Evaluate the argument.
2. Return SQL `NULL` if the argument is SQL `NULL`.
3. Reject non-string scalar arguments with error `3064`.
4. If the text is a valid JSON string literal, return its decoded contents.
5. If the text begins with a quote but has invalid JSON string escapes, return
   error `3141`.
6. Otherwise return the argument text unchanged.

`JSON_ARRAY()` evaluates values left-to-right and serializes them to a JSON
array. Direct `JSON_ARRAY()` and `JSON_OBJECT()` child expressions are embedded
as JSON values. Other text results are serialized as JSON strings.

`JSON_OBJECT()` evaluates key/value pairs left-to-right. Keys are converted to
text except that SQL `NULL` keys are rejected. Values follow `JSON_ARRAY()`
serialization. Duplicate keys replace earlier values, and the output is sorted
by key byte length then byte value to match observed MySQL normalization.

## Errors And Warnings

- `JSON_OBJECT()` odd arity: error `1582`, incorrect parameter count.
- `JSON_OBJECT(NULL, value)`: error `3158`, JSON documents may not contain
  `NULL` member names.
- `JSON_TYPE(non_string)` returns error `3146`.
- `JSON_TYPE(invalid_json_text)` returns error `3141`.
- `JSON_QUOTE(non_string)` and `JSON_UNQUOTE(non_string)` return error `3064`.
- `JSON_UNQUOTE()` invalid JSON string escapes return error `3141`.

These are execution errors in current MyLite scalar expression paths, represented
as error-level expression conditions.

## Metadata

`JSON_VALID()`:

- field type: `LONGLONG`
- display length: `21`
- charset: binary
- decimals: `0`
- numeric and binary flags
- nullable if its argument may be nullable

`JSON_TYPE()`, `JSON_QUOTE()`, and `JSON_UNQUOTE()`:

- field type: `VAR_STRING`
- charset/collation: current connection character metadata
- decimals: `31`
- binary flag set
- nullable
- display length: `JSON_TYPE()` uses 17 characters, `JSON_QUOTE()` reserves
  escape expansion for its argument, and plain-string `JSON_UNQUOTE()` follows
  the argument display length. `JSON_UNQUOTE()` over JSON-typed expressions
  returns `LONG_BLOB` metadata.

`JSON_ARRAY()` and `JSON_OBJECT()`:

- field type: `JSON`
- length: MySQL's JSON document character limit scaled by connection character
  width, such as `4294967292` under `utf8mb4`
- charset/collation: current connection character metadata
- decimals: `31`
- binary flag set
- nullable

## Runtime And Storage

This slice does not alter MyLite storage. JSON values remain scalar expression
results represented as text plus JSON field metadata. JSON column storage,
binary JSON layout, partial update metadata, generated-column interactions, and
JSON indexing remain deferred.

## Tests

Runtime coverage must include:

- no-table `SELECT` projection for all six functions
- table projection, `WHERE`, `ORDER BY`, `UPDATE`, and `DELETE` expression
  paths where ordinary scalar functions are supported
- `NULL` propagation and non-string argument diagnostics
- JSON parser coverage for objects, arrays, strings, numbers, booleans, JSON
  `null`, whitespace, escapes, and invalid text
- JSON string quoting and unquoting of newline, tab, quote, backslash, control,
  and Unicode escapes
- JSON array/object serialization for SQL text, numbers, SQL `NULL`, boolean
  literals, and direct nested JSON creation functions
- duplicate object-key replacement and key-order normalization
- odd-arity and `NULL` key errors for `JSON_OBJECT()`
- result metadata for numeric, string, and JSON-valued functions
