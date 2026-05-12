# Baseline VARCHAR Length Lifecycle

## Status

This feature widens MyLite's existing `VARCHAR` descriptor slice from the first
`0..255` storage baseline to the useful `utf8mb4` single-column envelope
verified against MySQL 8.4.9. It keeps the existing row-value, default,
metadata, DML, clone/copy, and persistence paths, but raises the non-key
descriptor length cap to `0..16383` and adds descriptor-owned row-size checks so
wide definitions do not over-accept obvious MySQL row-size failures.

The feature is intentionally not full MySQL string support. It does not add
column character sets, general collation behavior, prefix indexes, wider string
key semantics, string ordering, protocol-grade metadata, or SQLite fork hooks.

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
- Baseline `VARCHAR` type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline string defaults:
  `docs/specs/baseline-string-defaults/specs.md`
- Baseline `CHARACTER` aliases:
  `docs/specs/baseline-character-alias-lifecycle/specs.md`
- Baseline string keys:
  `docs/specs/baseline-char-varchar-key-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, data type storage requirements:
  https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html
- MySQL 8.4 Reference Manual, row-size limits:
  https://dev.mysql.com/doc/refman/8.4/en/column-count-limit.html
- MySQL 8.4 Reference Manual, InnoDB limits:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-limits.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_varchar_length_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- MySQL 8.4.9 under the default `utf8mb4_0900_ai_ci` schema accepts
  `VARCHAR(256)`, `VARCHAR(512)`, and a single-column `VARCHAR(16383)`.
- `INFORMATION_SCHEMA.COLUMNS` reports `CHARACTER_MAXIMUM_LENGTH = n` and
  `CHARACTER_OCTET_LENGTH = n * 4` for these `utf8mb4` descriptors.
- `SHOW COLUMNS` renders `varchar(n)`, and `SHOW CREATE TABLE` renders the
  normalized descriptor text with the fixed default charset/collation suffix.
- `CHARACTER VARYING(n)` and `CHAR VARYING(n)` normalize to `varchar(n)` for
  admitted wider lengths.
- `VARCHAR(16384)` fails with error `1074`, SQLSTATE `42000`, and a column
  length-too-big diagnostic.
- Wide combinations are still constrained by MySQL's 65,535-byte row envelope:
  two `VARCHAR(8191)` columns are accepted, two `VARCHAR(8192)` columns fail
  with error `1118`, `INT + VARCHAR(16382)` is accepted, and
  `INT + VARCHAR(16383)` fails with error `1118`.
- MySQL accepts a unique key on `VARCHAR(512)` under the default 16KB InnoDB
  page size, but rejects much wider keys once the 3072-byte key limit is
  exceeded. MyLite keeps the current `CHAR` / `VARCHAR(1..255)` ASCII key
  subset until full key-length and collation semantics are designed.

## Scope

The implementation must add:

- descriptor-owned `VARCHAR(n)`, `CHARACTER VARYING(n)`, and
  `CHAR VARYING(n)` support for admitted lengths `0..16383`;
- continued physical SQLite type text `TEXT` for admitted wide `VARCHAR`
  descriptors;
- `CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]` row-size validation for
  current MyLite descriptors using MySQL's documented maximum row-size envelope
  and MyLite's fixed `utf8mb4` assumption;
- preservation of existing string value conversion, UTF-8 validation, embedded
  `NUL` rejection, strict overlength diagnostics, nullable / not-null behavior,
  `INSERT` / `REPLACE` / single-assignment `UPDATE` string values, and
  descriptor-backed `IS NULL` / `IS NOT NULL` predicates;
- preservation of existing literal string defaults for `CHAR` / `VARCHAR`
  values that fit MyLite's durable catalog default storage;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA.COLUMNS` metadata for wider descriptors;
- descriptor clone/copy behavior for `CREATE TABLE ... LIKE`,
  `CREATE TABLE ... SELECT`, and `INSERT ... SELECT`;
- persistent storage, reopen behavior, rename/drop behavior, `.mylite` preamble
  preservation, and independent file-backed handle behavior for wider
  `VARCHAR` rows;
- deterministic rejection of `VARCHAR(16384)`, row-size overflow definitions,
  `VARCHAR(0)` key participation, and `VARCHAR(256+)` string keys under the
  current MyLite-specific string-key subset;
- MySQL 8.4.9 expectation coverage for the supported wider descriptors,
  metadata, row-size boundary behavior, and intentionally deferred key surface.

