# Baseline JSON_INSERT Function

## Goal

Add a narrow, descriptor-aware implementation of MySQL `JSON_INSERT()` for
scalar projection contexts. This slice completes the current MyLite JSON
document mutation family built around `JSON_SET()`, `JSON_REPLACE()`, and
`JSON_REMOVE()` while preserving public API, catalog, storage, VFS, and SQLite
fork boundaries.

The admitted execution contexts are:

- `SELECT JSON_INSERT(...)` without a source table;
- `SELECT JSON_INSERT(...) FROM DUAL`;
- `DO JSON_INSERT(...)`;
- single-table row-scalar `SELECT JSON_INSERT(...) FROM table` with the
  existing descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT` subsets.

This feature does not add stored partial JSON updates, DML assignment
expressions, `JSON_ARRAY_INSERT()`, arbitrary expression arguments, predicates
over JSON mutation results, ordering/grouping expressions, joins beyond the
existing row-scalar envelope, or SQLite fork changes.

## Compatibility Authority

Compatibility is based on:

- MySQL 8.4 Reference Manual, "Functions That Modify JSON Values":
  <https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_json_insert_function_expectations.sh`.

Observed MySQL 8.4.9 behavior used by this slice:

- `JSON_INSERT(json_doc, path, val[, path, val]...)` requires a document plus
  one or more path/value pairs, so the total argument count is odd and at least
  `3`.
- `json_doc` or any path argument evaluating to SQL `NULL` returns SQL `NULL`.
- If a path argument is SQL `NULL`, the document and any earlier non-`NULL`
  paths are still parsed for syntactic validity; later paths are not validated.
- SQL `NULL` used as a value becomes JSON `null`.
- Path/value pairs apply left to right. Later pairs observe earlier changes.
- Existing object members and array elements are not overwritten.
- The root path `$` is an existing path and is therefore a no-op.
- Missing object members in existing objects are added.
- Missing intermediate parents are ignored.
- A final array index at or past the current array length appends one value.
- A final array index against a non-array value autowraps the old value and
  appends the new value only when the index is greater than `0`; index `0`
  already identifies the existing value and is ignored.
- SQL string values are inserted as JSON strings; JSON descriptor values and
  supported JSON-producing function results are inserted as JSON values.
- Invalid JSON document text raises MySQL error `3141 / 22032`.
- Invalid JSON path text raises MySQL error `3143 / 42000`.
- Paths with wildcard/range forms are outside this slice and are rejected with a
  deterministic MyLite unsupported-feature diagnostic.

## Architecture

Ownership boundaries:

- Public API: unchanged. Results use existing scalar result and non-row `DO`
  conventions.
- Statement context: unchanged. Selected schema matters only through existing
  row-scalar source planning.
- Lexer/parser/AST: recognize `JSON_INSERT` as a nonreserved function name and
  build a MyLite AST node containing the ordinary function argument list.
- Analyzer/planner: validate argument count and admitted argument shapes.
  Descriptor column references resolve through MyLite catalog descriptors, not
  SQLite metadata.
- Catalog: read-only for this feature. No descriptor rows, descriptor versions,
  cache generations, or SQLite schema generation values change.
- Runtime JSON module: reuse the baseline JSON document tree, parse the current
  simple path subset, apply insert-only mutation, and emit normalized JSON text.
- SQLite physical execution: row-scalar table scans remain SQLite-driven.
  MyLite lowers row-scalar `JSON_INSERT()` to a private deterministic scalar
  function `_mylite_json_insert(...)`; SQLite invokes it per output row.
- Storage/VFS/SQLite fork: unchanged. The `.mylite` preamble, shifted SQLite
  payload, VFS, and SQLite fork patch stack are not touched.

## Syntax

MyLite Lemon-syntax sketch:

```lemon
expression(A) ::= JSON_INSERT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_INSERT_FUNCTION, B, R);
}

expression(A) ::= JSON_INSERT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR, NULL, R);
}

identifier(A) ::= JSON_INSERT(T).
```

Runtime validation requires `argument_count >= 3` and an odd argument count.
Any other count reports MySQL-compatible native function parameter-count error
`1582 / 42000`.

## Admitted Arguments

Document argument:

- SQL string literal containing valid baseline JSON;
- SQL `NULL`;
- row-scalar descriptor column of JSON or nonbinary string family;
- nested `JSON_INSERT()` document arguments are admitted only where another
  current JSON function explicitly lists current JSON mutation results.

Path arguments:

- SQL string literals;
- SQL `NULL`.

Path expressions admitted by the runtime reuse the current simple JSON mutation
path subset:

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

Binary string, `BIT`, decimal/float, temporal, arbitrary arithmetic, subquery,
parameter, and user-variable value arguments are outside this slice unless they
already fold through one of the admitted scalar paths.

## Semantics

`JSON_INSERT()` parses the document once, applies each path/value pair in
argument order, then emits normalized JSON text using the existing MyLite JSON
writer.

If the document is SQL `NULL`, the result is SQL `NULL` without path
validation. If a path argument is SQL `NULL`, MyLite parses the document and
earlier path text but returns SQL `NULL` without applying insertions and without
validating later paths.

For each non-`NULL` path pair:

- If the path is `$`, the pair has no effect.
- If every parent leg exists and the final member leg identifies an existing
  object member, the pair has no effect.
- If every parent leg exists and the final member leg is missing from an
  existing object, the member is added and the containing object is normalized
  using MyLite's existing JSON object key order.
- If every parent leg exists and the final array leg identifies an existing
  array element, the pair has no effect.
- If every parent leg exists and the final array leg is at or beyond the
  current array length, one value is appended.
- If the final array leg targets a non-array value, index `0` has no effect and
  indexes greater than `0` replace that value with an array containing the old
  value followed by the new value.
- If any parent leg is missing, the pair has no effect.

Successful scalar `SELECT` returns one JSON result column, nullable, with the
existing JSON result metadata. `DO JSON_INSERT(...)` returns no rows and sets
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
- existing member no-op, missing member insertion, missing intermediate parent
  no-op, duplicate/left-to-right paths, root no-op, array existing-element
  no-op, array append, scalar index `0` no-op, scalar nonzero index autowrap,
  leading-zero array indexes, SQL string values, JSON values, booleans,
  integers, and SQL `NULL` value/document/path behavior;
- table-backed row-scalar projection with JSON, integer, boolean-like integer,
  nonbinary string, and nullable values;
- `WHERE`, `ORDER BY`, and `LIMIT` in the existing row-scalar subset;
- composition where current JSON extraction/type functions consume
  `JSON_INSERT()` results;
- reopen persistence of source rows and lack of source mutation;
- diagnostics for incorrect argument counts, invalid document, invalid path,
  unsupported wildcard path, non-JSON document data type, unsupported value
  expressions, unknown columns, table-backed nonliteral paths when outside the
  row-scalar slice, and binary columns;
- parser coverage for the new AST node and argument-count node;
- compatibility docs showing `JSON_INSERT()` as partial/limited.

## Compatibility Gaps

MyLite still does not claim full JSON mutation compatibility. Deferred behavior
includes stored partial JSON updates, recursive mutation values, `CAST(... AS
JSON)`, binary JSON storage, decimal/float/temporal JSON number handling,
non-ASCII path members, wildcard/range paths, predicates, ordering/grouping
expressions, DML assignment expressions, arbitrary nested expressions, and
protocol metadata beyond the existing limited scalar/row-scalar JSON metadata.
