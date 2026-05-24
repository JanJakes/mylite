# Baseline JSON_SET Function

## Goal

Add a narrow, descriptor-aware implementation of MySQL `JSON_SET()` for scalar
projection contexts. This slice extends existing JSON construction, extraction,
and introspection support without changing public API, catalog storage,
descriptor versions, file format, VFS behavior, or the SQLite fork.

The feature is intentionally limited to expression evaluation through
`mylite_execute()`:

- `SELECT JSON_SET(...)` without a source table;
- `SELECT JSON_SET(...) FROM DUAL`;
- `DO JSON_SET(...)`;
- single-table row-scalar `SELECT JSON_SET(...) FROM table` with the existing
  descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT` subsets.

It does not add JSON mutation functions other than `JSON_SET()`, DML assignment
expressions, generated-column/index interactions, partial JSON storage updates,
`CAST(... AS JSON)`, JSON_TABLE, subqueries as arguments, aggregate usage, joins
outside the already admitted row-scalar shape, or arbitrary expression
arguments.

## Compatibility Authority

Compatibility is based on:

- MySQL 8.4 Reference Manual, "Functions That Modify JSON Values":
  <https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_json_set_function_expectations.sh`.

Observed MySQL 8.4.9 behavior used by this slice:

- `JSON_SET(json_doc, path, val[, path, val]...)` requires one document plus one
  or more path/value pairs, so the total argument count is odd and at least 3.
- `json_doc` or any `path` evaluating to SQL `NULL` returns SQL `NULL`.
- SQL `NULL` used as a value becomes JSON `null`.
- Path/value pairs apply left to right. Later pairs see earlier changes.
- The root path `$` replaces the whole document.
- Existing object members and array elements are replaced.
- A missing object member in an existing object is added.
- A final array index at or past the current array length appends one value.
- A final array index against a non-array value replaces the value when the
  index is 0 and autowraps the old value in an array plus the new value when the
  index is greater than 0.
- Missing intermediate parents are ignored.
- SQL string values are inserted as JSON strings; JSON descriptor values and
  JSON-producing supported function results are inserted as JSON values.
- Invalid JSON document text raises MySQL error 3141 / SQLSTATE 22032.
- Invalid JSON path text raises MySQL error 3143 / SQLSTATE 42000.
- Paths with wildcard/range forms are outside this slice and are rejected with a
  deterministic MyLite unsupported-feature diagnostic.

## Architecture

Ownership boundaries stay the same as in the existing JSON expression slices:

- Public API: no new symbols and no ABI changes. Results use existing scalar
  result and non-row `DO` conventions.
- Statement context: unchanged; the feature observes selected schema only
  through existing row-scalar source planning.
- Lexer/parser/AST: recognize `JSON_SET` as a nonreserved function name and
  build a MyLite AST node containing the ordinary function argument list.
- Analyzer/planner: validate argument count and admitted argument shapes. Resolve
  descriptor column references from MyLite catalog descriptors, not SQLite
  metadata.
- Catalog: read-only for this feature. No descriptor rows, descriptor versions,
  cache generations, or SQLite schema generation values change.
- Runtime JSON module: parse MyLite's baseline JSON document tree, apply
  `JSON_SET()` mutations, and emit normalized JSON text.
- SQLite physical storage: row-scalar table scans are still executed by SQLite.
  MyLite lowers `JSON_SET()` to a private deterministic scalar function
  `_mylite_json_set(...)`; SQLite invokes it per output row. MyLite does not
  materialize a table result set before applying the function.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload are
  not touched by this expression feature.

## Syntax

MyLite Lemon-syntax sketch:

```lemon
expression(A) ::= JSON_SET(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_SET_FUNCTION, B, R);
}

expression(A) ::= JSON_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR, NULL, R);
}
```

Runtime/planner validation requires `argument_count >= 3` and an odd argument
count. Any other count reports MySQL-compatible native function parameter count
error 1582 / SQLSTATE 42000.

## Admitted Arguments

Document argument:

- SQL string literal containing valid baseline JSON;
- SQL `NULL`;
- row-scalar descriptor column of JSON or nonbinary string family;
- nested `JSON_SET()` document arguments are admitted only where another current
  JSON function explicitly lists them.

Path arguments:

- SQL string literals;
- SQL `NULL`.

Path expressions admitted by the runtime are simple root/member/array paths:

- root `$`;
- unquoted ASCII members such as `$.a`;
- quoted ASCII members such as `$."a-b"`;
- nonnegative decimal array indexes such as `$[0]`, `$[1]`;
- nested combinations of these forms.

Wildcard, recursive wildcard, ranges, `last`, non-ASCII quoted path members, and
other path forms remain outside this slice. Unsupported paths fail
deterministically with a MyLite unsupported-feature diagnostic; malformed paths
fail with MySQL-style invalid path diagnostics.

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
  arguments. Recursive row-scalar `JSON_SET()` values and row-scalar
  `JSON_EXTRACT()` values are outside this slice.

Binary string, BIT, decimal/float, temporal, arbitrary arithmetic, subquery,
parameter, and user-variable value arguments are outside this slice unless they
already fold through one of the admitted supported scalar paths.

## Semantics

`JSON_SET()` parses the document once, applies each path/value pair in argument
order, then emits normalized JSON text using the existing MyLite JSON writer.

For a path pair:

- If the path is `$`, the entire document is replaced with the value.
- If every parent leg exists and the final leg identifies an existing object
  member or array element, that value is replaced.
- If the parent exists and the final leg is a missing object member, the member
  is added and the containing object is re-normalized using MyLite's existing
  JSON object key order.
- If the parent exists and the final leg is an array index, an index smaller
  than the array count replaces that element; an index at or beyond the count
  appends one value.
- If the final leg is an array index on a non-array value, index 0 replaces the
  value; indexes greater than 0 replace the value with an array containing the
  old value followed by the new value.
- If an intermediate parent is missing or the path tries to traverse through a
  scalar value before the final leg, the pair has no effect.

Successful scalar `SELECT` returns one JSON result column, nullable, with the
existing JSON result metadata. `DO JSON_SET(...)` returns no rows and sets
`ROW_COUNT()` to 0 with warning count 0. Successful scalar `SELECT` preserves the
existing `ROW_COUNT() = -1` and warning count 0 behavior.

## Diagnostics

The implementation reports:

- Incorrect argument count: 1582 / 42000, native function parameter count.
- Invalid document text: 3141 / 22032, invalid JSON text in argument 1 to JSON
  function.
- Invalid path text: 3143 / 42000, invalid JSON path expression.
- Invalid document data type: 3146 / 22032 when an admitted parsing path reaches
  a non-JSON, non-string document value.
- Unsupported path shape: MyLite unsupported-feature diagnostic.
- Unsupported value or document argument shape: MyLite unsupported-feature
  diagnostic.
- Binary JSON argument where unsupported by current JSON policy: existing MyLite
  JSON binary-character-set diagnostic.
- Allocation failure: existing no-memory diagnostic.
- Physical SQLite scalar callback misuse/failure: existing runtime error mapping.

## Tests

Add a dedicated C runtime test and a MySQL expectation script covering:

- no-source, DUAL, and DO use;
- root replacement, member replacement, member insertion, nested member
  insertion, missing intermediate parent no-op, array replacement, array append,
  scalar autowrap, duplicate paths, SQL string values, JSON values, booleans,
  integers, and SQL NULL value/document/path behavior;
- table-backed row-scalar projection with JSON, integer, boolean-as-integer,
  nonbinary string, and nullable values;
- `WHERE`, `ORDER BY`, and `LIMIT` in the existing row-scalar subset;
- reopen persistence of the source rows and lack of source mutation;
- diagnostics for incorrect argument counts, invalid document, invalid path,
  unsupported wildcard path, non-JSON document data type, unsupported value
  expressions, unknown columns, and binary columns;
- parser coverage for the new AST node and argument-count node;
- compatibility docs showing `JSON_SET()` as partial/limited.
