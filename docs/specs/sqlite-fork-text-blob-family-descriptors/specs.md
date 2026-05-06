# SQLite Fork Text And Blob Family Descriptors

## Status

Implemented as a SQLite-fork assignment descriptor slice for MySQL text and
blob families.

## References

- MySQL 8.4 Reference Manual, String Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, The `BLOB` and `TEXT` Types:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL-runtime fixture:
  `docs/specs/sqlite-fork-text-blob-family-descriptors/mysql-text-blob-family-coercion.sql`

## Scope

This slice extends MyLite's private SQLite fork with native row-write
descriptors for:

- `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT`
- `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and `LONGBLOB`

The descriptor path covers public MyLite `INSERT`, `UPDATE`, `REPLACE`, and
`ON DUPLICATE KEY UPDATE`, plus direct annotated SQLite writes through the fork
API. It enforces strict byte-capacity assignment, converts numeric values using
SQLite's scalar string representation, preserves text storage for text-family
columns, stores blob-family values as SQLite BLOB values, and publishes MySQL
condition 1406 with SQLSTATE `22001` for over-length assignment.

Deferred scope:

- non-strict truncation and `INSERT IGNORE` warning demotion
- exact MySQL error messages with row interpolation
- complete binary-string comparison behavior across every planner path
- direct SQLite parser/catalog descriptor reload without the MyLite catalog
  bridge
- storage above SQLite's configured maximum string/blob length for `LONGTEXT`
  and `LONGBLOB`

## SQLite Extension Surface Evaluation

SQLite's public scalar-function surface is not sufficient for this feature.
Every write path would have to wrap assigned expressions manually, and direct
SQLite writes through annotated schema objects would still bypass the wrappers.
CHECK constraints can reject lengths, but they cannot canonicalize storage
class, cannot consistently stringify numeric input at the assignment boundary,
and cannot participate in MyLite's MySQL diagnostics bridge.

The existing MyLite column descriptor and `OP_MyliteTypeCheck` VDBE fork point
is the right integration point. It is per-column schema metadata, runs once at
record creation for every physical write path, and leaves ordinary SQLite
affinity untouched for columns without MyLite descriptors.

## Semantics

The text-family descriptor converts non-`NULL` values to UTF-8 text and checks
the resulting byte length against the cataloged family capacity. This is
different from `VARCHAR(N)`, where MyLite checks character count. For example,
under `utf8mb4`, `REPEAT('é', 127)` fits in `TINYTEXT` because it is 254
bytes, but `REPEAT('é', 128)` is rejected because it is 256 bytes.

The blob-family descriptor converts non-`NULL` numeric inputs to text bytes,
preserves existing text/blob bytes, checks byte length against the cataloged
capacity, and stores the result with SQLite BLOB storage class. Unlike
`BINARY(N)`, blob-family columns do not right-pad shorter values.

The cataloged capacities are:

| Family | Capacity |
| --- | ---: |
| `TINYTEXT`, `TINYBLOB` | 255 bytes |
| `TEXT`, `BLOB` | 65,535 bytes |
| `MEDIUMTEXT`, `MEDIUMBLOB` | 16,777,215 bytes |
| `LONGTEXT`, `LONGBLOB` | 4,294,967,295 bytes, subject to SQLite's configured maximum value size |

`NULL` remains `NULL`; ordinary `NOT NULL` enforcement owns nullability.

## MyLite Integration

The public fork ABI adds `MYLITE_SQLITE_FORK_COLUMN_TYPE_TEXT` and
`MYLITE_SQLITE_FORK_COLUMN_TYPE_BLOB`. Both use `byte_maximum_length` for the
family capacity.

`mylite_dml_load_write_table()` maps catalog rows whose `DATA_TYPE` is one of
the text families to the text descriptor, and blob families to the blob
descriptor. The existing insert-value resolver already preserves quoted values
for these families as text, while the descriptor still catches numeric
expressions and copied row values uniformly at write time.

## Tests

The test suite covers:

- direct annotated SQLite descriptor `INSERT` and `UPDATE`
- direct text-family multibyte byte-capacity rejection
- direct blob-family byte-capacity rejection
- public MyLite `INSERT`, `UPDATE`, `ON DUPLICATE KEY UPDATE`, and `REPLACE`
- numeric-to-text and numeric-to-blob assignment
- UTF-8 byte length versus character length for `TINYTEXT`
- strict over-length diagnostics for text and blob families
- MySQL-runtime fixture diff against
  `mysql-text-blob-family-coercion.expected.tsv`

## Follow-Up Work

- Add SQL-mode-aware truncation and warning demotion.
- Preserve embedded zero bytes through the public MyLite literal/bind path.
- Add direct SQLite parser descriptor attachment when the SQLite parser fork
  starts accepting MySQL declarations natively.
- Revisit `LONGTEXT` and `LONGBLOB` limits alongside SQLite compile-time
  maximum value-size policy.
