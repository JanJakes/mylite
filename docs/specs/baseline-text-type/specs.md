# Baseline TEXT Type

## Status

This feature specifies the first large-object string slice for persistent
`.mylite` handles. It extends the existing descriptor-owned `VARCHAR` text
storage path to the four MySQL `TEXT` family types: `TINYTEXT`, `TEXT`,
`MEDIUMTEXT`, and `LONGTEXT`.

The feature is intentionally not full MySQL large-object support. It admits
ordinary UTF-8 non-`NUL` string values, `NULL`, strict byte-length checks,
descriptor cloning/copying, and descriptor-backed introspection. It does not
implement `TEXT(M)`, column-level character sets/collations, string defaults,
expression defaults, binary `BLOB` types, string comparison semantics, string
ordering/grouping/distinct, full protocol metadata, or streaming large-object
I/O.

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
- Baseline VARCHAR type:
  `docs/specs/baseline-varchar-type/specs.md`
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
- MySQL 8.4 Reference Manual, `BLOB` and `TEXT`:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_text_type_expectations.sh` records the
runtime probes for this feature. Observed behavior that shapes this slice:

- MySQL 8.4.9 under its default strict SQL mode accepts bare `TINYTEXT`,
  `TEXT`, `MEDIUMTEXT`, and `LONGTEXT` column types.
- `SHOW COLUMNS` renders the lower-case type names with SQL `NULL` in the
  default column for nullable and not-null `TEXT` family columns.
- `SHOW CREATE TABLE` renders `TEXT` family columns without `DEFAULT NULL`,
  even when the column was declared nullable or `DEFAULT NULL`.
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE` and `COLUMN_TYPE` as the
  lower-case type name. For the default `utf8mb4_0900_ai_ci` table surface,
  `CHARACTER_SET_NAME` is `utf8mb4`, `COLLATION_NAME` is
  `utf8mb4_0900_ai_ci`, and character/octet lengths are:
  `255` for `TINYTEXT`, `65535` for `TEXT`, `16777215` for `MEDIUMTEXT`, and
  `4294967295` for `LONGTEXT`.
- `TEXT` family values preserve empty strings and trailing spaces. `NULL`
  stores and reads back as `NULL` for nullable columns.
- Strict-mode assignment of nonspace-overlength `TINYTEXT` values fails with
  error `1406`, SQLSTATE `22001`, and `Data too long for column ...`.
- Strict-mode assignment of only excess trailing spaces to `TEXT` family
  columns succeeds with a warning and truncates to fit. MyLite defers this
  warning-producing truncation in this slice and admits only values already
  within the declared byte limit.
- `TEXT` family columns cannot have literal defaults. MySQL accepts expression
  defaults such as `DEFAULT ('abc')`, but MyLite defers expression defaults and
  string default catalog storage.
- `INSERT IGNORE` converts `NULL` into `TEXT NOT NULL` and no-default failures
  into warnings and stores the implicit empty string.
- Single-table `UPDATE` reports changed-row affected counts for `TEXT` family
  assignments. Reassigning the same value reports zero changed rows.
- MySQL accepts `UPDATE ... ORDER BY integer_column ... LIMIT row_count` over
  tables containing `TEXT` columns. Ordering by a `TEXT` column has collation
  and `max_sort_length` behavior outside this slice.

## Scope

The implementation must add:

- parser and AST support for bare `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and
  `LONGTEXT` column types;
- descriptor-owned logical type text equal to one of `TINYTEXT`, `TEXT`,
  `MEDIUMTEXT`, or `LONGTEXT`;
- physical SQLite type text `TEXT` for admitted `TEXT` family descriptors;
- `CREATE TABLE` support for persistent base tables containing admitted
  `TEXT` family columns;
- `ALTER TABLE ... ADD [COLUMN]` support for `TEXT` family columns, including
  physical empty-string backfill for `NOT NULL` no-explicit-default additions;
- `CREATE TABLE ... LIKE` descriptor cloning for tables containing admitted
  `TEXT` family columns;
- descriptor-backed `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` copying when source and target values are already
  compatible with admitted string target descriptors;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for `TEXT` family descriptors;
- ordinary string literal values for `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table `UPDATE`
  assignments into `TEXT` family columns;
- `NULL` assignment and effective nullable `DEFAULT NULL` materialization for
  `TEXT` family columns, while `SHOW CREATE TABLE` omits a visible `DEFAULT`
  clause for these columns;