## Non-Goals

This feature must not implement:

- `CHAR` lengths above `255`;
- column-level `CHARACTER SET`, `CHARSET`, `COLLATE`, `BINARY`, `ASCII`,
  `UNICODE`, or national-character syntax;
- general string comparisons, string ordering, grouped string keys, string
  `DISTINCT`, `LIKE` over table data, collations, coercibility, casts,
  functions, parameters, or arbitrary expression evaluation;
- InnoDB page-local row-size enforcement beyond the MySQL 65,535-byte row
  envelope for the current descriptor set;
- wider `VARCHAR` primary keys, unique keys, secondary indexes, prefix keys,
  descending keys, composite string keys, non-ASCII key collation behavior, or
  full 3072-byte InnoDB key-length semantics;
- warning-producing string default or row-value truncation;
- larger catalog default storage for very long literal defaults;
- protocol-grade string metadata or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics, warnings, affected rows, and statement
  transaction completion. Supported in-range wide `VARCHAR` operations report
  `warning_count == 0` unless they reuse an existing warning-producing path such
  as supported `INSERT IGNORE` adjustment.
- Lexer/parser/AST syntax is unchanged for this slice. Existing `VARCHAR(n)`,
  `CHARACTER VARYING(n)`, and `CHAR VARYING(n)` AST nodes store source spans
  only; they do not resolve descriptor lengths or row-size constraints.
- Analyzer/planner code maps the parsed length to durable descriptor text,
  validates `0..16383`, validates the MySQL row-size envelope for create/add
  column paths, preserves the current string-key cap, and produces
  descriptor-driven physical plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, defaults, visibility, column order, and key metadata. SQLite
  schema text is never consulted as logical metadata authority.
- Result and introspection builders render logical descriptors to MySQL-shaped
  text and metadata lengths.
- SQLite owns physical `TEXT` row storage and scans for generated prepared
  statements. MyLite binds string values with explicit byte lengths and stable
  generated identifiers.
- Storage/VFS behavior is unchanged. Wide `VARCHAR` data is written only inside
  the shifted SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL Grammar

No new grammar productions are required. This slice widens semantic admission
for existing column-type productions:

```sql
varchar_type:
    VARCHAR ( unsigned_decimal_integer_literal )
  | CHARACTER VARYING ( unsigned_decimal_integer_literal )
  | CHAR VARYING ( unsigned_decimal_integer_literal )
```

`unsigned_decimal_integer_literal` is admitted only when its value is between
`0` and `16383`. Signs, expressions, parameters, empty parentheses, omitted
lengths, non-decimal forms, and column character-set attributes remain outside
this slice.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar surface, not MySQL's complete
grammar:

```lemon
column_type ::= VARCHAR LPAREN INTEGER RPAREN.
column_type ::= CHARACTER VARYING LPAREN INTEGER RPAREN.
column_type ::= CHAR VARYING LPAREN INTEGER RPAREN.
```

The parser can continue to reject malformed syntax. The analyzer owns the
length cap and row-size diagnostics.

## Descriptor and Row-Size Semantics

`VARCHAR(n)` uses logical descriptor text `VARCHAR(n)` and physical descriptor
text `TEXT`. `n` is stored only in logical descriptor text and parsed by
descriptor helpers when validating values and rendering metadata.

The semantic maximum for this slice is `16383`, matching observed MySQL 8.4.9
behavior for a single `utf8mb4` `VARCHAR` column. Values above that maximum
produce a MySQL-compatible length-too-big diagnostic before physical SQL is
generated.

`CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]` validate an internal maximum
row-size estimate before mutating the catalog. The estimate is descriptor-owned
and independent of SQLite physical storage:

- `VARCHAR(n)` contributes `n * 4` maximum data bytes plus a one-byte length
  prefix when `n * 4 <= 255`, otherwise a two-byte prefix.
- `CHAR(n)` contributes `n * 4` maximum bytes.
- Integer-family descriptors contribute their documented fixed byte widths:
  `TINYINT` 1, `SMALLINT` 2, `MEDIUMINT` 3, `INT` / `INTEGER` 4, and `BIGINT`
  8, with signedness not changing byte width.
- `DATE`, `DATETIME`, and `TIMESTAMP` contribute their documented current
  nonfractional widths: 3, 5, and 4 bytes.
