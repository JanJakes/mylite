# Baseline JSON Type

## Status

This phase adds a narrow descriptor-owned `JSON` column type for common MySQL
application schemas:

```sql
CREATE TABLE events (id INT, payload JSON)
INSERT INTO events VALUES (1, '{"kind":"created","id":1}')
UPDATE events SET payload = '{"kind":"updated"}' WHERE id = 1
```

The implementation stores canonical JSON text in SQLite `TEXT` columns and keeps
MyLite descriptors authoritative for type admission, validation, conversion,
introspection, and result metadata. It does not add JSON path expressions,
JSON functions, generated columns, multi-valued indexes, partial updates, binary
JSON storage, or general expression conversion.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `TEXT`, binary string, `ENUM`, `SET`, update, scalar subquery, DML
  default, information schema, result metadata, and index specs:
  `docs/specs/baseline-text-type/specs.md`,
  `docs/specs/baseline-binary-string-types/specs.md`,
  `docs/specs/baseline-enum-type/specs.md`,
  `docs/specs/baseline-set-type/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`,
  `docs/specs/baseline-dml-default-keyword-values/specs.md`,
  `docs/specs/baseline-information-schema-core/specs.md`, and
  `docs/specs/baseline-result-column-metadata/specs.md`
- MySQL lexer and parser scaffold:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `JSON` data type:
  https://dev.mysql.com/doc/refman/8.4/en/json.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- RFC 8259, JSON data interchange syntax:
  https://www.rfc-editor.org/rfc/rfc8259

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, RFC 8259, public SQLite APIs, and
existing MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## MySQL 8.4.9 Observations

Runtime probes are recorded by
`packages/libmylite/tests/mysql_baseline_json_type_expectations.sh`.

Observed behavior shaping this slice:

- `JSON` is accepted as a column type and renders as lower-case `json` in
  `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- Nullable `JSON` columns have `DEFAULT NULL`; `JSON NOT NULL` columns have no
  visible default.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE = json`,
  `COLUMN_TYPE = json`, no character set or collation, no character maximum
  length, no octet length, and no numeric or datetime precision.
- JSON values are provided as SQL strings in JSON contexts. Valid JSON stores
  successfully; invalid JSON fails with `3140 / 22032`.
- Stored JSON is normalized for display. For this phase the verified subset is:
  JSON `null`, booleans, strings, signed integer numbers in the current
  signed-64 envelope, arrays, and objects with string keys. Object members with
  duplicate keys retain the last value for a key. Object output is ordered by
  MySQL's observed binary JSON object-key display order for the admitted ASCII
  key subset: shorter keys sort before longer keys, with bytewise comparison for
  equal-length keys.
- MySQL also normalizes decimal and exponent JSON numbers, Unicode escape
  details, non-ASCII object keys, and wider numeric ranges. MyLite defers those
  forms in this phase rather than guessing at partial behavior.
- Explicit `DEFAULT NULL` is accepted on nullable JSON columns. Bare literal
  defaults such as `DEFAULT '{}'` fail with `1101 / 42000`. Expression defaults
  such as `DEFAULT ('{}')` are accepted by MySQL but deferred by MyLite.
- `ALTER TABLE ... ADD COLUMN j JSON` backfills existing rows with SQL `NULL`.
  `ALTER TABLE ... ADD COLUMN j JSON NOT NULL` backfills existing rows with the
  JSON value `null` while keeping no explicit default for future omitted
  inserts.
- Omitted or explicit `DEFAULT` values for a `JSON NOT NULL` column with no
  explicit default fail in strict mode with `1364 / HY000`. `INSERT IGNORE`
  stores the JSON value `null` and records a warning for omitted no-default
  `JSON NOT NULL` columns.
- Explicit `NULL` into `JSON NOT NULL` fails with `1048 / 23000`.
- `UPDATE` reports changed-row affected counts. Reassigning a JSON value whose
  canonical stored text is unchanged reports zero changed rows.
- `WHERE json_col IS NULL` and `WHERE json_col IS NOT NULL` work like ordinary
  SQL null tests. General JSON comparison, ordering, grouping, JSON path
  extraction, and JSON functions are separate behavior.
- MySQL rejects direct indexes on JSON columns with `3152 / 42000`; JSON
  indexing requires generated columns or JSON-path multi-valued indexes, both
  outside this phase.

## Scope

Supported:

- persistent and shadowing session temporary base tables where the existing DDL
  and DML paths already support the statement class;
- `JSON` column definitions in `CREATE TABLE`, `CREATE TEMPORARY TABLE`,
  `CREATE TABLE ... LIKE`, and `ALTER TABLE ... ADD [COLUMN]`;
