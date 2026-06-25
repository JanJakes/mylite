# Baseline JSON Array Mutation Functions

## Goal

Add a narrow MySQL-compatible baseline for `JSON_ARRAY_APPEND()` and
`JSON_ARRAY_INSERT()` in the current JSON mutation execution envelope.

The admitted contexts are:

- `SELECT` without a source table;
- `SELECT ... FROM DUAL`;
- `DO`;
- single-table row-scalar projection with the existing descriptor-driven
  `WHERE`, `ORDER BY`, and `LIMIT` subset;
- compatible non-key single-table `UPDATE` assignments that already admit the
  current JSON mutation family.

This slice does not add stored partial JSON updates, arbitrary expression
arguments, predicates over JSON mutation results, grouping expressions, joins
beyond the existing row-scalar envelope, binary JSON storage, full JSON path
grammar, or SQLite fork changes.

## Compatibility Authority

Compatibility is based on:

- MySQL 8.4 Reference Manual, "Functions That Modify JSON Values":
  <https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_json_array_mutation_functions_expectations.sh`.

Observed MySQL 8.4.9 behavior used by this slice:

- Both functions require a JSON document plus one or more path/value pairs, so
  the total argument count is odd and at least `3`.
- A SQL `NULL` document returns SQL `NULL`.
- A SQL `NULL` path returns SQL `NULL`, after validating the document and all
  earlier non-`NULL` paths. Later paths are not validated.
- A SQL `NULL` value becomes JSON `null`.
- Path/value pairs are evaluated left to right, and each pair observes the
  document produced by earlier pairs.
- Invalid JSON document text raises MySQL error `3141 / 22032`.
- Non-string, non-JSON document values raise MySQL error `3146 / 22032`.
- Invalid path text raises MySQL error `3143 / 42000`.
- Path wildcard, recursive wildcard, and range forms raise MySQL error
  `3149 / 42000`.

`JSON_ARRAY_APPEND()` behavior:

- If a path identifies an array, the value is appended to that array.
- If a path identifies a scalar or object, that selected value is autowrapped as
  an array and the new value is appended.
- The root path `$` selects the full document and can autowrap it.
- If the path does not identify an existing value, the pair is ignored.

`JSON_ARRAY_INSERT()` behavior:

- Every non-`NULL` path must syntactically end in an array-cell leg. Paths such
  as `$`, `$.a`, or `$[0].a` raise MySQL error `3165 / 42000`.
- If the parent path identifies an array, the value is inserted at the requested
  index and later array elements shift right.
- If the requested index is at or past the current array length, the value is
  appended to the array.
- If the parent path is missing or does not identify an array, the pair is
  ignored.
- Earlier insertions can change later path positions.

## Architecture

Ownership boundaries:

- Lexer/parser/AST: add nonreserved function tokens and dedicated AST node
  kinds for the two functions and their zero-argument markers.
- Analyzer/planner: reuse the existing JSON mutation planning path with new
  planned mutation kinds.
- Runtime JSON module: extend the existing MyLite JSON DOM mutation mode enum
  and path application helpers. `JSON_ARRAY_INSERT()` adds one normalize status
  for paths that are not array-cell paths.
- SQLite row execution: lower row-scalar calls to private deterministic scalar
  UDFs `_mylite_json_array_append(...)` and `_mylite_json_array_insert(...)`.
  SQLite still drives table scans and expression invocation.
- Catalog, storage, VFS, and SQLite fork: unchanged. No new SQLite fork hook is
  needed because the current public scalar function API already provides the
  required execution surface.

## Syntax

MyLite Lemon-syntax sketch:

```lemon
expression(A) ::= JSON_ARRAY_APPEND(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION, B, R);
}

expression(A) ::= JSON_ARRAY_INSERT(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION, B, R);
}

expression(A) ::= JSON_ARRAY_APPEND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_APPEND_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= JSON_ARRAY_INSERT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_JSON_ARRAY_INSERT_ARGUMENT_COUNT_ERROR, NULL, R);
}

