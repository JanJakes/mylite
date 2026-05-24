# Baseline JSON_REPLACE Function

## Goal

Add a narrow, descriptor-aware implementation of MySQL `JSON_REPLACE()` for
scalar projection contexts. This slice builds directly on the existing
`JSON_SET()` JSON document/path/value infrastructure while preserving MyLite's
current architecture boundaries.

The admitted execution contexts are:

- `SELECT JSON_REPLACE(...)` without a source table;
- `SELECT JSON_REPLACE(...) FROM DUAL`;
- `DO JSON_REPLACE(...)`;
- single-table row-scalar `SELECT JSON_REPLACE(...) FROM table` with the
  existing descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT` subsets.

This feature does not add JSON storage partial updates, DML assignment
expressions, `JSON_INSERT()`, `JSON_REMOVE()`, `JSON_ARRAY_INSERT()`, arbitrary
expression arguments, joins beyond the existing row-scalar envelope, or SQLite
fork changes.

## Compatibility Authority

Compatibility is based on:

- MySQL 8.4 Reference Manual, "Functions That Modify JSON Values":
  https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html
- MySQL 8.4.9 runtime probes against the local `mysql:8.4.9` comparison
  container.

Observed MySQL 8.4.9 behavior used by this slice:

- `JSON_REPLACE(json_doc, path, val[, path, val]...)` requires a document plus
  one or more path/value pairs, so the total argument count is odd and at least
  3.
- `json_doc` or any path argument evaluating to SQL `NULL` returns SQL `NULL`.
- SQL `NULL` used as a value becomes JSON `null`.
- Path/value pairs apply left to right. Later pairs observe earlier changes.
- The root path `$` replaces the whole document.
- Existing object members and array elements are replaced.
- Missing object members, missing array elements, missing intermediate parents,
  and final array indexes beyond the current array length are ignored.
- A final array index against a scalar document or scalar member replaces the
  scalar only for index `0`; nonzero indexes are ignored.
- SQL string values are inserted as JSON strings; JSON descriptor values and
  supported JSON-producing function results are inserted as JSON values.
- Invalid JSON document text raises MySQL error `3141 / 22032`.
- Invalid JSON path text raises MySQL error `3143 / 42000`.
- Wildcard or range path forms are outside this slice and are rejected with a
  deterministic unsupported-path diagnostic matching the existing `JSON_SET()`
  policy.

## Architecture

Ownership boundaries:

- Public API: unchanged. Results use existing scalar result and non-row `DO`
  conventions.
- Statement context: unchanged. Selected schema matters only through existing
  row-scalar source planning.
- Lexer/parser/AST: recognize `JSON_REPLACE` as a nonreserved function name and
  build a MyLite AST node containing the ordinary function argument list.
- Analyzer/planner: validate argument count and admitted argument shapes.
  Descriptor column references resolve through MyLite catalog descriptors, not
  SQLite metadata.
- Catalog: read-only for this feature. No descriptor rows, descriptor versions,
  cache generations, or SQLite schema generation values change.
- Runtime JSON module: reuse the baseline JSON document tree, parse the current
  simple path subset, apply replacement-only mutation, and emit normalized JSON
  text.
- SQLite physical execution: row-scalar table scans remain SQLite-driven.
  MyLite lowers row-scalar `JSON_REPLACE()` to a private deterministic scalar
  function `_mylite_json_replace(...)`; SQLite invokes it per output row.
- Storage/VFS/SQLite fork: unchanged. The `.mylite` preamble, shifted SQLite
  payload, VFS, and SQLite fork patch stack are not touched.

## Syntax

MyLite Lemon-syntax sketch:

```lemon
expression(A) ::= JSON_REPLACE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_REPLACE_FUNCTION, B, R);
}

expression(A) ::= JSON_REPLACE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR, NULL, R);
}

