# Baseline Binary String Types

## Status

This feature specifies the first byte-string storage slice for persistent
`.mylite` handles. It adds descriptor-owned `BINARY`, `VARBINARY`, and `BLOB`
family columns plus byte-safe public result readback.

The feature is intentionally not full MySQL binary-string support. It admits
bounded byte values from ordinary string and hexadecimal literals, `NULL`,
strict length checks, fixed-length `BINARY` zero-byte padding, descriptor
cloning/copying, and descriptor-backed introspection. It does not implement
binary defaults, expression defaults, binary comparisons, binary ordering,
binary indexes, string introducers, bit literals, parameters, streaming large
objects, protocol-grade metadata, or SQLite fork patches.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline `CHAR`, `VARCHAR`, and `TEXT` types:
  `docs/specs/baseline-char-type/specs.md`,
  `docs/specs/baseline-varchar-type/specs.md`, and
  `docs/specs/baseline-text-type/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline INFORMATION_SCHEMA core:
  `docs/specs/baseline-information-schema-core/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, string data type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, `BINARY` and `VARBINARY`:
  https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html
- MySQL 8.4 Reference Manual, `BLOB` and `TEXT`:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4 Reference Manual, hexadecimal literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- MySQL 8.4 Reference Manual, data type defaults:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, column indexes:
  https://dev.mysql.com/doc/refman/8.4/en/column-indexes.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_binary_string_types_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- MySQL 8.4.9 under its default strict SQL mode accepts bare `BINARY` as
  `binary(1)`, accepts `BINARY(0)` through `BINARY(255)`, and rejects
  `BINARY(256)` with error `1074`.
- `VARBINARY` requires a length. `VARBINARY(0)` is accepted. A single
  `VARBINARY(65532)` column is accepted, while larger single-column lengths hit
  the InnoDB row-size error before storage.
- `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and `LONGBLOB` are accepted. `BLOB(M)`
  maps to the smallest BLOB family descriptor that can hold `M` bytes.
- `CHAR BYTE` and `CHAR(n) BYTE` render as `binary(1)` and `binary(n)`. This is
  distinct from `CHAR(n) BINARY`, which remains a nonbinary character column
  with a binary collation and is outside this slice.
- `SHOW COLUMNS` renders lower-case type text. `SHOW CREATE TABLE` renders
  nullable `BINARY` and `VARBINARY` with `DEFAULT NULL`, while BLOB family
  columns omit visible `DEFAULT NULL` clauses.
- `INFORMATION_SCHEMA.COLUMNS` reports binary string descriptors with
  `CHARACTER_SET_NAME` and `COLLATION_NAME` as SQL `NULL`. Character maximum
  and octet lengths are byte counts.
- `BINARY(n)` stores fixed-length byte strings. Shorter values are right-padded
  with `0x00`, and no trailing bytes are stripped on readback.
- `VARBINARY` and BLOB family columns store variable-length byte strings with
  no padding and no stripping on readback.
- Ordinary string literals assigned to binary string columns are decoded before
  storage. Under default SQL mode `\0` stores byte `0x00`; `\%` preserves the
  backslash outside pattern matching; `NO_BACKSLASH_ESCAPES` makes backslash an
  ordinary byte for later string literals.
- `X'...'` and `0x...` literals store binary byte strings in binary-string DML
  contexts. `X''` stores a zero-length byte string. Odd-length `0x...` literals
  are accepted by MySQL and represent a leading-zero padded byte sequence.
- Strict-mode overlength assignment to `BINARY`, `VARBINARY`, or BLOB family
  columns fails with error `1406`, SQLSTATE `22001`, and `Data too long for
  column ...`.
- `INSERT IGNORE` demotes binary overlength and `NULL` into `NOT NULL` failures
  to warnings. `BINARY NOT NULL` implicit values are zero-byte padded to the
  declared length; `VARBINARY NOT NULL` and BLOB family no-default implicit
  values are zero-length byte strings.
- Single-table `UPDATE` reports changed-row affected counts. Reassigning a
  value that produces the same stored byte string reports zero affected rows.
- `BINARY` and `VARBINARY` can have literal defaults. BLOB family columns can
  have defaults only when written as expressions. MyLite defers binary-string
  defaults in this slice because the durable catalog default field is currently
  NUL-terminated text.
- MySQL permits binary-string comparisons, ordering, grouping, and indexing
  with bytewise semantics. MyLite defers those wider operations here except for
  existing `IS NULL` and `IS NOT NULL` predicate forms.

## Scope

The implementation must add:

- public byte-safe result accessors that preserve public ABI compatibility with
  the existing text accessor;
- internal result-cell storage that carries byte lengths and can return
  embedded `NUL` bytes;
- parser and AST support for bare `BINARY`, `BINARY(length)`,
  `VARBINARY(length)`, `CHAR BYTE`, `CHAR(length) BYTE`, bare BLOB family
  types, and `BLOB(length)`;
- descriptor-owned logical type text for admitted binary-string descriptors:
  `BINARY(n)`, `VARBINARY(n)`, `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and
  `LONGBLOB`;
