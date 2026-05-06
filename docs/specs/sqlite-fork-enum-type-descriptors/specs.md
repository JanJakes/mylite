# SQLite Fork ENUM Type Descriptors

## Status

Implemented as the first SQLite-fork descriptor-payload and read-type slice for
MySQL `ENUM`.

## References

- MySQL 8.4 Reference Manual, The `ENUM` Type:
  https://dev.mysql.com/doc/refman/8.4/en/enum.html
- MySQL-runtime fixture:
  `docs/specs/sqlite-fork-enum-type-descriptors/mysql-enum-coercion.sql`

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 runtime behavior, SQLite public extension
documentation already cited by the extension-point map, and the current MyLite
codebase.

## Scope

This slice extends MyLite's private SQLite fork with two foundational pieces:

- owned per-column descriptor payloads for type metadata that needs value
  lists, beginning with `ENUM`
- a VDBE read-time column hook for MySQL types whose physical SQLite storage is
  not identical to their displayed value or numeric-context value

Implemented `ENUM` behavior:

- descriptor values are copied into the SQLite schema column and freed with the
  column metadata
- assignment of an exact label stores its one-based internal index
- numeric assignment truncates finite real values toward zero before index
  validation
- quoted integer text such as `'2'`, `'+2'`, and `'02'` maps to an index only
  when it is not an exact label
- exact numeric-looking labels win before numeric index fallback, so
  `ENUM('0','1','2')` assigns `'2'` as label index 3 while numeric `2`
  assigns label index 2
- valid empty-string labels are distinct from the later non-strict error value
  index 0 because the physical representation is the numeric index
- direct selected values display their labels while arithmetic sees the numeric
  index
- strict invalid labels and invalid indexes fail with MySQL condition 1265,
  SQLSTATE `01000`

Deferred behavior:

- MyLite parser/catalog support for `ENUM(...)` column declarations
- non-strict and `IGNORE` demotion to index 0 plus warning records
- collation-aware label matching beyond exact byte matching
- duplicate label diagnostics at SQL declaration time
- `SET`, which should reuse the descriptor-payload infrastructure but stores a
  bit mask instead of a one-based index

## SQLite Extension Surface Evaluation

SQLite loadable extensions cannot provide this transparently. A scalar
function can map labels to indexes, but every DML lowering path would need to
wrap assignments manually and every read path would need another wrapper to
display labels. Collations do not help because the physical storage and numeric
context semantics differ from the displayed string. Virtual tables would
replace SQLite's ordinary table storage and lose the point of the fork.

The fork-owned extension point is therefore:

- store the enum value list on `Column`
- coerce assignments in `OP_MyliteTypeCheck` before `OP_MakeRecord`
- emit a read-type opcode for column expressions so selected enum values can
  carry both the displayed string and numeric index

This keeps SQLite's planner, storage, indexes, transactions, CTEs, and window
functions available while putting MySQL semantics in the write/read hot paths.

## Runtime Semantics

The public fork API adds `mylite_sqlite_fork_set_enum_column_type()`. The input
value array is borrowed only for the duration of the call. On success, the
SQLite schema column owns copied strings. The descriptor is connection-local,
like existing scalar descriptors.

Write-time conversion stores an integer index:

- `NULL` passes through unchanged
- an exact label stores index `1..N`
- an integer or finite real numeric value stores index `1..N` after truncation
  toward zero
- quoted integer text stores index `1..N` when no exact label matches
- invalid text, zero, negative indexes, and indexes above `N` are strict
  conversion failures in this slice

Read-time conversion maps stored indexes back to labels. It sets the text
representation to the label and keeps the integer representation as the enum
index. That makes `SELECT enum_col` display like MySQL and makes
`enum_col + 0` return the MySQL index without requiring generated wrapper SQL.

## Tests

The test suite covers:

- direct annotated SQLite `INSERT` and `UPDATE`
- label, numeric index, quoted integer, signed quoted integer, zero-padded
  quoted integer, real-number truncation, and `NULL` assignments
- exact-label precedence for numeric-looking enum values
- valid empty labels as index 1
- direct selected labels plus numeric-context indexes
- strict invalid index and invalid-label diagnostics
- MySQL-runtime fixture diff against `mysql-enum-coercion.expected.tsv`

## Follow-Up Work

- Wire MyLite's parser, AST, catalog, and DML write-table descriptor loader to
  `ENUM(...)` declarations.
- Reuse the payload storage for `SET(...)` with MySQL bit-mask assignment and
  display rules.
- Add SQL-mode-aware warning demotion for invalid enum assignments.
- Integrate label lookup with the column collation registry.
