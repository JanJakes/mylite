# Baseline JSON Schema Functions

## Status

This feature specifies a bounded MySQL 8.4.9 compatibility slice for
`JSON_SCHEMA_VALID()` and `JSON_SCHEMA_VALIDATION_REPORT()`.

MySQL implements JSON Schema Draft 4 validation. MyLite does not add a general
JSON Schema engine in this slice. Instead, it uses the existing MyLite JSON DOM
to validate a small, explicit subset that is common in application-level schema
checks, and it rejects unsupported schema keywords with an explicit diagnostic.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing MyLite JSON function baselines under `docs/specs/`
- MySQL 8.4 Reference Manual, JSON Schema validation functions:
  https://dev.mysql.com/doc/refman/8.4/en/json-validation-functions.html
- MySQL 8.4.9 runtime probes in the local `mylite-mysql-849` container

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation adds:

- generic-function runtime recognition for `JSON_SCHEMA_VALID(schema, document)`;
- generic-function runtime recognition for
  `JSON_SCHEMA_VALIDATION_REPORT(schema, document)`;
- scalar no-source, `DUAL`, `DO`, and session user-variable argument support;
- MySQL-compatible argument-count diagnostics;
- SQL `NULL` propagation after SQL data-type validation;
- invalid JSON text diagnostics for schema and document arguments;
- invalid schema-root type diagnostics when the schema is valid JSON but is not
  an object;
- `type`, `required`, `properties`, `minimum`, and `maximum` schema keywords;
- no-op support for MySQL-documented annotation keys `id`, `$schema`, and
  `description`;
- MySQL-compatible `$ref` unsupported diagnostics;
- MySQL-shaped validation reports for supported failure kinds.

## Non-Goals

This slice does not implement:

- full JSON Schema Draft 4 validation;
- schema keywords outside the supported subset, including `items`, `enum`,
  `pattern`, `additionalProperties`, `oneOf`, `anyOf`, `allOf`, `not`,
  `minLength`, `maxLength`, `minItems`, `maxItems`, `multipleOf`,
  `exclusiveMinimum`, and `exclusiveMaximum`;
- JSON Schema formats or external resources;
- use in stored CHECK constraints, row-backed table projections, predicates,
  DML assignment expressions, arbitrary nested expression arguments, or
  protocol metadata beyond the current scalar `mylite_result` descriptors;
- a new dependency or SQLite fork hook.

Unsupported schema keywords must not be ignored. They return an explicit MyLite
unsupported diagnostic so applications do not receive silently incorrect
validation results.

## MySQL 8.4.9 Behavior

Observed and documented behavior for the baseline:

- both functions require exactly two arguments;
- if either argument has a non-JSON SQL data type, MySQL returns
  `3146 / 22032`;
- if at least one argument is SQL `NULL` after SQL data-type checks, the result
  is SQL `NULL`;
- invalid JSON text returns `3141 / 22032`, with the failing argument number in
  the message;
- a valid JSON schema argument whose root is not an object returns
  `3853 / 22032`;
- `$ref` returns `1235 / 42000`;
- `type` supports object, array, string, number, integer, boolean, and null;
- `required` and `properties` apply to object documents;
- `minimum` and `maximum` apply to numeric documents;
- a valid validation report is `{"valid": true}`;
- an invalid validation report includes `valid`, `reason`,
  `schema-location`, `document-location`, and `schema-failed-keyword`.

Examples verified against MySQL 8.4.9:

- `JSON_SCHEMA_VALID('{"type":"object"}', '{"a":1}')` returns `1`;
- `JSON_SCHEMA_VALID('{"type":"object"}', '[1]')` returns `0`;
- `JSON_SCHEMA_VALID('{}', '123')` returns `1`;
- `JSON_SCHEMA_VALID('{"required":["a"]}', '1')` returns `1`;
- `JSON_SCHEMA_VALID('{"minimum":2}', '1')` returns `0`;
- `JSON_SCHEMA_VALIDATION_REPORT('{"type":"object"}', '{"a":1}')`
  returns `{"valid": true}`.

## MyLite Compatibility Decision

MyLite implements the subset directly in `mylite_json_schema.c` using the
existing MyLite JSON parser and DOM. The helper returns validation status,
failure keyword, and JSON Pointer-style schema/document locations. The scalar
runtime formats MySQL-shaped diagnostics and report JSON.

No SQLite UDF or fork hook is needed for this slice because execution is
limited to scalar no-source/`DUAL`/`DO` and user-variable contexts. Row-backed
support should be added later as part of the broader JSON expression-context
expansion rather than as an isolated shortcut.

## Lemon Syntax

No grammar change is required. The existing generic-function path admits both
function names as:

```lemon
expression(A) ::= identifier(B) LPAREN function_argument_list(C) RPAREN(D).
expression(A) ::= identifier(B) LPAREN RPAREN(D).
```

Runtime dispatch recognizes `JSON_SCHEMA_VALID` and
`JSON_SCHEMA_VALIDATION_REPORT` case-insensitively and applies native function
argument-count diagnostics.

## Tests

MySQL-runtime expectations live in
`packages/libmylite/tests/mysql_baseline_json_schema_functions_expectations.sh`.

MyLite runtime coverage lives in
`packages/libmylite/tests/runtime_json_schema_functions_test.c` and covers:

- successful boolean validation;
- `type`, type arrays, `required`, `properties`, `minimum`, and `maximum`;
- supported annotation keys;
- validation report success and failure JSON;
- SQL `NULL` propagation;
- session user-variable arguments;
- `DO` status counters;
- wrong argument counts, invalid JSON text, invalid schema type, invalid SQL
  data type, `$ref`, and unsupported keyword diagnostics.
