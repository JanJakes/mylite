# SQLite Fork JSON Column Descriptors

## Status

Implemented for the first executable SQLite-fork foundation slice. Remaining
work is limited to the deferred compatibility items listed below.

## References

- MySQL 8.4 Reference Manual, The JSON Data Type:
  https://dev.mysql.com/doc/refman/8.4/en/json.html
- MySQL 8.4 Reference Manual, JSON Functions:
  https://dev.mysql.com/doc/refman/8.4/en/json-functions.html
- MySQL 8.4 Reference Manual, Data Type Storage Requirements:
  https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html
- SQLite JSON functions:
  https://www.sqlite.org/json1.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 behavior, SQLite public documentation, and
the current MyLite codebase. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## Scope

Implement the first executable `JSON` column-storage slice:

- parse `JSON` column declarations;
- reject `JSON(M)`, unsigned/signed numeric attributes, binary string
  attributes, and character-set attributes that do not apply to MySQL JSON
  columns;
- write MySQL-compatible catalog metadata for `DATA_TYPE` and `COLUMN_TYPE`;
- expose MySQL protocol metadata for table-backed JSON columns;
- validate non-NULL assignments at the SQLite-fork record-construction
  boundary;
- preserve SQL `NULL` for nullable JSON columns;
- support public MyLite `INSERT`, `UPDATE`, `DELETE`, `TRUNCATE`, and
  `DROP TABLE` CRUD paths for tables containing JSON columns;
- allow existing MyLite JSON scalar/path functions to operate over stored JSON
  text values.

Out of scope for this slice:

- MySQL's native binary JSON storage layout and normalized text rendering;
- exact diagnostic text, including parser position and column-qualified value
  wording;
- `IGNORE` and non-strict warning demotion;
- JSON default expression edge cases;
- `CAST(... AS JSON)`, generated-column interactions, JSON mutators,
  multi-valued indexes, partial update metadata, comparison ordering, and JSON
  storage-size functions;
- direct SQLite parser support for MySQL `JSON` syntax.

## MySQL Semantics

`JSON` is declared without a display length, precision, scale, character set,
or numeric sign attribute. `INFORMATION_SCHEMA.COLUMNS` reports:

- `DATA_TYPE = 'json'`;
- `COLUMN_TYPE = 'json'`;
- character-set and collation fields as SQL `NULL`.

Protocol metadata for a table-backed JSON column reports MySQL field type
`JSON`, binary collation id `63`, declared length `4294967295`, decimals `0`,
and BLOB/BINARY flags.

Assignment behavior in strict mode:

- SQL `NULL` is accepted only when column nullability allows it;
- string values must be valid JSON text;
- JSON object, array, string, number, boolean, and JSON `null` documents are
  accepted when supplied as valid JSON text;
- invalid JSON text raises MySQL error `3140`, SQLSTATE `22032`;
- non-string scalar assignment such as integer `1` raises the same condition,
  even though the value could be represented as JSON text after an explicit
  cast.

The MySQL 8.4.9 runtime fixture verifies WordPress-like option rows with object
and array JSON documents, JSON read functions, invalid text and non-text
assignment errors, truncate auto-increment reset, and metadata cleanup after
drop.

## SQLite Fork Design

The existing column-descriptor fork point is the correct integration point.
SQLite's public extension surface can register JSON functions and CHECK
constraints, but that is not enough for transparent MySQL behavior:

- a CHECK constraint cannot distinguish non-text scalar assignment from JSON
  text before SQLite affinity conversion;
- CHECK constraints do not carry MySQL column metadata or protocol field
  attributes;
- CHECK constraints cannot report MySQL condition code `3140` and SQLSTATE
  `22032` through the fork diagnostics bridge;
- generated wrapper SQL would miss direct annotated SQLite writes and would
  duplicate behavior across every DML lowering path.

Add a `JSON` descriptor kind to `MyliteColumnType` and the public fork
descriptor API. `OP_MyliteTypeCheck` validates assigned values before
`OP_MakeRecord`. The validation helper calls SQLite's own JSON text parser from
inside the source-tree fork and accepts only canonical JSON text for this first
slice. This keeps parser and memory behavior close to SQLite's optimized JSON
implementation while allowing MyLite to own MySQL diagnostics and metadata.

The physical storage for this slice remains SQLite `TEXT`. This is deliberate:
it provides correct validation and query behavior for the existing MyLite JSON
functions without committing to a binary JSON layout before comparison,
indexing, partial-update, and storage-size semantics are specified.

## MyLite SQL Integration

The MyLite parser adds a `json_column_type` production:

```lemon
column_type ::= json_column_type.
json_column_type ::= JSON.
```

`JSON` remains a nonreserved identifier keyword where the grammar expects an
identifier, so a table may have a column named `json` while another column uses
the `JSON` type.

The descriptor layer writes:

- `DATA_TYPE = 'json'`;
- `COLUMN_TYPE = 'json'`;
- null character-set, collation, numeric precision, scale, length, and datetime
  precision fields.

Physical SQLite tables use `TEXT` affinity for JSON columns. The catalog loader
maps `DATA_TYPE='json'` back into the fork descriptor for public MyLite write
paths and into `MYSQL_TYPE_JSON` result metadata for table-backed reads.

## Fixture

The MySQL 8.4.9 fixture in
`docs/specs/sqlite-fork-json-column-descriptors/mysql-json-column-crud.sql`
captures the supported declaration, assignment, JSON function readback, update,
delete, truncate, drop, and invalid-assignment behavior.

The current MyLite runtime test compares JSON semantics through
`JSON_TYPE()`, `JSON_EXTRACT()`, `JSON_UNQUOTE()`, `JSON_LENGTH()`, and
`JSON_VALID()` rather than raw column text. Raw selected JSON column text is
allowed to differ until MySQL binary JSON normalization and storage-visible
rendering are specified.

## Compatibility Status

MyLite now has partial executable `JSON` support: parser/catalog integration,
fork assignment validation, MySQL-style column metadata, table-backed protocol
metadata, public CRUD coverage, and existing JSON read functions over stored
column values are implemented. Binary JSON layout, normalized raw readback,
comparison/index semantics, JSON mutators, JSON aggregate functions, generated
column interactions, `IGNORE` demotion, exact diagnostic messages, and direct
SQLite parser MySQL syntax remain deferred.