- limited `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` adjustment for
  `NULL` into `TEXT NOT NULL` and omitted or explicit `DEFAULT` for no-
  explicit-default `TEXT NOT NULL`, storing the MySQL implicit empty string and
  recording warnings;
- descriptor-backed `SELECT` readback of SQLite `TEXT` values as public result
  text;
- descriptor-backed `WHERE column IS NULL` and `WHERE column IS NOT NULL` on
  `TEXT` family columns;
- deterministic rejection of collation-sensitive `TEXT` comparisons,
  `BETWEEN`, `IN`, truth predicates, ordering, `DISTINCT`,
  `COUNT(DISTINCT column)`, grouped columns, and numeric aggregates;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted `TEXT` family data;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL behavior.

## Non-Goals

This feature must not implement:

- `TEXT(M)`, `TINYTEXT(M)`, `MEDIUMTEXT(M)`, `LONGTEXT(M)`, `LONG`, or
  `LONG VARCHAR`;
- `BLOB`, `TINYBLOB`, `MEDIUMBLOB`, `LONGBLOB`, `BINARY`, `VARBINARY`, `CHAR`,
  `ENUM`, `SET`, `JSON`, or other deferred string/binary types;
- `TEXT` family primary keys, secondary indexes, prefix indexes, `FULLTEXT`
  indexes, or index prefix length handling;
- column-level `CHARACTER SET`, `COLLATE`, or `BINARY` attributes;
- literal defaults, expression defaults, `DEFAULT(col_name)`, or string default
  catalog storage;
- string-to-integer or integer-to-string DML conversion;
- string comparison predicates, `LIKE` over table data, `REGEXP`, collations,
  coercibility, `ORDER BY` over string columns, grouped string keys, string
  `DISTINCT`, or collation-aware uniqueness;
- adjacent string literal concatenation, national strings, introducers,
  hex/bit string values, parameters, user variables, functions, arbitrary
  expressions, or scalar string projection;
- embedded `NUL` result values, binary strings, or streaming reads/writes;
- `ALTER TABLE ... MODIFY [COLUMN]` or `CHANGE [COLUMN]` to or from `TEXT`
  family descriptors;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup. The public
  result API remains NUL-terminated text, so embedded `NUL` string values stay
  unsupported.
- Statement context owns per-statement diagnostics, warnings, affected rows,
  and transaction completion. Supported in-range `TEXT` family operations
  record `warning_count == 0`; supported `INSERT IGNORE` adjustments record
  warnings through the existing diagnostics area.
- Lexer/parser/AST own syntax admission for bare `TEXT` family column types
  and ordinary string literals. They store source spans and structural payloads
  only; they do not resolve catalog descriptors or perform storage conversion.
- Analyzer/planner code maps `TEXT` family AST nodes to durable descriptors,
  resolves schemas/tables/columns against the MyLite catalog, decodes admitted
  string literals, validates UTF-8, rejects embedded `NUL`, enforces the
  descriptor byte limit, rejects unsupported conversion or collation-sensitive
  operations, and produces descriptor-driven SQLite plans.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, defaults, and column order. This slice reuses the
  existing `logical_type` / `physical_type` descriptor fields and does not
  change `_mylite_catalog_columns` schema because string defaults are deferred.
- Result and introspection builders render logical descriptors to MySQL-shaped
  text. SQLite schema text and `sqlite_schema` are not metadata authority.
- SQLite owns physical row storage, scans, and mutations for generated prepared
  statements. MyLite binds string values with length-aware
  `sqlite3_bind_text(..., SQLITE_TRANSIENT)` and reads SQLite `TEXT` values for
  result rows.
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
    existing_integer_type
  | VARCHAR ( unsigned_decimal_integer_literal )
  | TINYTEXT
  | TEXT
  | MEDIUMTEXT
  | LONGTEXT
```

`TEXT(M)` and family-specific length arguments are not admitted in this slice.
Column-level charset, collation, and `BINARY` attributes are not admitted.
`TEXT` remains a nonreserved keyword in MyLite's keyword catalog and is still
admitted anywhere the current identifier grammar admits nonreserved keywords.
`TINYTEXT`, `MEDIUMTEXT`, and `LONGTEXT` remain reserved.

DML values for `TEXT` family targets reuse the existing string row-value
grammar:

```sql
insert_value:
    existing_integer_or_boolean_or_NULL_or_DEFAULT_value
  | string_literal

update_value:
    existing_integer_or_boolean_or_NULL_or_DEFAULT_value
  | string_literal