- durable logical descriptor text `JSON` and physical descriptor text `TEXT`;
- nullable `DEFAULT NULL` metadata for JSON columns;
- row values for `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and one-assignment `UPDATE`;
- JSON row assignments from SQL string literals containing the admitted JSON
  subset, SQL `NULL`, and `DEFAULT`;
- omitted/default values through the existing DML default machinery;
- strict invalid JSON diagnostics for malformed JSON and deterministic MyLite
  unsupported diagnostics for valid JSON shapes outside this phase;
- `INSERT IGNORE` adjustment for omitted/default and explicit `NULL` into
  `JSON NOT NULL`, storing canonical JSON `null` with existing warning policy;
- descriptor-backed `SELECT` readback as canonical JSON text;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL`;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`,
  `REPLACE ... SELECT`, and scalar-subquery `UPDATE` only when source and target
  descriptors are both JSON and the source physical value is already canonical
  JSON text;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, limited
  `INFORMATION_SCHEMA.COLUMNS`, and result-column metadata;
- reopen persistence, table rename/drop behavior, `.mylite` preamble
  preservation, and independent file-backed handle isolation.

Deferred:

- JSON decimal and exponent numbers, non-ASCII object-key ordering, and Unicode
  escape normalization beyond the exact JSON string forms covered by the tests;
- expression defaults such as `DEFAULT ('{}')`;
- bare JSON literal SQL syntax outside string literals;
- `CAST(... AS JSON)`, JSON functions, `->`, `->>`, JSON path literals, and
  `JSON_TABLE`;
- JSON comparison predicates beyond `IS NULL` / `IS NOT NULL`, ordering,
  grouping, distinct, aggregates, and expression projection;
- generated columns, functional indexes, multi-valued JSON indexes, direct JSON
  indexes, fulltext/spatial indexes, constraints, triggers, cascades, partial
  update optimization, `max_allowed_packet`, binary JSON storage, and protocol
  storage-size metadata.

## Ownership Boundary

- Public API: no new functions. `mylite_execute()` owns public misuse behavior,
  result lifetime, and cleanup. Existing text/byte result accessors expose
  canonical JSON text.
- Statement context: owns diagnostics, warning count, affected rows, insert id,
  and statement completion. Supported in-range JSON operations report
  `warning_count == 0` except for existing `IGNORE` adjustments.
- Lexer/parser/AST: admits `JSON` as a column type and keeps `JSON` usable as a
  nonreserved identifier where identifier grammar permits it. Parser code
  stores source spans only and does not validate JSON values.
- Analyzer/planner: maps `JSON` AST types to descriptors, resolves schemas and
  columns, decodes SQL string literals, validates and canonicalizes admitted
  JSON text, rejects unsupported conversions, and prepares descriptor-driven
  SQLite operations.
- Catalog: remains authoritative for logical type, physical type, nullability,
  default state, visibility, and column order. This phase reuses existing
  descriptor fields; no catalog schema migration is needed.
- Result and introspection builders: render MySQL-shaped JSON type metadata
  from descriptors. SQLite schema text and `sqlite_schema` are physical
  artifacts, not metadata authority.
- SQLite: stores canonical JSON as `TEXT`, executes scans and mutations, and is
  accessed through generated prepared statements with bound values.
- Storage/VFS: owns the `.mylite` preamble and shifted SQLite payload boundary.
  JSON data writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported Grammar

The feature extends the existing column definition grammar:

```sql
column_definition:
    column_name JSON [NULL | NOT NULL] [DEFAULT NULL]
```

MyLite Lemon-syntax snippet:

```lemon
column_type ::= json_type.

json_type ::= JSON.