- physical SQLite type text `BLOB` for admitted binary-string descriptors;
- `CREATE TABLE` support for persistent base tables containing admitted binary
  string columns;
- `ALTER TABLE ... ADD [COLUMN]` support for binary string columns, including
  physical implicit-value backfill for `NOT NULL` no-explicit-default additions;
- `CREATE TABLE ... LIKE` descriptor cloning for tables containing admitted
  binary string columns;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source and target binary-string descriptors
  are compatible and SQLite values are already BLOB values;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for binary string descriptors;
- ordinary string literal values and hexadecimal literal values for
  `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE` assignments into binary string
  columns;
- `NULL` assignment and effective nullable `DEFAULT NULL` materialization for
  binary string columns;
- MyLite-owned DML conversion before SQLite binding: string decoding, hex
  decoding, byte-length validation, `BINARY` right-padding with `0x00`, and
  strict overlength diagnostics;
- limited `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` adjustment for
  `NULL` into binary-string `NOT NULL`, omitted no-default binary-string
  `NOT NULL`, explicit `DEFAULT` for no-default binary-string `NOT NULL`, and
  strict overlength binary-string inputs;
- descriptor-backed `SELECT` readback of SQLite BLOB values as public result
  bytes;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  binary string columns;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted binary string data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `VARBINARY` without length, `BINARY()` or `VARBINARY()`, negative lengths,
  signed lengths, non-decimal lengths, parameterized lengths, or length
  expressions;
- `TINYBLOB(M)`, `MEDIUMBLOB(M)`, `LONGBLOB(M)`, `LONG VARBINARY`, `RAW`, or
  non-MySQL aliases;
- `CHARACTER SET binary` rewrites, `CHAR(n) BINARY` collation semantics,
  column-level charset/collation attributes for binary string descriptors, or
  general binary collation metadata beyond introspection values verified for
  this slice;
- explicit binary defaults, expression defaults, `DEFAULT(col_name)`, or
  durable binary default storage;
- binary-to-integer, integer-to-binary, bit-literal, decimal/float, temporal,
  function, subquery, user-variable, parameter, or arbitrary expression
  assignment conversion;
- `_binary` introducers, national strings, adjacent literal concatenation, or
  nonliteral client parameters;
- binary comparison predicates, `LIKE`, `BETWEEN`, `IN`, truth predicates,
  ordering, grouping, `DISTINCT`, aggregates over binary columns, or binary
  expression projection;
- primary keys, unique indexes, secondary indexes, prefix indexes, and
  `FULLTEXT` or spatial indexes over binary string columns;
- streaming BLOB I/O, protocol-grade result metadata, or changed-column
  protocol metadata;
- SQLite fork patches.

## Ownership Boundary