```

`string_literal` is an ordinary MySQL string token under MyLite's fixed default
SQL mode: single-quoted or double-quoted text with doubled quote characters and
backslash escapes decoded by MyLite. The admitted escape decoding follows the
existing `VARCHAR` runtime observations. National strings, introducers,
adjacent literal concatenation, hex literals, bit literals, parameters, and
expressions are not part of this slice.

The existing table-backed `WHERE` grammar is unchanged. `TEXT` family columns
are valid only for the already supported `IS NULL` and `IS NOT NULL` predicate
forms.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
column_definition ::=
    identifier column_type nullability_opt column_default_opt.

column_type ::= integer_type.
column_type ::= varchar_type.
column_type ::= text_type.

varchar_type ::= VARCHAR LPAREN INTEGER RPAREN.

text_type ::= TINYTEXT.
text_type ::= TEXT.
text_type ::= MEDIUMTEXT.
text_type ::= LONGTEXT.

insert_value ::= STRING.
update_value ::= STRING.
```

The parser may continue to reject unsupported forms as syntax errors. If a
shape is parsed for an existing wider statement family, the analyzer must
return deterministic unsupported diagnostics before generating SQLite SQL.

## Type and Value Semantics

Admitted `TEXT` family descriptors use these logical and physical forms:

| MySQL type | Logical descriptor | Physical descriptor | Maximum stored bytes |
| --- | --- | --- | --- |
| `TINYTEXT` | `TINYTEXT` | `TEXT` | `255` |
| `TEXT` | `TEXT` | `TEXT` | `65535` |
| `MEDIUMTEXT` | `MEDIUMTEXT` | `TEXT` | `16777215` |
| `LONGTEXT` | `LONGTEXT` | `TEXT` | `4294967295` |

The maximum is enforced as a byte count after string-literal escape decoding.
The value must also be valid UTF-8 and must not contain an embedded `NUL`,
matching the current public result surface. MyLite does not implement MySQL's
warning-producing trailing-space truncation in this slice; any decoded value
whose byte length exceeds the descriptor limit fails with data-too-long error
`1406` / SQLSTATE `22001`.

The current fixed character set/collation surface treats admitted `TEXT` family
columns as `utf8mb4` / `utf8mb4_0900_ai_ci` for metadata only. No collation
comparison behavior is implied by storage support.

`NULL` follows existing row-value policy:

- assigning `NULL` to a nullable `TEXT` family column stores SQL `NULL`;
- assigning `NULL` to a `TEXT NOT NULL` column fails with bad-null diagnostic;
- omitted nullable `TEXT` family columns and explicit DML `DEFAULT` on nullable
  `TEXT` family columns materialize SQL `NULL`;
- omitted or explicit DML `DEFAULT` on `TEXT NOT NULL` with no explicit default
  fails in strict mode;
- limited `INSERT IGNORE` adjusts strict `NULL` and no-default failures for
  `TEXT NOT NULL` to warnings and stores the empty string.

`SHOW CREATE TABLE` must omit `DEFAULT NULL` for `TEXT` family columns even
when the descriptor has MyLite's effective nullable default. This differs from
current `VARCHAR` rendering and matches MySQL's observed `TEXT` family surface.

## DML and Query Semantics

`INSERT`, `REPLACE`, and single-table `UPDATE` use the existing descriptor-
driven storage path. The assignment target must resolve to a MyLite descriptor
column. For `TEXT` family targets, the admitted values are `NULL`, `DEFAULT`,
and ordinary string literals. Numeric, boolean, expression, column-to-column,
function, parameter, hex, bit, decimal, and float values are rejected as
unsupported conversions for string targets.

Successful supported `UPDATE` statements report changed rows like MySQL:
assigning a different `TEXT` value or changing between `NULL` and non-`NULL`
counts as one changed row; assigning the same string value or existing `NULL`
counts as zero changed rows. `warning_count` is zero for supported in-range
updates.

`WHERE column IS NULL` and `WHERE column IS NOT NULL` are supported for `TEXT`
family columns through descriptor resolution and SQLite `IS NULL` / `IS NOT
NULL` predicates. Other `TEXT` family predicate forms are rejected because
collation, coercion, and string comparison semantics are not implemented.

Existing integer `ORDER BY` and `LIMIT` support may operate on tables that
contain `TEXT` family columns, including updates that assign `TEXT` values.
Ordering by a `TEXT` family column remains unsupported. No additional tie-order
claim is made beyond existing integer order behavior.