identifier(A) ::= JSON_REPLACE(T).
```

Runtime validation requires `argument_count >= 3` and an odd argument count. Any
other count reports MySQL-compatible native function parameter-count error
`1582 / 42000`.

## Admitted Arguments

Document argument:

- SQL string literal containing valid baseline JSON;
- SQL `NULL`;
- row-scalar descriptor column of JSON or nonbinary string family;
- nested `JSON_REPLACE()` document arguments to another `JSON_REPLACE()` are
  not admitted unless a later feature explicitly adds recursive mutation
  values. Current `JSON_EXTRACT()` and JSON introspection paths may consume a
  no-source or row-scalar `JSON_REPLACE()` result where they already consume
  `JSON_SET()` results.

Path arguments:

- SQL string literals;
- SQL `NULL`.

Path expressions admitted by the runtime reuse the `JSON_SET()` simple path
subset:

- root `$`;
- unquoted ASCII members such as `$.a`;
- quoted ASCII members such as `$."a-b"`;
- nonnegative decimal array indexes such as `$[0]`, `$[1]`;
- nested combinations of these forms.

Wildcard, recursive wildcard, ranges, `last`, non-ASCII quoted path members,
and other path forms remain outside this slice.

Value arguments:

- SQL string literals, inserted as JSON strings;
- signed integer literals within the signed 64-bit range;
- `TRUE` and `FALSE` literals, inserted as JSON booleans;
- SQL `NULL`, inserted as JSON `null`;
- row-scalar descriptor columns of JSON, integer, and nonbinary string family;
- no-source scalar evaluation accepts nested supported JSON-producing
  expressions where already implemented by the scalar path, including
  `JSON_EXTRACT()`, `JSON_ARRAY()`, and `JSON_OBJECT()`;
- row-scalar lowering accepts `JSON_ARRAY()` and `JSON_OBJECT()` as JSON value
  arguments.

Binary string, BIT, decimal/float, temporal, arbitrary arithmetic, subquery,
parameter, and user-variable value arguments are outside this slice unless they
already fold through one of the admitted scalar paths.

## Semantics

`JSON_REPLACE()` parses the document once, applies each path/value pair in
argument order, then emits normalized JSON text using the existing MyLite JSON
writer.

For each path pair:

- If the path is `$`, the entire document is replaced with the value.
- If every parent leg exists and the final leg identifies an existing object
  member or array element, that value is replaced.
- If any parent leg is missing, the pair has no effect.
- If the final member leg is missing from an existing object, the pair has no
  effect.
- If the final array index is at or beyond the current array length, the pair
  has no effect.
- If the final array index targets a non-array value, index `0` replaces the
  value and indexes greater than `0` have no effect.

Successful scalar `SELECT` returns one JSON result column, nullable, with the
existing JSON result metadata. `DO JSON_REPLACE(...)` returns no rows and sets
`ROW_COUNT()` to `0` with warning count `0`. Successful scalar `SELECT`
preserves the existing `ROW_COUNT() = -1` and warning count `0` behavior.

## Diagnostics

The implementation reports:

- Incorrect argument count: `1582 / 42000`, native function parameter count.
- Invalid document text: `3141 / 22032`, invalid JSON text in argument 1 to
  JSON function.
- Invalid path text: `3143 / 42000`, invalid JSON path expression.
- Invalid document data type: `3146 / 22032` when an admitted parsing path
  reaches a non-JSON, non-string document value.
- Unsupported path shape: MyLite unsupported-feature diagnostic aligned with
  the existing JSON mutation path policy.
- Unsupported value or document argument shape: MyLite unsupported-feature
  diagnostic.
- Binary JSON argument where unsupported by current JSON policy: existing
  MyLite JSON binary-character-set diagnostic.
- Allocation failure: existing no-memory diagnostic.
- Physical SQLite scalar callback misuse/failure: existing runtime error
  mapping.

## Tests

Add a dedicated C runtime test and a MySQL expectation script covering:

- no-source, `DUAL`, and `DO` use;
- root replacement, member replacement, missing member no-op, nested member
  replacement, missing intermediate parent no-op, array replacement, array
  out-of-range no-op, scalar index `0` replacement, scalar nonzero index no-op,
  duplicate paths, SQL string values, JSON values, booleans, integers, and SQL
  `NULL` value/document/path behavior;
- table-backed row-scalar projection with JSON, integer, boolean-like integer,
  nonbinary string, and nullable values;
- `WHERE`, `ORDER BY`, and `LIMIT` in the existing row-scalar subset;
- reopen persistence of source rows and lack of source mutation;
- diagnostics for incorrect argument counts, invalid document, invalid path,
  unsupported wildcard path, non-JSON document data type, unsupported value
  expressions, unknown columns, and binary columns;
- parser coverage for the new AST node and argument-count node;
- compatibility docs showing `JSON_REPLACE()` as partial/limited.

## Compatibility Gaps

MyLite still does not claim full JSON mutation compatibility. Deferred behavior
includes stored partial JSON updates, recursive mutation values, `CAST(... AS
JSON)`, binary JSON storage, decimal/float/temporal values, non-ASCII path
members, wildcard/range paths, predicates, ordering/grouping expressions, DML
assignment expressions, arbitrary nested expressions, and protocol metadata
beyond the existing limited scalar/row-scalar JSON metadata.