- Public API owns call validation, result ownership, public misuse behavior,
  and additive ABI. `mylite_result_value_text()` remains available as a
  NUL-terminated convenience pointer for non-`NULL` cells, while the new
  byte-size accessors are authoritative for binary values and embedded `NUL`
  bytes.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported in-range binary-string operations
  record `warning_count == 0`; supported `INSERT IGNORE` adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for binary string column types and
  literal shape. They store source spans and structural payloads only; they do
  not resolve catalog descriptors, decode bytes, or perform storage conversion.
- Analyzer/planner code maps binary string AST nodes to durable descriptors,
  resolves schemas/tables/columns against the MyLite catalog, decodes admitted
  string and hex literals, validates lengths, applies `BINARY` zero-byte
  padding, rejects unsupported conversions or operations, and produces
  descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, defaults, and column order. This slice reuses
  logical and physical type descriptor fields and does not change the catalog
  default schema because binary defaults are deferred.
- Result and introspection builders render logical descriptors to MySQL-shaped
  text. SQLite schema text and `sqlite_schema` are not metadata authority.
- SQLite owns physical row storage, scans, and mutations for generated prepared
  statements. MyLite binds binary values with
  `sqlite3_bind_blob(..., SQLITE_TRANSIENT)` and reads SQLite BLOB values with
  `sqlite3_column_blob()` plus `sqlite3_column_bytes()`.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

The feature extends the existing limited column definition grammar:

```sql
column_definition:
    column_name column_type [NULL | NOT NULL] [DEFAULT NULL]
  | column_name column_type

column_type:
    existing_type
  | BINARY
  | BINARY ( unsigned_decimal_integer_literal )
  | VARBINARY ( unsigned_decimal_integer_literal )
  | CHAR BYTE
  | CHAR ( unsigned_decimal_integer_literal ) BYTE
  | TINYBLOB
  | BLOB
  | BLOB ( unsigned_decimal_integer_literal )
  | MEDIUMBLOB
  | LONGBLOB
```

`BINARY` without a length is normalized to descriptor length `1`.
`BINARY(n)` and `CHAR(n) BYTE` admit `0..255`. `VARBINARY(n)` admits
`0..65532` subject to the current row-size validator. `BLOB(n)` maps to the
smallest BLOB family descriptor with capacity at least `n`:

| Length range | Descriptor |
| --- | --- |
| `0..255` | `TINYBLOB` |
| `256..65535` | `BLOB` |
| `65536..16777215` | `MEDIUMBLOB` |
| `16777216..4294967295` | `LONGBLOB` |

DML values for binary string targets extend the existing row-value grammar:

```sql
insert_value:
    existing_value
  | string_literal
  | hex_literal

update_value:
    existing_value
  | string_literal
  | hex_literal
```

`string_literal` is an ordinary MySQL string token under MyLite's current
session SQL mode. For binary-string targets only, `\0` is admitted and decodes
to byte `0x00`. `hex_literal` admits the existing lexer token forms `X'...'`
and `0x...`; the lexer already rejects odd-length `X'...'` forms, while
`0x...` odd digit counts are interpreted with a leading zero nibble like MySQL.

The existing table-backed `WHERE` grammar is unchanged. Binary string columns
are valid only for the already supported `IS NULL` and `IS NOT NULL` predicate
forms.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_type ::= binary_string_type.

binary_string_type ::= BINARY.
binary_string_type ::= BINARY LPAREN INTEGER RPAREN.
binary_string_type ::= VARBINARY LPAREN INTEGER RPAREN.
binary_string_type ::= CHAR BYTE.
binary_string_type ::= CHAR LPAREN INTEGER RPAREN BYTE.
binary_string_type ::= blob_string_type.

blob_string_type ::= TINYBLOB.
blob_string_type ::= BLOB.
blob_string_type ::= BLOB LPAREN INTEGER RPAREN.
blob_string_type ::= MEDIUMBLOB.
blob_string_type ::= LONGBLOB.

