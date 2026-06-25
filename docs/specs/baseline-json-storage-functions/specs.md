# Baseline JSON Storage Functions

## Status

This feature specifies the baseline `JSON_STORAGE_SIZE()` and
`JSON_STORAGE_FREE()` compatibility slice. It extends the current MyLite JSON
introspection family with MySQL-shaped storage observability for the JSON
document subset MyLite already parses, stores, and exposes through scalar and
single-table row-scalar JSON function contexts.

The feature is intentionally not full MySQL binary JSON storage. MyLite stores
descriptor-owned JSON values as canonical SQLite `TEXT`, not as MySQL binary
JSON blobs, and it does not implement MySQL's partial in-place JSON update
optimization. The baseline therefore reports a MyLite-owned estimate of
MySQL's binary JSON layout for admitted documents and reports no freed partial
update space.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing JSON introspection baseline:
  `docs/specs/baseline-json-introspection-functions/specs.md`
- Existing JSON depth/pretty baseline:
  `docs/specs/baseline-json-depth-pretty-functions/specs.md`
- MySQL 8.4 Reference Manual, JSON data type and partial-update behavior:
  https://dev.mysql.com/doc/refman/8.4/en/json.html
- MySQL 8.4 Reference Manual, JSON utility functions:
  https://dev.mysql.com/doc/refman/8.4/en/json-utility-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser, AST, and keyword-token support for `JSON_STORAGE_SIZE(json_doc)`;
- parser, AST, and keyword-token support for `JSON_STORAGE_FREE(json_doc)`;
- native-function argument-count diagnostics for zero or multiple arguments;
- scalar/no-source execution for `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar execution through private SQLite callbacks wherever
  the existing one-argument JSON introspection functions are admitted;
- signed decimal integer text results with nullable `LONGLONG`-shaped metadata;
- `NULL`, invalid JSON text, invalid JSON data type, binary character-set, and
  missing-column diagnostics consistent with the current JSON introspection
  family;
- parser, runtime, MySQL expectation, compatibility-matrix, and detailed JSON
  compatibility documentation updates.

## Non-Goals

This feature must not implement:

- persisted MySQL binary JSON storage;
- partial in-place JSON updates or reusable free-space accounting;
- `JSON_STORAGE_FREE()` values above zero;
- full MySQL binary JSON layout for documents outside MyLite's current JSON DOM
  subset;
- general constructor, aggregate, variable, subquery, joined-source, grouped,
  or arbitrary expression contexts beyond the existing JSON introspection
  planner boundaries;
- a SQLite fork hook.

## MySQL 8.4.9 Behavior

`JSON_STORAGE_SIZE(json_doc)` returns `NULL` for SQL `NULL`. For valid
documents it returns the number of bytes used for MySQL's binary JSON storage
representation. Observed MySQL 8.4.9 examples:

- `JSON_STORAGE_SIZE('{}')` and `JSON_STORAGE_SIZE('[]')` return `5`;
- `JSON_STORAGE_SIZE('{"a":1}')` returns `13`;
- `JSON_STORAGE_SIZE('[1,2,3]')` returns `14`;
- `JSON_STORAGE_SIZE('"abc"')` returns `5`;
- table values such as `{"a":[1,2],"b":{"c":3}}` return `43`;
- `JSON_STORAGE_SIZE(JSON_EXTRACT('{"a":[1,2]}', '$.a'))` returns `11`.

`JSON_STORAGE_FREE(json_doc)` returns `NULL` for SQL `NULL`. MySQL reports
space freed by previous partial updates of a stored JSON column. Documents that
are not carrying reusable partial-update slack return `0`; this includes string
literal documents and JSON function results. Observed examples:

- `JSON_STORAGE_FREE('{}')` returns `0`;
- `JSON_STORAGE_FREE(JSON_SET('{"a":1}', '$.a', 2))` returns `0`;
- table values inserted or fully replaced without a tracked partial-update free
  region return `0`.

For both functions:

- wrong argument count raises error `1582` / SQLSTATE `42000`;
- non-JSON scalar SQL values such as `1` raise error `3146` / SQLSTATE
  `22032`;
- invalid JSON text raises error `3141` / SQLSTATE `22032`;
- binary string input raises error `3144` / SQLSTATE `22032`.

## MyLite Compatibility Decision

MyLite does not store MySQL binary JSON blobs, so there is no persisted
byte-for-byte MySQL binary payload and no partial-update slack to inspect. For
the current baseline:

- `JSON_STORAGE_SIZE()` parses the admitted JSON document and computes a
  MyLite-owned small-document binary-layout estimate matching the verified
  MySQL 8.4.9 scalar, array, object, nested, string, and signed-integer probes
  for the current DOM subset.
- `JSON_STORAGE_FREE()` parses and validates the admitted JSON document, then
  returns `0` for non-`NULL` documents and `NULL` for SQL `NULL`.
- Unsupported JSON shapes remain outside the baseline rather than pretending
  that MyLite has full MySQL binary JSON parity.

No SQLite fork hook is required. The row-backed path lowers to deterministic,
direct-only, innocuous private SQLite scalar functions that call the same JSON
DOM helper used by no-source scalar execution.

## Lemon Syntax

The intended MyLite Lemon syntax is:

```lemon
expression(A) ::= JSON_STORAGE_SIZE(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= JSON_STORAGE_FREE(T) LPAREN expression(B) RPAREN(R).

expression(A) ::= JSON_STORAGE_SIZE(T) LPAREN RPAREN(R).
expression(A) ::= JSON_STORAGE_SIZE(T) LPAREN expression(B) COMMA
                  function_argument_list(C) RPAREN(R).

expression(A) ::= JSON_STORAGE_FREE(T) LPAREN RPAREN(R).
expression(A) ::= JSON_STORAGE_FREE(T) LPAREN expression(B) COMMA
                  function_argument_list(C) RPAREN(R).

identifier(A) ::= JSON_STORAGE_SIZE(T).
identifier(A) ::= JSON_STORAGE_FREE(T).
```

Both functions return integer-valued results and should follow the numeric JSON
introspection admission paths used by `JSON_LENGTH()` and `JSON_DEPTH()`.

## Runtime Design

The JSON DOM layer exposes `mylite_json_storage_size()`. The helper parses the
document with the existing MyLite JSON parser and computes a binary-layout size
over:

- literals as one-byte literal payloads;
- signed integer numbers as 2-, 4-, or 8-byte numeric payloads;
- strings as a variable-length byte count plus payload bytes;
- small arrays as a four-byte header, three-byte value entries, and out-of-line
  child payloads;
- small objects as a four-byte header, seven-byte key/value entries, key bytes,
  and out-of-line child payloads.

`JSON_STORAGE_FREE()` intentionally reuses this parser path for validation and
then returns zero. This keeps diagnostics and accepted document shapes aligned
with `JSON_STORAGE_SIZE()` without creating fake partial-update state.

## Tests

Expected behavior must be verified against a real MySQL 8.4.9 runtime in
`packages/libmylite/tests/mysql_baseline_json_introspection_functions_expectations.sh`.

MyLite coverage must include:

- parser AST nodes and argument-count marker nodes;
- no-source/`DUAL`/`DO` values and status counters;
- table-backed JSON columns, nonbinary string columns, nested `JSON_EXTRACT()`
  results, and reopen persistence;
- wrong argument counts, invalid scalar data types, invalid text, binary
  strings, missing columns, and invalid table string values;
- compatibility documentation rows for both the top-level matrix and detailed
  JSON function table.
