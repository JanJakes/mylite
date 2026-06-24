# Baseline JSON_OVERLAPS and MEMBER OF Functions

## Status

This feature specifies the baseline `JSON_OVERLAPS()` function and
`MEMBER OF()` JSON membership operator. The slice extends the existing MyLite
JSON search predicate family with MySQL 8.4.9-compatible overlap and membership
checks over the JSON value subset already supported by the MyLite JSON DOM.

The feature is intentionally not full MySQL JSON support. It does not add
binary JSON storage, `CAST(... AS JSON)`, multi-valued index optimization,
decimal/exponent JSON number parsing beyond the current MyLite parser subset,
or arbitrary expression support outside existing scalar and single-table
row-scalar JSON contexts.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing JSON search baseline:
  `docs/specs/baseline-json-contains-functions/specs.md`
- MySQL 8.4 Reference Manual, JSON search functions:
  https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser, AST, and keyword-token support for `JSON_OVERLAPS(json_doc,
  json_doc)`;
- parser and AST support for `expression MEMBER OF (expression)`;
- native-function argument-count diagnostics for `JSON_OVERLAPS()`;
- MySQL syntax diagnostics for malformed `MEMBER OF()` calls;
- scalar/no-source execution for `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar execution through private SQLite callbacks for
  projections and admitted predicate/comparison contexts;
- MySQL-equivalent `NULL`, invalid JSON text, invalid JSON data type, and
  binary character-set diagnostics for the documented baseline contexts;
- parser, runtime, MySQL expectation, compatibility-matrix, and detailed JSON
  compatibility documentation updates.

## Non-Goals

This feature must not implement:

- binary JSON storage or MySQL binary JSON internals;
- `CAST(... AS JSON)`;
- multi-valued index planning or optimizer behavior;
- full JSON decimal/exponent parsing beyond the current MyLite parser scope;
- JSON path extensions;
- writable server JSON metadata;
- protocol metadata parity outside MyLite's current scalar and row-scalar
  result metadata layer;
- arbitrary-expression contexts not admitted by the existing JSON row-scalar
  planner.

## MySQL 8.4.9 Behavior

`JSON_OVERLAPS(doc1, doc2)` returns `NULL` if either argument is SQL `NULL`.
Both non-`NULL` arguments must be JSON strings or JSON values. SQL numeric
arguments fail with `3146` / `22032`; invalid JSON text fails with `3141` /
`22032`; binary string arguments fail with the existing JSON binary character
set diagnostic.

Overlap rules observed against MySQL 8.4.9:

- equal scalar JSON values overlap;
- unequal scalar JSON values do not overlap;
- two arrays overlap when any element is equal;
- an array and a nonarray JSON value overlap when the nonarray value equals an
  array element;
- two objects overlap when they contain at least one member with the same key
  and an equal value;
- object member values are compared by JSON equality, not containment, so
  `{"a":{"b":2}}` and `{"a":{"b":2,"c":3}}` do not overlap;
- empty arrays and empty objects do not overlap.

`value MEMBER OF(json_doc)` returns `NULL` if either operand is SQL `NULL`.
The right operand must be a JSON string or JSON value. SQL numeric right
operands fail with `3146` / `22032`, invalid JSON text fails with `3141` /
`22032`, and binary string right operands fail with the JSON binary character
set diagnostic.

The left operand is compared as a JSON value:

- SQL integer values compare as JSON numbers;
- SQL nonbinary strings compare as JSON strings and are not parsed as JSON
  documents;
- JSON-producing expressions such as `JSON_ARRAY()` and `JSON_OBJECT()`
  compare as their produced JSON documents;
- no string-number conversion is performed;
- if the right operand is an array, the result is `1` when any array element is
  JSON-equal to the left value;
- if the right operand is not an array, the result is `1` when the right value
  itself is JSON-equal to the left value.

Observed examples:

- `JSON_OVERLAPS('1','1')` returns `1`;
- `JSON_OVERLAPS('[1,2]','[2,3]')` returns `1`;
- `JSON_OVERLAPS('{"a":1}','{"a":2}')` returns `0`;
- `JSON_OVERLAPS('{"a":{"b":2}}','{"a":{"b":2,"c":3}}')` returns `0`;
- `17 MEMBER OF('[23, "abc", 17, "ab", 10]')` returns `1`;
- `'17' MEMBER OF('[23, "abc", 17, "ab", 10]')` returns `0`;
- `'[4,5]' MEMBER OF('[[4,5]]')` returns `0`;
- `JSON_ARRAY(4,5) MEMBER OF('[[4,5]]')` returns `1`;
- `1 MEMBER OF('1')` returns `1`.

## Lemon Syntax

The intended MyLite Lemon syntax is:

```lemon
json_overlaps_expression(A) ::= JSON_OVERLAPS(T) LPAREN expression(B)
                                COMMA expression(C) RPAREN(R).