identifier(A) ::= JSON_ARRAY_APPEND(T).
identifier(A) ::= JSON_ARRAY_INSERT(T).
```

Runtime validation requires `argument_count >= 3` and an odd argument count.
Any other count reports MySQL-compatible native function parameter-count error
`1582 / 42000`.

## Admitted Arguments

Document argument:

- SQL string literal containing valid baseline JSON;
- SQL `NULL`;
- row-scalar descriptor column of JSON or nonbinary string family;
- supported nested JSON mutation results where current JSON introspection and
  mutation argument paths already admit them.

Path arguments:

- SQL string literals;
- SQL `NULL`.

The path grammar reuses the current simple JSON mutation path subset:

- root `$`;
- unquoted ASCII members such as `$.a`;
- quoted ASCII members such as `$."a-b"`;
- nonnegative decimal array indexes such as `$[0]`;
- nested combinations of these forms.

Wildcard, recursive wildcard, ranges, `last`, filters, non-ASCII member names,
and other path forms remain outside this slice.

Value arguments:

- SQL string literals, inserted as JSON strings;
- signed integer literals within the signed 64-bit range;
- `TRUE` and `FALSE` literals, inserted as JSON booleans;
- SQL `NULL`, inserted as JSON `null`;
- row-scalar descriptor columns of JSON, integer, boolean-like integer, and
  nonbinary string family;
- no-source scalar evaluation accepts nested supported JSON-producing
  expressions where the existing JSON mutation path already admits them,
  including current `JSON_EXTRACT()`, `JSON_ARRAY()`, and `JSON_OBJECT()`
  results;
- row-scalar lowering accepts `JSON_ARRAY()` and `JSON_OBJECT()` as JSON value
  arguments.

Binary strings, decimal/float/temporal values, path columns, arbitrary
expression arguments, recursive mutation values, and row-scalar
`JSON_EXTRACT()` value arguments remain outside this slice.

## Result Metadata

Result columns use the existing limited JSON mutation metadata:

- nullable result;
- JSON logical result type;
- current scalar/row-scalar column naming behavior.

No protocol metadata beyond the existing JSON mutation baseline is added.

## Runtime And Error Handling

- SQL `NULL` document or path returns SQL `NULL` after the MySQL-observed
  validation order.
- Non-`NULL` path/value pairs apply left to right.
- `JSON_ARRAY_APPEND()` autowraps selected nonarray values and ignores missing
  paths.
- `JSON_ARRAY_INSERT()` validates each non-`NULL` path as an array-cell path
  before applying it; invalid cell paths report `3165 / 42000`.
- Unsupported wildcard/range paths currently report MyLite's deterministic
  unsupported JSON mutation diagnostic for this function family, consistent
  with the existing JSON mutation baseline. Matching MySQL's `3149 / 42000`
  path-wildcard diagnostic is deferred to the full JSON path grammar work.
- Binary-string document and path values reuse the current JSON binary charset
  diagnostics.

## Tests

The MySQL expectation script covers:

- literal object, array, scalar, root, missing-path, and left-to-right cases;
- SQL `NULL` document/path/value behavior;
- row count and warning count after `SELECT` and `DO`;
- row-scalar descriptor projection and composition with current JSON
  introspection functions;
- diagnostics for arity, invalid document, invalid document type, invalid path,
  wildcard path, array-cell path errors, and validation-before-`NULL` ordering.

Runtime C tests mirror those expectations against MyLite and add metadata and
nonmutation checks for source rows. Parser tests verify AST node construction,
zero-argument markers, `DO`, lower-case function names, and identifier use.

## Known Incompatibilities

- No full JSON path grammar.
- No binary JSON storage or partial-update storage accounting.
- No stored partial update optimization.
- No arbitrary expression context support beyond the current scalar and
  row-scalar envelope.
- No SQLite fork changes.
