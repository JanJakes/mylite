# Baseline JSON Contains Functions

This phase adds a narrow descriptor-driven baseline for the JSON search
predicates:

```sql
JSON_CONTAINS(target, candidate)
JSON_CONTAINS(target, candidate, path)
JSON_CONTAINS_PATH(json_doc, one_or_all, path [, path] ...)
```

The feature builds on the existing MyLite JSON parser, simple JSON path subset,
row-scalar expression planner, and SQLite scalar-function registration. It does
not add SQLite JSON1 as a dependency and does not require a SQLite fork patch.

## Compatibility Authority

The normative references are the official MySQL 8.4 Reference Manual pages for
JSON search functions and JSON path syntax:

- https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/json.html

Runtime expectations are verified against MySQL 8.4.9 in
`packages/libmylite/tests/mysql_baseline_json_contains_functions_expectations.sh`.

Observed MySQL 8.4.9 behavior for this slice:

- `JSON_CONTAINS()` accepts exactly two or three arguments. Wrong arity fails
  with `1582 / 42000`.
- `JSON_CONTAINS_PATH()` accepts at least three arguments. Wrong arity fails
  with `1582 / 42000`.
- SQL `NULL` propagation is argument-order sensitive:
  - `JSON_CONTAINS(NULL, 'bad')` returns `NULL` without validating the
    candidate.
  - `JSON_CONTAINS('bad', NULL)` fails on invalid target JSON.
  - `JSON_CONTAINS('{}', 'bad', NULL)` fails on invalid candidate JSON.
  - `JSON_CONTAINS_PATH(NULL, 'bad', 'bad')` returns `NULL`.
  - `JSON_CONTAINS_PATH('bad', 'one', NULL)` fails on invalid document JSON.
  - `JSON_CONTAINS_PATH('bad', 'some', 'bad')` fails on invalid document JSON
    before validating `one_or_all`.
  - `JSON_CONTAINS_PATH('{"a":1}', NULL, 'bad')` returns `NULL`.
- SQL numeric JSON document arguments fail with `3146 / 22032`; JSON document
  arguments must be string or JSON values in this baseline.
- Invalid JSON text fails with `3141 / 22032`. Invalid simple path syntax fails
  with `3143 / 42000`.
- `JSON_CONTAINS()` rejects path wildcards with `3149 / 42000` in MySQL. MyLite
  keeps wildcard/range/recursive paths outside this baseline and reports a
  deterministic unsupported diagnostic for those forms.
- `JSON_CONTAINS_PATH()` accepts MySQL wildcard paths, but MyLite defers them to
  the broader JSON path phase and reports deterministic unsupported diagnostics.
- `one_or_all` is case-insensitive for `'one'` and `'all'`; other non-`NULL`
  values fail with `3154 / 42000`.
- Supported successful calls report `warning_count == 0`.

## Scope

Supported:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar calls;
- single-table row-scalar projection over persistent descriptor tables;
- single-table `WHERE` truth predicates, integer/boolean comparisons, and
  `IS [NOT] NULL` predicates using the existing row-scalar predicate envelope;
- JSON document arguments from SQL string literals, `NULL`, JSON descriptor
  columns, and nonbinary string descriptor columns;
- simple JSON path string literals and `NULL` path literals;
- `JSON_CONTAINS()` containment for the current parsed JSON value subset:
  `null`, booleans, strings, signed 64-bit integer numbers, arrays, and objects;
- `JSON_CONTAINS_PATH()` path existence over the same simple path subset;
- result metadata through the existing scalar result conventions: integer
  `1`/`0` text for non-`NULL` results, SQL `NULL` for unknown results, no row
  metadata changes for `DO`, and warning count `0`.

Deferred:

- wildcard paths, recursive descent, array ranges, `last`, filters, non-ASCII
  path members, and multi-match path semantics;
- JSON decimal and exponent-number containment, beyond the current signed
  integer parser subset;
- binary string document inputs, `CAST(... AS JSON)`, and protocol-grade JSON
  metadata;
- arbitrary expression arguments, subqueries inside function arguments,
  parameters, variables, generated columns, expression indexes, grouping,
  ordering by JSON search results, and DML assignment expressions;
- JSON mutation functions and multi-valued index optimization.

## Semantics

`JSON_CONTAINS(target, candidate[, path])`:

1. Evaluate and decode the target argument. If it is SQL `NULL`, return SQL
   `NULL`.
2. Parse the target JSON text using the MyLite JSON parser.
3. Evaluate and decode the candidate argument. If it is SQL `NULL`, return SQL
   `NULL`.
4. Parse the candidate JSON text using the MyLite JSON parser.
5. If a path argument is present, evaluate and decode it. If it is SQL `NULL`,
   return SQL `NULL`. Otherwise validate it against the MyLite simple path
   subset and select the target subdocument. If the path does not match, return
   SQL `NULL`.
6. Return `1` if the candidate is contained in the target or selected
   subdocument, otherwise `0`.

Containment rules implemented by MyLite:

- a candidate scalar is contained in a target scalar when they have the same
  parsed kind and equal normalized value text or boolean value;
- a candidate array is contained in a target array when every candidate element
  is contained in at least one target element; duplicate candidate values do not
  require duplicate target values, matching MySQL's documented rule;
