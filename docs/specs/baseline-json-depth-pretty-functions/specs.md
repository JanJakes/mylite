# Baseline JSON_DEPTH and JSON_PRETTY Functions

## Status

This feature specifies the baseline `JSON_DEPTH()` and `JSON_PRETTY()`
compatibility slice. It extends MyLite's existing JSON introspection function
family with MySQL 8.4.9-compatible document-depth calculation and deterministic
human-readable JSON rendering.

The feature is intentionally not full MySQL JSON support. It keeps the existing
MyLite JSON document parser, object-member display ordering, JSON column
storage, scalar/no-source expression evaluation, and single-table row-scalar SQL
translation boundaries. It does not implement binary JSON storage, full MySQL
JSON number grammar beyond the currently supported subset, JSON path variants,
or collation/protocol metadata parity outside the established baseline function
contexts.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing JSON introspection baseline:
  `docs/specs/baseline-json-row-scalar-context-followup/specs.md`
- MySQL 8.4 Reference Manual, JSON attribute functions:
  https://dev.mysql.com/doc/refman/8.4/en/json-attribute-functions.html
- MySQL 8.4 Reference Manual, JSON utility functions:
  https://dev.mysql.com/doc/refman/8.4/en/json-utility-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser, AST, and keyword-token support for `JSON_DEPTH(json_doc)`;
- parser, AST, and keyword-token support for `JSON_PRETTY(json_doc)`;
- native-function argument-count diagnostics for zero or multiple arguments;
- scalar/no-source execution for `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar execution through private SQLite callbacks for
  projections, admitted predicates/order contexts, and compatible non-key
  `UPDATE` assignments wherever existing JSON introspection functions are
  accepted;
- `JSON_DEPTH()` result formatting as a signed decimal integer text cell;
- `JSON_PRETTY()` result formatting as a text cell containing MySQL-shaped JSON;
- MySQL-equivalent `NULL`, invalid JSON text, invalid JSON data type, and binary
  character-set diagnostics for the documented baseline contexts;
- parser, runtime, MySQL expectation, compatibility-matrix, and detailed JSON
  compatibility documentation updates.

## Non-Goals

This feature must not implement:

- binary JSON storage or MySQL binary JSON internals;
- JSON path variants for either function;
- writable server JSON metadata;
- full JSON decimal/exponent parsing beyond the current MyLite parser scope;
- collation/protocol metadata parity outside MyLite's current scalar and
  row-scalar result metadata layer;
- arbitrary-expression contexts not admitted by the existing JSON row-scalar
  planner.

## MySQL 8.4.9 Behavior

`JSON_DEPTH(json_doc)` returns `NULL` for SQL `NULL`. Scalars, empty objects,
and empty arrays have depth `1`. Non-empty arrays and objects return one plus
the maximum depth of their children. Observed examples:

- `JSON_DEPTH('{}')`, `JSON_DEPTH('[]')`, `JSON_DEPTH('true')`,
  `JSON_DEPTH('123')`, and `JSON_DEPTH('"x"')` return `1`;
- `JSON_DEPTH('[10,20]')` and `JSON_DEPTH('[[],{}]')` return `2`;
- `JSON_DEPTH('[10,{"a":20}]')` returns `3`;
- `JSON_DEPTH('{"a":{"b":[1]}}')` returns `4`.

`JSON_PRETTY(json_doc)` returns `NULL` for SQL `NULL`. Scalars are returned as
valid compact JSON scalar text. Empty arrays and objects remain `[]` and `{}`.
Non-empty arrays and objects are formatted with:

- newline after the opening bracket or brace;
- two spaces per nesting level;
- one element or member per line;
- comma followed by newline between elements or members;
- object members rendered as `"key": value`;
- closing bracket or brace aligned with the opening container's indentation.

MyLite already sorts object members to match the MySQL display order used by
the existing JSON normalization surface. `JSON_PRETTY('{"b":1,"a":2}')` must
therefore return:

```text
{
  "a": 2,
  "b": 1
}
```

For both functions:

- wrong argument count raises error `1582` / SQLSTATE `42000`;
- non-JSON scalar SQL values such as `1` raise error `3146` / SQLSTATE
  `22032`;
- invalid JSON text raises error `3141` / SQLSTATE `22032`;
- binary string input raises the existing MyLite/MySQL-compatible JSON binary
  character-set diagnostic.

## Lemon Syntax

The intended MyLite Lemon syntax is:

```lemon
expression(A) ::= JSON_DEPTH(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= JSON_PRETTY(T) LPAREN expression(B) RPAREN(R).

expression(A) ::= JSON_DEPTH(T) LPAREN RPAREN(R).
expression(A) ::= JSON_DEPTH(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).

expression(A) ::= JSON_PRETTY(T) LPAREN RPAREN(R).
expression(A) ::= JSON_PRETTY(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).

identifier(A) ::= JSON_DEPTH(T).
identifier(A) ::= JSON_PRETTY(T).
```

`JSON_DEPTH()` is also admissible in the row-scalar JSON predicate expression
nonterminal because it returns a numeric value. `JSON_PRETTY()` is a text
function and follows the projection/update/order contexts already admitted for
row-scalar JSON introspection functions.

## Runtime Design

The JSON DOM layer exposes:

- `mylite_json_depth()`, implemented by parsing the document and recursively
  computing the maximum child depth with parser nesting already capped at
  `json_max_nesting_depth`;
- `mylite_json_pretty()`, implemented by parsing the document and rendering it
  through the existing JSON writer with a recursive pretty emitter.

The SQLite private callback layer registers `_mylite_json_depth` and
`_mylite_json_pretty` as deterministic, innocuous, direct-only scalar
functions. Row-backed expressions are planned to these callbacks instead of
materializing table contents in MyLite memory.

No SQLite fork hook is required for this slice. Public SQLite scalar UDFs and
the existing row-scalar SQL translation boundary are sufficient.

## Tests

Expected behavior must be verified against a real MySQL 8.4.9 runtime in
`packages/libmylite/tests/mysql_baseline_json_introspection_functions_expectations.sh`.

MyLite coverage must include:

- parser AST nodes and argument-count marker nodes;
- no-source/`DUAL`/`DO` values and status counters;
- table-backed JSON columns, string columns, nested `JSON_EXTRACT()` results,
  and reopen persistence;
- row-scalar diagnostics for wrong argument counts, invalid data types, invalid
  text, binary strings, and missing columns;
- compatibility documentation rows for both the top-level matrix and detailed
  JSON function table.
