# Baseline SHOW TABLE STATUS Metadata

## Status

This feature extends the existing descriptor-driven `SHOW TABLE STATUS` and
`INFORMATION_SCHEMA.TABLES` baseline with MyLite-owned table metadata that is
visible to applications inspecting MySQL-style table state.

The slice is deliberately not full InnoDB statistics support. It adds durable
descriptor timestamps for persistent base tables and a descriptor-derived
secondary-index size placeholder. It keeps the existing exact MyLite row count
and fixed data-length model, and it continues to omit views, temporary tables,
`SHOW TABLE STATUS ... WHERE`, privileges, partition status, checksums,
tablespaces, comments, and full optimizer/statistics behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `SHOW TABLE STATUS` introspection:
  `docs/specs/baseline-show-table-status-introspection/specs.md`
- Baseline `INFORMATION_SCHEMA` metadata:
  `docs/compatibility/metadata-information-schema.md`
- Baseline catalog foundation and table lifecycle:
  `docs/specs/baseline-catalog-foundation/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline DML lifecycle slices:
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-delete-lifecycle/specs.md`,
  `docs/specs/baseline-truncate-table-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW TABLE STATUS` and `INFORMATION_SCHEMA.TABLES` expose the same table
  status fields for ordinary base tables.
- `Create_time` is displayed in the current session time zone. Creating a
  table after `SET timestamp = 1700000000` produced
  `2023-11-14 22:13:20` at `time_zone = '+00:00'` and
  `2023-11-15 00:13:20` at `time_zone = '+02:00'`.
- `Update_time` is InnoDB-dependent. Inserts into small tables can make
  `Update_time` non-`NULL` and formatted in the current session time zone, but
  other observed immediate row-changing paths left it `NULL`.
- `Check_time` was `NULL` for the observed InnoDB base tables.
- A table with only a primary key reported `Index_length = 0` in the observed
  baseline. Tables with non-primary secondary or unique indexes reported
  `Index_length = 16384`, including an empty secondary-indexed table.
- The existing simple-table fields remained compatible with the previous
  baseline observations: `Engine = InnoDB`, `Version = 10`,
  `Row_format = Dynamic`, `Data_length = 16384`, `Max_data_length = 0`,
  `Data_free = 0`, `Checksum = NULL`, and empty `Create_options` and
  `Comment` cells.

The runtime behavior is intentionally treated as evidence for the narrow
metadata fields in this slice, not as a claim that MyLite can reproduce InnoDB
statistics volatility.

## Scope

The implementation must add:

- durable persistent-table catalog fields for creation time and last
  row-change time, stored as UTC epoch seconds;
- schema migration support for existing `.mylite` catalogs, with legacy tables
  receiving zero timestamp values that render as SQL `NULL`;
- table creation metadata using the current statement timestamp source, so
  `SET timestamp` and session test clocks can produce deterministic creation
  times;
- last row-change metadata for persistent base-table row mutations that
  actually affect rows in the current admitted DML paths;
- `SHOW TABLE STATUS` rendering of descriptor-owned `Create_time` and
  `Update_time` in the current session `time_zone`;
- `INFORMATION_SCHEMA.TABLES` rendering of the same table timestamps;
- descriptor-derived `Index_length = 16384` when the table has at least one
  non-primary index descriptor, and `0` otherwise;
- shared helper behavior so `SHOW TABLE STATUS` and
  `INFORMATION_SCHEMA.TABLES` do not drift for row count, average row length,
  data length, index length, auto-increment, timestamps, collation, and fixed
  placeholders.

## Non-Goals

This feature must not implement:

- full InnoDB persistent statistics, estimates, optimizer statistics, page
  accounting, `ANALYZE TABLE`, `CHECK TABLE`, checksums, comments, tablespaces,
  partitions, temporary-table status rows, views, privilege filtering, or
  `SHOW TABLE STATUS ... WHERE`;
- exact InnoDB `Update_time` volatility for every operating-system and
  storage-engine path;
- timestamp metadata for temporary tables;
- table-option/comment syntax or rendering beyond the current empty
  placeholders;
- additional SQL grammar, parser changes, public API changes, arbitrary SQLite
  metadata reads, PRAGMA-driven statistics, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to return
  non-row result objects for DML and row result objects for metadata queries
  through the existing public result conventions.
- Statement context owns statement time. Table creation timestamps use the same
  statement timestamp source as current temporal functions, including the
  existing `SET timestamp` override used by tests.
