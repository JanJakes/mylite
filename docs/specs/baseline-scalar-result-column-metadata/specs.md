# Baseline Scalar Result Column Metadata

## Summary

This phase extends the completed descriptor-backed result-column metadata work
to a narrow set of scalar and row-scalar expression result columns already
accepted by MyLite:

```sql
SELECT scalar_item[, scalar_item ...]
SELECT scalar_item[, scalar_item ...] FROM DUAL
SELECT row_scalar_item[, row_scalar_item ...] FROM table_name ...
```

The goal is to stop reporting unknown metadata for common scalar values such as
`SELECT 1`, `SELECT 'abc'`, `SELECT NULL`, common session/system functions, and
the current JSON scalar function cluster. It does not add SQL syntax, expression
evaluation, protocol APIs, or new row values. Existing descriptor-column
metadata remains authoritative for base-table columns.

## Sources And Evidence

- Existing MyLite metadata design:
  - `docs/specs/baseline-result-column-metadata/specs.md`
  - `docs/specs/runtime-handles-statement-context/specs.md`
- Existing scalar and row-scalar expression designs:
  - `docs/specs/baseline-scalar-expression-projection/specs.md`
  - `docs/specs/baseline-json-valid-function/specs.md`
  - `docs/specs/baseline-json-extract-functions/specs.md`
  - `docs/specs/baseline-json-introspection-functions/specs.md`
  - `docs/specs/baseline-json-construction-functions/specs.md`
  - `docs/specs/baseline-json-contains-functions/specs.md`
- Official MySQL 8.4 documentation:
  - C API `MYSQL_FIELD` metadata:
    <https://dev.mysql.com/doc/c-api/8.4/en/c-api-data-structures.html>
  - Optional result-set metadata:
    <https://dev.mysql.com/doc/c-api/8.4/en/c-api-optional-metadata.html>
  - JSON function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
- MySQL 8.4.9 runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_scalar_result_column_metadata_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes with `mysql --column-type-info -vvv` establish the subset used
by this phase:

- Scalar result columns have empty `db`, `table`, `org_table`, and `org_name`
  metadata.
- Explicit select-item aliases replace the label only. They do not create
  origin metadata.
- Nonnegative decimal integer literals that fit in MySQL's unsigned 64-bit
  literal envelope report type `LONGLONG`, binary collation `63`, decimal count
  `0`, and `NOT_NULL BINARY NUM`; values above signed 64-bit range add
  `UNSIGNED`.
- Negative decimal integer literals within signed 64-bit range report
  `LONGLONG`. Negative literals below signed 64-bit range report `NEWDECIMAL`.
- Larger admitted decimal integer literals, up to the scalar projection
  integer-literal limit, report type `NEWDECIMAL`, binary collation `63`,
  decimal count `0`, `NOT_NULL BINARY NUM`, and display length equal to the
  significant digit count plus one sign position.
- SQL `NULL` reports type `NULL`, binary collation `63`, length `0`, decimal
  count `0`, and `BINARY NUM`.
- Ordinary string literals report type `VAR_STRING`, the current connection
  collation, decimal count `31`, and `NOT_NULL`; display length is the decoded
  byte length multiplied by the connection character set maximum byte width for
  the admitted ASCII/UTF-8 subset.
- `DATABASE()` and `SCHEMA()` report nullable `VAR_STRING`, length `256`,
  decimal count `31`, and the current connection collation.
