# Baseline JSON_REMOVE Function

## Goal

Add a narrow, descriptor-aware implementation of MySQL `JSON_REMOVE()` for scalar
projection contexts. This slice builds on the current MyLite JSON path/document
infrastructure used by `JSON_SET()` and `JSON_REPLACE()` while keeping storage,
catalog, public API, and SQLite fork boundaries unchanged.

The admitted execution contexts are:

- `SELECT JSON_REMOVE(...)` without a source table;
- `SELECT JSON_REMOVE(...) FROM DUAL`;
- `DO JSON_REMOVE(...)`;
- single-table row-scalar `SELECT JSON_REMOVE(...) FROM table` with the
  existing descriptor-driven `WHERE`, `ORDER BY`, and `LIMIT` subsets.

This feature does not add stored partial JSON updates, DML assignment
expressions, `JSON_INSERT()`, `JSON_ARRAY_INSERT()`, arbitrary expression
arguments, predicates over JSON mutation results, ordering/grouping expressions,
joins beyond the existing row-scalar envelope, or SQLite fork changes.

## Compatibility Authority

Compatibility is based on:

- MySQL 8.4 Reference Manual, "Functions That Modify JSON Values":
  <https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_json_remove_function_expectations.sh`.

Observed MySQL 8.4.9 behavior used by this slice:

- `JSON_REMOVE(json_doc, path[, path]...)` requires a document plus one or more
  paths, so the total argument count is at least 2.
- `json_doc` evaluating to SQL `NULL` returns SQL `NULL` without path
  validation.
- A path argument evaluating to SQL `NULL` returns SQL `NULL` after the
  document and any earlier paths have been parsed for syntactic validity; later
  paths are not validated. Semantic removal checks such as root-path rejection
  are not applied before this `NULL` return.
- Paths apply left to right. Later paths observe earlier removals.
- Existing object members and array elements are removed.
- Missing object members, missing array elements, and missing intermediate
  parents are ignored.
- An intermediate array index against a non-array treats that value as a
  single-element array for index `0`; any other index is missing and has no
  effect.
- A final array index against a scalar document or scalar member is ignored.
- The root path `$` is rejected with MySQL error `3153 / 42000`.
- Invalid JSON document text raises MySQL error `3141 / 22032`.
- Invalid JSON path text raises MySQL error `3143 / 42000`.
- Wildcard or range path forms are outside this slice and are rejected with the
  existing deterministic MyLite unsupported-path policy.

## Architecture

Ownership boundaries:

- Public API: unchanged. Results use existing scalar result and non-row `DO`
  conventions.
- Statement context: unchanged. Selected schema matters only through existing
  row-scalar source planning.
- Lexer/parser/AST: recognize `JSON_REMOVE` as a nonreserved function name and
  build a MyLite AST node containing the ordinary function argument list.
- Analyzer/planner: validate argument count and admitted argument shapes.
  Descriptor column references resolve through MyLite catalog descriptors, not
  SQLite metadata.
- Catalog: read-only for this feature. No descriptor rows, descriptor versions,
  cache generations, or SQLite schema generation values change.
- Runtime JSON module: reuse the baseline JSON document tree, parse the current
  simple path subset, apply removal-only mutation, and emit normalized JSON
  text.
- SQLite physical execution: row-scalar table scans remain SQLite-driven.
  MyLite lowers row-scalar `JSON_REMOVE()` to a private deterministic scalar
  function `_mylite_json_remove(...)`; SQLite invokes it per output row.
- Storage/VFS/SQLite fork: unchanged. The `.mylite` preamble, shifted SQLite
  payload, VFS, and SQLite fork patch stack are not touched.

## Syntax

MyLite Lemon-syntax sketch:

```lemon
expression(A) ::= JSON_REMOVE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_REMOVE_FUNCTION, B, R);
}

expression(A) ::= JSON_REMOVE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR, NULL, R);
}