- Runtime owns row-mutation side effects. After successful persistent base-table
  mutations with positive affected rows, runtime updates the table descriptor's
  last row-change time.
- The catalog owns durable table metadata fields and migrations. Creation and
  update timestamp fields are descriptor data, not SQLite schema text.
- The existing result builder owns `SHOW TABLE STATUS` row construction and
  `INFORMATION_SCHEMA.TABLES` rows. This slice should centralize common status
  formatting where practical.
- SQLite remains the physical row store. MyLite continues to use descriptor
  physical table names for row counts and DML execution; no SQLite catalog or
  PRAGMA data becomes authoritative for MySQL metadata.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. Catalog
  migrations must preserve the preamble and normal shifted SQLite invariants.

## Grammar

No new SQL syntax is admitted by this feature. The existing grammar remains:

```lemon
statement ::= show_table_status_statement.

show_table_status_statement ::=
    SHOW TABLE STATUS show_schema_clause_opt show_like_clause_opt.

show_schema_clause_opt ::= .
show_schema_clause_opt ::= FROM identifier.
show_schema_clause_opt ::= IN identifier.

show_like_clause_opt ::= .
show_like_clause_opt ::= LIKE STRING.
```

`SHOW TABLE STATUS ... WHERE` remains outside this slice even though MySQL
accepts it. It should be implemented later with the same expression-filtering
discipline used for other descriptor-backed metadata.

## Catalog Metadata

Persistent table descriptors gain:

- `created_time_utc_epoch`: an `INTEGER NOT NULL` UTC epoch-second value.
- `updated_time_utc_epoch`: an `INTEGER NOT NULL` UTC epoch-second value.

New tables set `created_time_utc_epoch` to the creation timestamp and
`updated_time_utc_epoch` to `0` until row-changing DML or table maintenance
touches the table. Existing catalogs upgraded by migration set both fields to
`0`; `0` renders as SQL `NULL`.

`created_time_utc_epoch` is stable across row DML and table rename. DDL that
rebuilds or recreates a table may later choose MySQL-compatible creation-time
replacement behavior, but this slice must not broaden DDL semantics beyond
paths it tests.

`updated_time_utc_epoch` is updated for persistent base-table row-changing DML
when the statement reports a positive affected-row count. `UPDATE` follows the
existing MyLite affected-row semantics: no-op assignments with zero affected
rows do not update the metadata. `TRUNCATE TABLE` updates the field because it
rewrites table contents even though MySQL reports affected rows as zero for
the statement.

Temporary tables are backed by the session-local temporary catalog and remain
omitted from `SHOW TABLE STATUS` and durable `INFORMATION_SCHEMA.TABLES`.

## Timestamp Formatting

`SHOW TABLE STATUS.Create_time`,
`SHOW TABLE STATUS.Update_time`,
`INFORMATION_SCHEMA.TABLES.CREATE_TIME`, and
`INFORMATION_SCHEMA.TABLES.UPDATE_TIME` format nonzero UTC epoch seconds as
`YYYY-MM-DD HH:MM:SS` after applying the session `time_zone` offset. The
format is the same current timestamp display format used by existing temporal
functions.

`CHECK_TIME` remains SQL `NULL`.

Creation timestamps use the current statement timestamp source. Row-change
timestamps use the wall-clock time of the mutation because observed MySQL
8.4.9 row-change `Update_time` behavior is not controlled by `SET timestamp`.
Tests must verify visible shape and time-zone rendering without pretending to
own full InnoDB update-time volatility.

## Status Row Values

For each supported persistent base table, MyLite emits:

| Column | MyLite value in this slice |
| --- | --- |
| `Name` / `TABLE_NAME` | Logical descriptor table name |
| `Engine` / `ENGINE` | `InnoDB` |
| `Version` / `VERSION` | `10` |
| `Row_format` / `ROW_FORMAT` | `Dynamic` |
| `Rows` / `TABLE_ROWS` | Exact current physical row count as decimal text |
| `Avg_row_length` / `AVG_ROW_LENGTH` | `0` when rows are zero; otherwise `floor(16384 / rows)` |
| `Data_length` / `DATA_LENGTH` | `16384` |
| `Max_data_length` / `MAX_DATA_LENGTH` | `0` |
| `Index_length` / `INDEX_LENGTH` | `16384` when at least one non-primary index descriptor exists; otherwise `0` |
| `Data_free` / `DATA_FREE` | `0` |
| `Auto_increment` / `AUTO_INCREMENT` | SQL `NULL` for fresh implicit auto-increment tables and non-auto tables; the durable next counter after implicit counters advance; the explicit table-option status value when available |
| `Create_time` / `CREATE_TIME` | Formatted descriptor creation time, or SQL `NULL` for legacy zero values |
| `Update_time` / `UPDATE_TIME` | Formatted descriptor update time, or SQL `NULL` for legacy zero values |
| `Check_time` / `CHECK_TIME` | SQL `NULL` |
| `Collation` / `TABLE_COLLATION` | Descriptor table default collation |
| `Checksum` / `CHECKSUM` | SQL `NULL` |
| `Create_options` / `CREATE_OPTIONS` | Empty string |
| `Comment` / `TABLE_COMMENT` | Empty string |

The fixed size fields are MyLite-owned deterministic placeholders. They are not
SQLite page counts and not full InnoDB statistics. `Auto_increment` predicates
evaluate the same metadata value that is rendered in the result row.

## DML Side Effects

The current supported persistent base-table DML paths must touch update time
after successful row changes:

- `INSERT ... VALUES`
- `INSERT ... SET`
- supported `INSERT ... SELECT`
- `REPLACE`
- supported single-table `UPDATE`
- supported single-table `DELETE`
- `TRUNCATE TABLE`

Mutation-time catalog updates must not change descriptor versions, catalog
generation, schema generation, or `sqlite_schema_generation`; they are status
metadata updates comparable to the existing auto-increment counter updates.

Failed statements and statements with zero affected rows must not change
`updated_time_utc_epoch`, except for `TRUNCATE TABLE`, whose table-content
rewrite is visible as an update-time touch.

## Diagnostics

This feature does not add new SQL diagnostics. Existing diagnostics remain
authoritative for syntax errors, missing schemas, `information_schema` access,
unknown tables, unsupported object kinds, physical SQLite failures, allocation
failures, and public API misuse.

Internal formatting failures report runtime errors through the existing
statement diagnostics. Catalog migration failures make opening the `.mylite`
file fail with the existing catalog initialization diagnostics.

## SQLite Handling

The implementation stays in MyLite wrapper/runtime/catalog code:

- use public SQLite APIs for catalog table migrations and descriptor metadata
  updates;
- continue to execute row counts against descriptor physical table names;
- quote generated identifiers and bind catalog update values;
- do not inspect `sqlite_schema`, PRAGMA statistics, or SQLite index pages;
- do not add SQLite fork patches for this metadata slice.

## Performance

This slice adds a small catalog update after successful persistent row-changing
DML. It must not materialize table data in memory to compute metadata. Row
counts keep the existing `COUNT(*)` behavior already used by `SHOW TABLE
STATUS` and `INFORMATION_SCHEMA.TABLES`; index length is descriptor-derived
without scanning rows or SQLite indexes.

## Tests

Fast C tests must cover:

- `Create_time` rendered in `SHOW TABLE STATUS`;
- `Create_time` rendered in `INFORMATION_SCHEMA.TABLES`;
- `SET timestamp` and `SET time_zone` effects on creation-time rendering;
- `Update_time` after successful persistent `INSERT`, `UPDATE`, `DELETE`, and
  `TRUNCATE TABLE` paths;
- no update-time change after a zero-affected-row `UPDATE` or `DELETE`;
- persistence of timestamp metadata after close/reopen;
- preservation across table rename;
- `Index_length = 0` for no-index and primary-key-only tables;
- `Index_length = 16384` for supported non-primary secondary and unique
  indexes;
- matching `SHOW TABLE STATUS` and `INFORMATION_SCHEMA.TABLES` values for the
  fields this slice owns;
- no temporary-table status rows;
- no `.mylite` preamble mutation outside existing catalog/row payload writes.

MySQL 8.4.9 expectation artifacts must verify the runtime observations that
define this slice: session-time-zone rendering, `SET timestamp` creation-time
behavior, `Update_time` shape, secondary-index `Index_length`, and shared
`SHOW TABLE STATUS` / `INFORMATION_SCHEMA.TABLES` fields.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-show-statements.md`, and
`docs/compatibility/metadata-information-schema.md` only for the exact partial
metadata surface above. Do not claim full table statistics, views, temporary
tables, `WHERE` filters, comments, storage-engine page accounting, privileges,
or complete InnoDB timestamp volatility.