insert_value ::= STRING.
insert_value ::= HEX_LITERAL.
update_value ::= STRING.
update_value ::= HEX_LITERAL.
```

The parser may continue to reject unsupported forms as syntax errors. If a
shape is parsed for an existing wider statement family, the analyzer must
return deterministic unsupported diagnostics before generating SQLite SQL.

## Type and Value Semantics

`BINARY(n)` stores exactly `n` bytes. If an admitted input value is shorter than
`n`, MyLite appends zero bytes before SQLite binding. If the value is longer
than `n`, strict DML fails with MySQL-compatible data-too-long diagnostics.
`INSERT IGNORE` truncates to `n` bytes and records a truncation warning.

`VARBINARY(n)` stores between zero and `n` bytes. BLOB family descriptors store
between zero and their family maximum byte length. Values are not padded and
readback does not remove any bytes.

`NULL` stores `NULL` for nullable columns. Assigning `NULL` to `NOT NULL`
binary-string columns fails with MySQL-compatible bad-null diagnostics, except
for supported `INSERT IGNORE` adjustment. The implicit adjusted value is:

| Descriptor family | Adjusted value |
| --- | --- |
| `BINARY(n)` | `n` zero bytes |
| `VARBINARY(n)` | zero-length byte string |
| BLOB family | zero-length byte string |

The current physical range is bounded by SQLite's `int` byte-count APIs and
MyLite's in-memory result object. Values whose byte lengths exceed `INT_MAX`
are rejected with a deterministic MyLite unsupported diagnostic until a
streaming or chunked path exists.

## Metadata and Introspection

`SHOW COLUMNS`, `DESCRIBE`, and `EXPLAIN table` render MySQL lower-case type
text:

| Logical descriptor | `SHOW COLUMNS` type |
| --- | --- |
| `BINARY(0)` | `binary(0)` |
| `BINARY(1)` | `binary(1)` |
| `VARBINARY(3)` | `varbinary(3)` |
| `TINYBLOB` | `tinyblob` |
| `BLOB` | `blob` |
| `MEDIUMBLOB` | `mediumblob` |
| `LONGBLOB` | `longblob` |

`SHOW CREATE TABLE` renders nullable `BINARY` and `VARBINARY` columns with
`DEFAULT NULL`, renders `NOT NULL` binary string columns without a default
clause, and renders BLOB family columns without visible `DEFAULT NULL`.

Limited `INFORMATION_SCHEMA.COLUMNS` reports:

- `DATA_TYPE`: lower-case binary family name;
- `COLUMN_TYPE`: lower-case type text including lengths where applicable;
- `CHARACTER_MAXIMUM_LENGTH`: byte capacity;
- `CHARACTER_OCTET_LENGTH`: byte capacity;
- `CHARACTER_SET_NAME`: SQL `NULL`;
- `COLLATION_NAME`: SQL `NULL`;
- `COLUMN_DEFAULT`: SQL `NULL` for this slice.

## Physical SQLite Handling

Generated MyLite user tables remain ordinary SQLite rowid tables unless a
separate feature changes that invariant. Binary string columns use physical
SQLite type `BLOB`. Generated SQL must quote every SQLite identifier and use
prepared-statement parameters for all binary values. Literal bytes must never
be interpolated into generated SQL.

This slice uses SQLite public APIs only:

- `sqlite3_bind_blob()` for binary DML values;
- `sqlite3_column_blob()` and `sqlite3_column_bytes()` for readback;
- ordinary SQLite `BLOB` column storage inside MyLite-owned physical table
  names such as `_mylite_user_table_<table_id>`.

No targeted SQLite fork hook is required.

## Diagnostics

Supported diagnostics must include:

- syntax errors for malformed binary type lengths and unsupported grammar;
- column length too big for `BINARY(n)` above `255`;
- row size too large for admitted `VARBINARY` combinations that exceed the
  current MySQL row-size baseline;
- unknown schema, unknown table, reserved `_mylite_*` target names, and
  unsupported object kinds through existing descriptor resolution;
- unsupported explicit binary defaults and BLOB expression defaults;
- unsupported assignment expression, unsupported bit literal, unsupported
  nonliteral value, unsupported binary predicate, unsupported binary ordering,
  and unsupported binary index diagnostics;
- data-too-long errors for strict overlength binary DML;
- truncation warnings for supported `INSERT IGNORE` overlength binary DML;
- bad-null errors and warnings for `NULL` into `NOT NULL`;
- no-default errors and warnings for omitted or explicit `DEFAULT` into
  no-default `NOT NULL` descriptors;
- physical SQLite failures, allocation failures, and public API misuse through
  existing MyLite diagnostics and status conventions.

Successful supported in-range binary DML reports `warning_count == 0`.
Successful `UPDATE` reports changed-row affected counts. Successful binary DML
does not mutate catalog rows, descriptor versions, descriptor caches, catalog
generation, or `sqlite_schema_generation`.

## Compatibility Documentation

The implementation must update:

- `COMPATIBILITY.md` for `BINARY`, `VARBINARY`, `CHAR BYTE`, and BLOB family
  status;
- `docs/compatibility/type-system-literals-conversion.md` for binary-string
  values, hex literals in binary-string DML contexts, and byte-safe result
  readback;
- `docs/compatibility/sql-table-ddl.md` for admitted binary string column
  definitions and introspection;
- `docs/compatibility/sql-table-dml.md` for admitted binary string row values,
  `INSERT IGNORE` adjustments, and update assignments;
- `docs/compatibility/sql-query-expressions.md` only to note that binary
  columns are currently limited to readback plus `IS NULL` / `IS NOT NULL`
  predicates;
- `docs/compatibility/character-sets.md` and `docs/compatibility/collations.md`
  only for the limited binary charset/collation metadata exposed by descriptors.

Do not overclaim full binary-string expressions, binary collation behavior,
prefix indexes, BLOB streaming, defaults, or protocol metadata.

## Test Plan

Add a fast C test binary, preferably `runtime_binary_string_types`, covering:

- public byte-result accessors, including SQL `NULL`, zero-length values, text
  values, embedded `NUL`, and misuse bounds;
- parser acceptance for admitted binary string type forms and rejection of
  unsupported lengths/forms;
- `CREATE TABLE`, `ALTER TABLE ADD COLUMN`, `CREATE TABLE LIKE`, table rename,
  drop, reopen persistence, independent file-backed handles, and preamble
  preservation;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `DESCRIBE`/`EXPLAIN table`, and
  `INFORMATION_SCHEMA.COLUMNS`;
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and `UPDATE` binary string assignments with ordinary
  string literals, `X'...'`, `0x...`, `X''`, `NULL`, `DEFAULT`, embedded `NUL`,
  default and `NO_BACKSLASH_ESCAPES` string decoding, `BINARY` padding,
  `VARBINARY` no-padding, and BLOB no-padding;
- `INSERT IGNORE` warning-producing adjustments for overlength, bad-null, and
  no-default cases;
- affected rows, warning counts, absence of result rows for successful DML, and
  row bytes after each mutation;
- existing `WHERE IS NULL` and `WHERE IS NOT NULL` predicate reuse;
- deterministic rejection of unknown columns, unsupported binary predicates,
  unsupported order keys, unsupported defaults, unsupported indexes, unsupported
  assignment expressions, bit literals, parameters, functions, subqueries, and
  arbitrary expressions;
- that existing lexer, parser, runtime lifecycle, result metadata, catalog,
  storage, VFS, and compatibility tests still pass.

Before implementation is marked complete, run:

1. `cmake --build --preset dev`
2. the new CTest entry and related parser/runtime lifecycle entries;
3. `sh packages/libmylite/tests/mysql_baseline_binary_string_types_expectations.sh`
4. `cmake --workflow --preset check`