identifier(A) ::= JSON_REMOVE(T).
```

Runtime validation requires `argument_count >= 2`. Any smaller count reports
MySQL-compatible native function parameter-count error `1582 / 42000`.

## Admitted Arguments

Document argument:

- SQL string literal containing valid baseline JSON;
- SQL `NULL`;
- row-scalar descriptor column of JSON or nonbinary string family;
- nested `JSON_REMOVE()` document arguments are admitted only where another
  current JSON function explicitly lists current JSON mutation results.

Path arguments:

- SQL string literals;
- SQL `NULL`.

Path expressions admitted by the runtime reuse the current simple JSON mutation
path subset:

- unquoted ASCII members such as `$.a`;
- quoted ASCII members such as `$."a-b"`;
- nonnegative decimal array indexes such as `$[0]`, `$[1]`;
- nested combinations of these forms.

The root path `$` parses but is rejected for `JSON_REMOVE()`. Wildcard,
recursive wildcard, ranges, `last`, non-ASCII quoted path members, and other
path forms remain outside this slice.

## Semantics

`JSON_REMOVE()` parses the document once, applies each path in argument order,
then emits normalized JSON text using the existing MyLite JSON writer.

If a path argument is SQL `NULL`, MyLite parses the document and earlier path
text but returns SQL `NULL` without applying removals or root-path rejection.

For each path:

- If the path is `$`, the statement fails with MySQL error `3153 / 42000`.
- If every parent leg exists and the final leg identifies an existing object
  member, that member is removed.
- If every parent leg exists and the final leg identifies an existing array
  element, that element is removed and later elements shift left.
- If any parent leg is missing, the path has no effect.
- If the final member leg is missing from an existing object, the path has no
  effect.
- If the final array index is at or beyond the current array length, the path
  has no effect.
- If the final array index targets a non-array value, the path has no effect.

Successful scalar `SELECT` returns one JSON result column, nullable, with the
existing JSON result metadata. `DO JSON_REMOVE(...)` returns no rows and sets
`ROW_COUNT()` to `0` with warning count `0`. Successful scalar `SELECT`
preserves the existing `ROW_COUNT() = -1` and warning count `0` behavior.

## Diagnostics

The implementation reports:

- Incorrect argument count: `1582 / 42000`, native function parameter count.
- Invalid document text: `3141 / 22032`, invalid JSON text in argument 1 to
  JSON function.
- Invalid path text: `3143 / 42000`, invalid JSON path expression.
- Root path `$`: `3153 / 42000`, path expression not allowed in this context.
- Invalid document data type: `3146 / 22032` when an admitted parsing path
  reaches a non-JSON, non-string document value.
- Unsupported path shape: MyLite unsupported-feature diagnostic aligned with
  the existing JSON mutation path policy.
- Unsupported document or path argument shape: MyLite unsupported-feature
  diagnostic.
- Binary JSON argument where unsupported by current JSON policy: existing
  MyLite JSON binary-character-set diagnostic.
- Allocation failure: existing no-memory diagnostic.
- Physical SQLite scalar callback misuse/failure: existing runtime error
  mapping.

## Tests

Add a dedicated C runtime test and a MySQL expectation script covering:

- no-source, `DUAL`, and `DO` use;
- object member removal, nested member removal, missing member no-op, missing
  intermediate parent no-op, array removal, out-of-range array no-op, scalar
  array-leg no-op, leading-zero array indexes, duplicate/left-to-right paths,
  and SQL `NULL` document/path behavior;
- table-backed row-scalar projection with JSON and nonbinary string documents,
  `NULL` path literals, path-`NULL` precedence against earlier document and
  path syntax errors, and existing `WHERE`, `ORDER BY`, and `LIMIT`;
- composition where current JSON extraction/type functions consume
  `JSON_REMOVE()` results;
- reopen persistence of source rows and lack of source mutation;
- diagnostics for incorrect argument counts, invalid document, invalid path,
  root path, unsupported wildcard path, non-JSON document data type, unknown
  columns, table-backed nonliteral paths when outside the row-scalar slice, and
  binary columns;
- parser coverage for the new AST node and argument-count node;
- compatibility docs showing `JSON_REMOVE()` as partial/limited.

## Compatibility Gaps

MyLite still does not claim full JSON mutation compatibility. Deferred behavior
includes stored partial JSON updates, recursive mutation values, `CAST(... AS
JSON)`, binary JSON storage, decimal/float/temporal JSON number handling,
non-ASCII path members, wildcard/range paths, predicates, ordering/grouping
expressions, DML assignment expressions, arbitrary nested expressions, and
protocol metadata beyond the existing limited scalar/row-scalar JSON metadata.