- `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, and `CURRENT_USER()` report
  nullable `VAR_STRING`, length `1152`, decimal count `31`, and the current
  connection collation.
- `VERSION()` reports `VAR_STRING`, length `20`, decimal count `31`, current
  connection collation, and `NOT_NULL`.
- `ROW_COUNT()` reports `LONGLONG`, length `21`, binary collation, decimal
  count `0`, and `NOT_NULL BINARY NUM`.
- `LAST_INSERT_ID()` reports `LONGLONG`, length `21`, binary collation,
  decimal count `0`, and `NOT_NULL UNSIGNED BINARY NUM`.
- `RAND()` and literal-seeded `RAND(seed)` report `DOUBLE`, length `23`,
  decimal count `31`, binary collation, and `NOT_NULL BINARY NUM`.
- `JSON_VALID()`, `JSON_LENGTH()`, `JSON_CONTAINS()`, and
  `JSON_CONTAINS_PATH()` report nullable `LONGLONG`, length `21`, binary
  collation, decimal count `0`, and `BINARY NUM`.
- `JSON_TYPE()` reports nullable `VAR_STRING`, length `68`, decimal count
  `31`, current connection collation, and `BINARY`.
- `JSON_EXTRACT()`, `JSON_ARRAY()`, and `JSON_OBJECT()` report type `JSON`,
  length `4294967292`, decimal count `31`, current connection collation, and
  `BINARY`.
- Row-scalar JSON metadata has the same expression metadata as scalar JSON
  calls. Plain descriptor columns inside row-scalar projections keep the same
  descriptor-owned metadata as ordinary descriptor `SELECT`.
- Successful metadata-only probes produce `warning_count == 0`.

## Supported Scope

Supported result-column metadata:

- no-source and `FROM DUAL` scalar `SELECT` labels and metadata for:
  - decimal integer literals with optional unary sign;
  - `TRUE`, `FALSE`, and `NULL`;
  - ordinary string literals in the current decoded string subset;
  - `DATABASE()`, `SCHEMA()`, `USER()`, `SESSION_USER()`, `SYSTEM_USER()`,
    `CURRENT_USER()`, `VERSION()`, `ROW_COUNT()`, `LAST_INSERT_ID()`,
    `RAND()`, and literal-seeded `RAND(seed)`;
  - current scalar JSON functions `JSON_VALID()`, `JSON_LENGTH()`,
    `JSON_CONTAINS()`, `JSON_CONTAINS_PATH()`, `JSON_TYPE()`,
    `JSON_EXTRACT()`, `JSON_ARRAY()`, and `JSON_OBJECT()`;
- one-table row-scalar `SELECT` metadata for:
  - plain descriptor column items, using descriptor metadata and table alias
    rules from `baseline-result-column-metadata`;
  - current row-scalar JSON functions listed above.

Deferred metadata:

- exact metadata for arithmetic, comparison, logical, bitwise, temporal,
  string, control-flow, aggregate, grouped, joined, `SHOW`, and
  `INFORMATION_SCHEMA` synthetic result expressions outside the listed subset;
- expression metadata for parameters, user variables, scalar subqueries, stored
  functions, generated expressions, defaults, predicates, ordering expressions,
  grouping expressions, and DML assignment expressions;
- protocol-grade metadata beyond the existing public result metadata API.

## Ownership Boundaries

- Public API: unchanged. The existing opaque `mylite_result` accessors expose
  the improved metadata.
- Statement context: continues to own diagnostics, warnings, affected rows, and
  result finalization. Metadata inference does not add warnings.
- Lexer/parser/AST: unchanged. This phase reads existing AST expression kinds
  and aliases only.
- Analyzer/planner: descriptor columns inside row-scalar projections are
  resolved through existing MyLite descriptors. Scalar metadata is inferred
  from the supported AST or planned row-scalar expression kind.
- Catalog module: remains authoritative for descriptor-column metadata. Scalar
  metadata does not mutate catalog rows, descriptor caches, catalog generation,
  or `sqlite_schema_generation`.
- SQLite physical execution: unchanged. SQLite still evaluates generated SQL
  for row-scalar projections, and MyLite still ignores SQLite's physical result
  metadata.
- Result builder: appends either descriptor-backed metadata or scalar
  expression metadata before row values are appended.
- Storage/VFS/file format: unchanged. Metadata reads do not touch the `.mylite`
  preamble or shifted SQLite payload invariants.

## Metadata Rules

All supported scalar expression metadata uses empty schema/table/origin fields.

Common scalar descriptor defaults:

- binary numeric scalar:
  - collation id `63`;
  - flags include `BINARY` and `NUM`;
  - type-specific `NOT_NULL` and `UNSIGNED` bits are added where MySQL reports
    them for the supported expression.
- nonbinary scalar string:
  - type `VAR_STRING`;
  - collation id from the current connection collation;
  - decimals `31`;
  - `NOT_NULL` only where MySQL reports it for the supported expression.
- JSON scalar:
  - type `JSON`;
  - current connection collation id;
  - display length `4294967292`;
  - decimals `31`;
  - `BINARY` flag.

Plain descriptor columns inside row-scalar `SELECT` reuse
`baseline-result-column-metadata` mapping, including selected schema, table
alias, origin table, origin column, type, flags, collation id, display length,
decimals, nullability, and key flags.

## Diagnostics

This phase adds no new SQL diagnostics. Public metadata accessor misuse remains
unchanged: `NULL` results or out-of-range column indexes return `NULL`, `0`,
`MYLITE_RESULT_COLUMN_TYPE_UNKNOWN`, or non-nullable false according to the
existing accessor contract.

Allocation failure while appending metadata reports the existing MyLite
out-of-memory diagnostic and rolls back the in-progress result.

## Performance

Metadata inference is per result column, not per row. Descriptor metadata for
row-scalar plain columns loads the same key/index context used by descriptor
`SELECT`. JSON and scalar expression metadata uses constant mappings and does
not parse JSON documents, evaluate expressions, or inspect row data.

## Tests

Extend `packages/libmylite/tests/runtime_result_column_metadata_test.c` and add
`packages/libmylite/tests/mysql_baseline_scalar_result_column_metadata_expectations.sh`.

Coverage:

- scalar metadata for integer, large decimal integer, boolean, `NULL`, and
  string literals;
- alias labels with empty origin fields;
- current session/system function metadata for the supported subset;
- current JSON scalar function metadata;
- row-scalar JSON function metadata over a descriptor table;
- row-scalar plain descriptor column metadata inside a row-scalar projection;
- existing descriptor metadata, misuse, file preamble, and cleanup behavior.

Verification:

```sh
packages/libmylite/tests/mysql_baseline_scalar_result_column_metadata_expectations.sh
ctest --preset dev -R 'libmylite\.runtime\.result_column_metadata|libmylite\.runtime\.json_.*|libmylite\.parser' --output-on-failure
cmake --workflow --preset check
```