## Physical SQLite Handling

Generated physical user tables remain SQLite rowid tables unless an existing
feature explicitly documents a different invariant. `TEXT` family columns are
created as quoted SQLite `TEXT` columns. Generated SQL uses only descriptor-
derived physical table names such as `_mylite_user_table_<table_id>` and
descriptor physical column names. Every generated identifier is quoted. String
values are bound with prepared statements and `sqlite3_bind_text` using the
decoded byte length; SQL string literal text is never interpolated into
generated SQLite SQL.

This feature uses public SQLite prepare/bind/step APIs plus existing MyLite
wrapper/planner logic. It does not need a targeted SQLite fork patch.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar forms such as `TEXT(M)`,
  `TINYTEXT(M)`, explicit charset/collation attributes, and `BINARY`;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` target names through existing table-resolution policy;
- unsupported object kinds once non-base-table descriptors exist;
- unsupported primary keys, auto-increment attributes, and indexes on `TEXT`
  family columns;
- unknown assignment, predicate, and ordering columns through existing
  descriptor resolution;
- unsupported assignment values and unsupported string conversions;
- string values containing decoded `NUL` bytes;
- invalid UTF-8 for admitted string row values;
- overlength `TEXT` family values with data-too-long diagnostics;
- `NULL` into `TEXT NOT NULL` and no default for `TEXT NOT NULL`;
- unsupported `TEXT` comparison, `BETWEEN`, `IN`, truth predicates, ordering,
  grouping, distinct, and aggregate operations;
- physical SQLite failures and allocation failures;
- public API misuse only through existing public execution/result conventions.

## Compatibility Documentation

Update `COMPATIBILITY.md` and detailed compatibility docs to mark only the
limited `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT` baseline as supported
with gaps. Do not overclaim `BLOB`, `TEXT(M)`, string defaults, string
collations, string ordering, indexes, protocol metadata, or general expression
support.

## Test Plan

Add MySQL-runtime expectation coverage for:

- MySQL 8.4.9 version and strict default SQL mode guard;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  metadata for all four `TEXT` family types;
- inserts of ordinary string literals, empty strings, trailing spaces, and
  `NULL`;
- strict `NULL` and omitted-value diagnostics for `TEXT NOT NULL`;
- `INSERT IGNORE` adjustment for `TEXT NOT NULL` `NULL` and no-default inputs;
- strict overlength `TINYTEXT` diagnostics and deferred trailing-space
  truncation behavior;
- `UPDATE` changed-row counts, no-op updates, `NULL` assignment, filtered
  updates, and integer `ORDER BY ... LIMIT` updates over tables containing
  `TEXT` columns;
- unsupported MySQL-accepted forms that MyLite deliberately defers, including
  `TEXT(M)`, expression defaults, column charset/collation attributes, and
  `LONG` / `LONG VARCHAR`.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_text_type_test.c`, registered as `libmylite.runtime.text_type`. Cover:

- parser acceptance for bare `TEXT` family types and parser rejection or
  runtime rejection for deferred type forms;
- creation, insertion, selection, update, delete filtering by `IS NULL` /
  `IS NOT NULL`, rename/drop interactions, and reopen persistence;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` metadata;
- all four `TEXT` family descriptors and byte-limit enforcement using
  `TINYTEXT` as the fast overlength case;
- `INSERT`, `INSERT SET`, `REPLACE`, `REPLACE SET`, compatible
  `CREATE TABLE LIKE`, compatible `CREATE TABLE SELECT`, compatible
  `INSERT ... SELECT`, and compatible `REPLACE ... SELECT` paths;
- `UPDATE` changed rows and `ORDER BY` / `LIMIT` over integer keys while
  assigning `TEXT` values;
- nullability/no-default diagnostics and limited `INSERT IGNORE` adjustment;
- unsupported conversions, defaults, indexes/primary keys, auto-increment,
  string comparisons, string ordering, grouping, distinct, aggregate, and
  expression forms;
- independent handles, file preamble preservation, zero-initialized cleanup,
  and existing parser/runtime lifecycle regressions.

Verification before marking done:

1. `cmake --build --preset dev`
2. `ctest --test-dir build/dev --output-on-failure -R 'libmylite\\.(parser|runtime\\.text_type|runtime\\.varchar_type|runtime\\.information_schema_core)'`
3. `./packages/libmylite/tests/mysql_baseline_text_type_expectations.sh`
4. `cmake --workflow --preset check`