- a candidate nonarray is contained in a target array when it is contained in
  at least one target array element;
- a candidate object is contained in a target object when every candidate key
  exists in the target object and the candidate member value is contained in the
  corresponding target member value;
- all other kind combinations return `0`.

`JSON_CONTAINS_PATH(json_doc, one_or_all, path [, path] ...)`:

1. Evaluate and decode the document argument. If it is SQL `NULL`, return SQL
   `NULL`.
2. Parse the document JSON text.
3. Evaluate and decode `one_or_all`. If it is SQL `NULL`, return SQL `NULL`.
   The accepted values are ASCII case-insensitive `one` and `all`; other
   non-`NULL` values fail with the MySQL one-or-all diagnostic.
4. Evaluate and decode path arguments in order. If any path argument is SQL
   `NULL`, return SQL `NULL`. Otherwise validate each path against the MyLite
   simple path subset and test whether it matches a value in the document.
5. For `one`, return `1` if at least one path matches. For `all`, return `1`
   only if every path matches. Otherwise return `0`.

## Grammar

The MyLite grammar additions are independently authored and intentionally small:

```lemon
json_contains_expression(A) ::= JSON_CONTAINS(T) LPAREN expression(B) COMMA
                                expression(C) RPAREN(R).
json_contains_expression(A) ::= JSON_CONTAINS(T) LPAREN expression(B) COMMA
                                expression(C) COMMA expression(D) RPAREN(R).
json_contains_path_expression(A) ::= JSON_CONTAINS_PATH(T) LPAREN
                                     function_argument_list(B) RPAREN(R).

predicate_atom(A) ::= json_contains_expression(B).
predicate_atom(A) ::= json_contains_path_expression(B).
predicate_atom(A) ::= json_contains_expression(B) comparison_operator(O)
                      predicate_integer_value(V).
predicate_atom(A) ::= json_contains_path_expression(B) comparison_operator(O)
                      predicate_integer_value(V).
predicate_atom(A) ::= json_contains_expression(B) IS(I) [NOT] NULL(N).
predicate_atom(A) ::= json_contains_path_expression(B) IS(I) [NOT] NULL(N).

identifier(A) ::= JSON_CONTAINS(T).
identifier(A) ::= JSON_CONTAINS_PATH(T).
```

Wrong arity nodes are admitted so runtime diagnostics can report the native
function name consistently.

## Architecture

- Public API: no ABI changes. Successful scalar `SELECT` returns rows through
  the existing result API; successful `DO` returns a non-row result with
  `affected_rows == 0` and `warning_count == 0`.
- Parser/AST: adds nonreserved function tokens and AST function/error nodes.
- Analyzer/planner: resolves row-scalar column arguments from MyLite descriptors
  and rejects unknown columns before generated SQLite SQL is built.
- Runtime JSON module: owns JSON parsing, path selection, containment, and path
  existence semantics.
- SQLite integration: registers `_mylite_json_contains` and
  `_mylite_json_contains_path` scalar functions using public SQLite APIs. SQLite
  scans descriptor-owned physical rows and invokes MyLite scalar callbacks per
  row; MyLite does not materialize whole result sets for these predicates.
- Catalog/storage/VFS: unchanged. JSON search calls do not mutate descriptors,
  catalog generation, physical schema, `.mylite` preamble, or SQLite payload
  layout.

Generated SQLite SQL is descriptor-driven. Function arguments are emitted as
quoted physical column names or bound parameters via the existing row-scalar
planner; SQL literals are not interpolated into generated physical SQL.

## Diagnostics

- Wrong argument count: `1582 / 42000`, native function names
  `JSON_CONTAINS` and `JSON_CONTAINS_PATH`.
- Invalid JSON document/candidate text: `3141 / 22032` where MyLite can identify
  the JSON function text diagnostic; row-backed SQLite callback failures may
  map to the existing generic JSON function text diagnostic.
- Invalid JSON data type: `3146 / 22032`.
- Invalid path syntax: `3143 / 42000`.
- Unsupported path expression or JSON number shape: deterministic MyLite
  unsupported diagnostic.
- Invalid `one_or_all`: `3154 / 42000`.
- Binary string JSON input: `3144 / 22032`.
- Unknown descriptor column: existing MySQL-style unknown-column diagnostic.
- Allocation failure: existing out-of-memory diagnostic.

## Tests

Add fast C coverage under
`packages/libmylite/tests/runtime_json_contains_functions_test.c` and register
it as `libmylite.runtime.json_contains_functions`.

Coverage includes:

- literal scalar/object/array containment and simple path containment;
- path existence for `one`, `all`, case-insensitive modes, misses, and `NULL`;
- no-source, `DUAL`, and `DO` calls;
- table-backed JSON and `VARCHAR` columns, `WHERE` truth predicates, comparison
  predicates, `IS NULL`, close/reopen persistence, and independent handles;
- labels, affected rows, warning count, and absence of result rows for `DO`;
- wrong arity, invalid JSON, invalid path, unsupported wildcard path, invalid
  data type, invalid `one_or_all`, binary input, unknown columns, and unsupported
  arbitrary expressions.

Existing JSON, parser, runtime lifecycle, file-backed opening, VFS, and
compatibility tests must continue to pass.