- `DECIMAL(M,D)` uses the documented packed decimal byte calculation for the
  integer and fractional parts independently.
- `TEXT` family descriptors contribute the documented row-buffer pointer range
  conservatively as 12 bytes.

If the total exceeds `65535`, MyLite returns error `1118`, SQLSTATE `42000`,
with a row-size-too-large diagnostic. MyLite intentionally does not enforce
InnoDB page-local row-size limits in this slice because the physical storage
engine is SQLite and MyLite has not yet modeled InnoDB page formats.

## Defaults, Values, and Metadata

Existing `VARCHAR` value semantics continue to apply:

- admitted string values must be valid UTF-8, contain no embedded `NUL`, and
  contain no more than the declared number of Unicode code points;
- `VARCHAR` preserves trailing spaces for stored and returned values;
- strict nonspace overlength row values fail with the existing
  `Data too long for column ...` diagnostic;
- nullable no-explicit-default columns render `DEFAULT NULL`;
- `NOT NULL` no-explicit-default string columns use the existing MySQL implicit
  empty-string behavior for supported backfill and `INSERT IGNORE` adjustment;
- explicit string defaults are supported only when the decoded default text fits
  MyLite's current catalog default storage.

Metadata rendering remains descriptor-driven:

- `SHOW COLUMNS` and `SHOW CREATE TABLE` render `varchar(n)`;
- `INFORMATION_SCHEMA.COLUMNS.CHARACTER_MAXIMUM_LENGTH` is `n`;
- `INFORMATION_SCHEMA.COLUMNS.CHARACTER_OCTET_LENGTH` is `n * 4`;
- alias spellings render as normalized `varchar(n)`.

## Key and Index Boundary

Wide descriptors do not widen string-key support. The current MyLite string-key
subset remains single-column ASCII `CHAR(1..255)` / `VARCHAR(1..255)` for
primary keys, unique keys, and nonunique secondary indexes. `VARCHAR(0)` and
`VARCHAR(256+)` key participation must fail before catalog mutation and before
physical index SQL is generated.

This is a documented compatibility gap: MySQL accepts some wider indexed
`VARCHAR` descriptors when the full key length fits InnoDB's byte limit. MyLite
keeps that behavior deferred until key-prefix length, collation, and non-ASCII
index semantics are designed together.

## Diagnostics

The feature uses existing diagnostics where possible:

- malformed syntax, missing length, empty length, signed length, parameters, or
  unsupported attributes use the current parse diagnostic;
- `VARCHAR(16384)` and larger lengths use MySQL-compatible error `1074`,
  SQLSTATE `42000`;
- row-size overflow uses MySQL-compatible error `1118`, SQLSTATE `42000`;
- unsupported `VARCHAR(0)` keys and `VARCHAR(256+)` keys use the current
  deterministic MyLite string-key diagnostics;
- invalid string assignment, embedded `NUL`, overlength values, `NULL` into
  `NOT NULL`, duplicate keys, unknown tables/columns, physical SQLite failures,
  allocation failures, and public API misuse use existing normalized `VARCHAR`
  paths.

## Tests

Extend existing parser/runtime coverage rather than creating a new framework:

- MySQL-runtime expectation script for `VARCHAR(256)`, `VARCHAR(512)`,
  `VARCHAR(16383)`, aliases, metadata, accepted/rejected row-size boundaries,
  `VARCHAR(16384)`, and deferred wider string-key semantics.
- Fast C runtime coverage for successful wide create/insert/update/select,
  alias normalization, `SHOW` / `INFORMATION_SCHEMA` metadata, explicit default
  within catalog capacity, `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, reopen persistence, table rename/drop behavior,
  independent handles, and `.mylite` preamble preservation.
- Diagnostics coverage for `VARCHAR(16384)`, row-size overflow at create time
  and `ALTER TABLE ... ADD COLUMN`, `VARCHAR(256)` primary/unique/nonunique key
  attempts, and existing unsupported string expression/order surfaces.

Run focused runtime/parser tests, the MySQL expectation script, and
`cmake --workflow --preset check` before marking the feature complete.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/type-system-literals-conversion.md`, and
`docs/compatibility/character-sets.md` to say that non-key `VARCHAR` and
`CHARACTER VARYING` descriptors are admitted up to `16383` under the current
fixed `utf8mb4` surface, with row-size validation and unchanged
`VARCHAR(1..255)` ASCII key limits.