identifier ::= JSON.
```

JSON row values reuse the existing DML value grammar. A JSON document is an
ordinary SQL string literal in a JSON column value position:

```sql
INSERT INTO t VALUES ('{"a":1}')
UPDATE t SET j = '[true,false,null]'
```

SQL `NULL` stores SQL `NULL` when the column is nullable. The JSON value `null`
must be written as the string literal `'null'`.

## Validation And Canonicalization

MyLite validates and canonicalizes JSON before binding physical values:

- SQL string literals are decoded using the existing session SQL-mode string
  policy.
- Embedded NUL bytes are rejected for this text-backed JSON slice.
- The JSON grammar admits objects, arrays, strings, signed integer numbers in
  signed-64 range, `true`, `false`, and `null`.
- Whitespace is accepted around and between JSON tokens.
- Objects require string keys and use last-key-wins duplicate handling.
- Object output order follows the verified MySQL display order for ASCII keys:
  key byte length first, then bytewise ascending order.
- Arrays preserve element order.
- Canonical output inserts one space after each comma and colon, matching the
  verified MySQL 8.4.9 display for the admitted subset.
- JSON strings output with double quotes and JSON escapes for quotes,
  backslash, and control characters. Non-control UTF-8 bytes are preserved.
- Valid JSON numbers outside the admitted signed-integer subset produce a
  deterministic MyLite unsupported diagnostic, not guessed MySQL behavior.

Malformed JSON values fail with `3140 / 22032` and include the approximate
0-based position in the decoded JSON value. Unsupported but syntactically valid
JSON shapes fail with MyLite's generic unsupported diagnostic until a later
slice specifies and verifies them.

## Defaults And Implicit Values

- Nullable `JSON` columns default to SQL `NULL`.
- `JSON DEFAULT NULL` is accepted only when the column is nullable.
- `JSON NOT NULL DEFAULT NULL` fails with `1067 / 42000`.
- Bare literal JSON defaults fail with `1101 / 42000`, matching MySQL's rule
  that JSON cannot have a bare literal default.
- JSON expression defaults are deferred and rejected deterministically.
- `ALTER TABLE ... ADD COLUMN j JSON NOT NULL` uses physical `DEFAULT 'null'`
  only for the SQLite rebuild/backfill operation; descriptor metadata remains
  no-explicit-default.
- Omitted/default `JSON NOT NULL` DML values with `IGNORE` store canonical
  JSON `null` and append the existing no-default warning.

## Indexes, Keys, And Constraints

Direct indexes on JSON descriptors are rejected before generated SQLite SQL is
created. MyLite should use MySQL-compatible `3152 / 42000` for direct JSON
primary, unique, and secondary key attempts in this slice. JSON generated-column
indexes and multi-valued indexes are separate features.

## SQLite Handling

Generated physical column SQL uses quoted identifiers and SQLite `TEXT` type
names for JSON descriptors. DML binds canonical JSON with
`sqlite3_bind_text(..., SQLITE_TRANSIENT)`. No SQL literals are interpolated for
row values. No SQLite JSON extension, JSON1 dependency, or SQLite fork patch is
required for this slice.

## Diagnostics

The implementation must cover:

- syntax errors and unsupported JSON grammar positions;
- missing default schema, unknown schema, unknown table, reserved names, and
  unsupported object kind through existing table-resolution diagnostics;
- invalid JSON text: `3140 / 22032`;
- unsupported JSON values outside this phase: MyLite generic unsupported
  diagnostic;
- JSON bare literal defaults: `1101 / 42000`;
- invalid `DEFAULT NULL` on `JSON NOT NULL`: `1067 / 42000`;
- `NULL` into `JSON NOT NULL`: `1048 / 23000`;
- omitted/default `JSON NOT NULL` without `IGNORE`: `1364 / HY000`;
- direct JSON indexes: `3152 / 42000`;
- unsupported implicit conversion into or out of JSON for `INSERT ... SELECT`,
  `REPLACE ... SELECT`, and scalar-subquery `UPDATE`;
- physical SQLite failures, allocation failures, and public API misuse through
  existing result/diagnostic conventions.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered as a
dotted CTest entry. Tests must cover:

- parser support and `JSON` as a nonreserved identifier;
- `CREATE TABLE`, `CREATE TEMPORARY TABLE`, `ALTER TABLE ADD COLUMN`, and
  `CREATE TABLE ... LIKE` with JSON descriptors;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and
  result-column metadata;
- `INSERT`, `INSERT IGNORE`, `INSERT SET`, `REPLACE`, `UPDATE`,
  `UPDATE ... WHERE`, and `UPDATE` no-op affected rows;
- descriptor-compatible `INSERT ... SELECT`, `REPLACE ... SELECT`, and
  scalar-subquery `UPDATE`;
- canonical readback for admitted objects, arrays, strings, integers,
  booleans, and JSON `null`;
- SQL `NULL` handling distinct from JSON `null`;
- invalid JSON text, unsupported decimal/exponent JSON numbers, defaults,
  direct indexes, unknown names, and unsupported predicates/order/grouping;
- reopen persistence, rename/drop behavior, preamble preservation, independent
  handles, and zero-initialized cleanup.

Before implementation is marked complete, run:

1. `cmake --build --preset dev`
2. The new CTest entry plus parser, table DDL, row-value, update, select,
   information-schema, result-metadata, and relevant type lifecycle entries
3. `packages/libmylite/tests/mysql_baseline_json_type_expectations.sh`
4. `cmake --workflow --preset check`