member_of_expression(A) ::= expression(B) MEMBER(T) OF LPAREN expression(C) RPAREN(R).

expression(A) ::= json_overlaps_expression(B).
expression(A) ::= member_of_expression(B).

predicate_atom(A) ::= predicate_scalar_literal(B) MEMBER(T) OF LPAREN
                      expression(C) RPAREN(R).
predicate_atom(A) ::= literal_left_comparison_value(B) MEMBER(T) OF LPAREN
                      expression(C) RPAREN(R).
predicate_atom(A) ::= qualified_identifier(B) MEMBER(T) OF LPAREN
                      expression(C) RPAREN(R).

expression(A) ::= JSON_OVERLAPS(T) LPAREN RPAREN(R).
expression(A) ::= JSON_OVERLAPS(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= JSON_OVERLAPS(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R).

identifier(A) ::= JSON_OVERLAPS(T).
identifier(A) ::= MEMBER(T).
```

`JSON_OVERLAPS()` is admitted through the existing JSON predicate family.
`MEMBER OF()` is admitted in expression contexts and in bounded predicate
contexts for scalar literals, string literals, and descriptor identifiers.

## Runtime Design

The JSON DOM layer exposes:

- `mylite_json_overlaps()`, implemented by parsing both JSON documents and
  checking scalar equality, array-element intersection, object shared key/value
  equality, and array/nonarray element equality;
- `mylite_json_member_of()`, implemented by parsing the right JSON document,
  converting the left SQL scalar or JSON-producing expression to a JSON value,
  and checking equality against either the right array elements or the right
  value itself.

The SQLite private callback layer registers `_mylite_json_overlaps` and
`_mylite_json_member_of` as deterministic, innocuous, direct-only scalar
functions. Row-backed expressions are planned to these callbacks and executed
by SQLite scans or filters; MyLite must not materialize whole tables for these
predicates.

No SQLite fork hook is required for this slice. Public SQLite scalar UDFs and
the existing row-scalar SQL translation boundary are sufficient.

## Diagnostics

- Wrong `JSON_OVERLAPS()` argument count: error `1582` / SQLSTATE `42000`.
- Malformed `MEMBER OF()` syntax: existing parser syntax diagnostic.
- Invalid JSON document text: error `3141` / SQLSTATE `22032`.
- Invalid JSON document data type: error `3146` / SQLSTATE `22032`.
- Binary string JSON input for document operands: existing `3144` / `22032`
  JSON binary character-set diagnostic.
- Unsupported JSON document number shape or excessive nesting: deterministic
  MyLite unsupported diagnostic.
- Unknown descriptor column: existing MySQL-style unknown-column diagnostic.
- Allocation failure: existing out-of-memory diagnostic.

## Tests

Expected behavior must be verified against a real MySQL 8.4.9 runtime in
`packages/libmylite/tests/mysql_baseline_json_overlaps_member_functions_expectations.sh`.

MyLite coverage must include:

- literal scalar, array, object, nested object, array/nonarray, empty container,
  and `NULL` overlap cases;
- literal numeric/string/JSON-constructor membership cases including no
  string-number conversion and nonarray right operand comparison;
- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` calls;
- table-backed JSON and `VARCHAR` columns, `WHERE` truth predicates,
  comparison predicates, `IS NULL`, and close/reopen persistence;
- labels, aliases, affected rows, warning count, and absence of result rows for
  `DO`;
- wrong `JSON_OVERLAPS()` arity, malformed `MEMBER OF()` syntax, invalid JSON,
  invalid data types, binary right/document operands, unknown columns, and
  unsupported arbitrary expressions.

Existing JSON, parser, runtime lifecycle, file-backed opening, VFS, and
compatibility tests must continue to pass.
