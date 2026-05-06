# SQLite Fork SET Type Descriptors

## Status

Implemented as the second SQLite-fork value-list descriptor slice, extending the
`ENUM` payload/readback foundation to MySQL `SET`.

## References

- MySQL 8.4 Reference Manual, The `SET` Type:
  https://dev.mysql.com/doc/refman/8.4/en/set.html
- MySQL 8.4 Reference Manual, `ENUM` and `SET` Constraints:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-enum.html
- MySQL-runtime fixture:
  `docs/specs/sqlite-fork-set-type-descriptors/mysql-set-coercion.sql`

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 runtime behavior, SQLite public extension
documentation already cited by the extension-point map, and the current MyLite
codebase.

## Scope

This slice adds native SQLite-fork descriptors for `SET(...)` columns.

Implemented behavior:

- descriptor values are copied into SQLite schema column metadata and freed with
  the owning column
- descriptors accept 1 through 64 members, reject duplicate byte-identical
  members, and reject members containing commas
- assignment of `NULL` passes through unchanged
- assignment of `''` stores the empty mask, even when the descriptor contains a
  valid empty-string member
- assignment of an exact label stores that member bit
- assignment of a comma-separated label list stores the union of matching
  member bits; input order and duplicates do not affect stored value
- assignment of numeric values stores the corresponding bit mask after finite
  real values are truncated toward zero
- assignment of quoted integer text such as `'3'`, `'+3'`, `'03'`, and `' 3'`
  stores the corresponding mask when there is no exact label and no comma-list
  form
- exact numeric-looking labels win before numeric-mask fallback, so
  `SET('0','1','2')` assigns `'2'` as label bit 3 while numeric `2` assigns
  label bit 2
- selected values display as comma-joined labels in descriptor order while
  numeric contexts see the stored mask
- an empty-string member can be selected by numeric mask bit 1, but it has no
  visible display text
- the 64th MySQL `SET` member bit is accepted and displayed correctly
- strict invalid labels, malformed lists, non-integer numeric text, trailing
  text after a numeric string, and masks with bits outside the descriptor fail
  with MySQL condition 1265, SQLSTATE `01000`

Deferred behavior:

- MyLite parser/catalog support for `SET(...)` column declarations
- non-strict and `IGNORE` demotion that drops invalid list substrings and
  records warnings
- collation-aware label matching beyond exact byte matching
- SQL declaration-time warnings or errors for duplicate members after trailing
  space normalization
- direct SQLite parser/catalog descriptor reload after schema changes outside
  MyLite
- exact MySQL expression metadata and display formatting for wide numeric masks
  in every arithmetic context

## SQLite Extension Surface Evaluation

SQLite loadable extensions are not sufficient for transparent `SET` support.
A scalar function can convert a label list to a mask, and another function can
format a mask as labels, but every write and read path would then need custom
wrapper SQL. Collations do not help because the stored value is numeric while
the displayed value is text. Virtual tables would replace ordinary SQLite row
storage and would not preserve SQLite's native planner, transactions, indexes,
CTEs, and window functions for normal user tables.

The fork-owned extension point is therefore the same one established by
`ENUM`:

- store the value list on `Column`
- coerce assignments in `OP_MyliteTypeCheck` before `OP_MakeRecord`
- emit `OP_MyliteColumnReadType` for direct column expressions so selected
  values can carry both MySQL display text and numeric mask behavior

## Runtime Semantics

The public fork API adds `mylite_sqlite_fork_set_set_column_type()`. The input
value array is borrowed only for the duration of the call. On success, the
SQLite schema column owns copied strings. The descriptor is connection-local,
like the other current fork descriptors.

Write-time conversion stores a signed SQLite integer containing the MySQL set
mask bit pattern:

- `NULL` passes through unchanged
- empty text stores mask `0`
- exact full-label text stores one bit
- comma-list text stores all listed member bits, requiring every component to
  be a valid member and every component to be nonempty
- integer and finite real numeric values store their bit mask if every set bit
  is within the descriptor's member range
- integer-looking text stores a bit mask only when it is not an exact label and
  is not a comma list

Read-time conversion maps stored masks back to MySQL display text. Non-empty
selected labels are joined in descriptor order with commas. Empty selected
labels are invisible in display text, matching observed MySQL behavior for
`SET('', 'a')`. The numeric mask remains attached to the same SQLite `Mem`
value so `set_col + 0` can use the stored mask without generated wrapper SQL.

## Tests

The test suite covers:

- direct annotated SQLite `INSERT` and `UPDATE`
- label lists, reversed label order, duplicate label input, empty assignment,
  integer masks, finite real truncation, `NULL`, quoted integer text, signed
  quoted integer text, zero-padded integer text, and leading-space integer text
- exact-label precedence for numeric-looking `SET` members
- empty-string member behavior and numeric selection of an invisible empty
  member
- 64th member display through unsigned mask text
- strict invalid mask, unknown label, trailing-space numeric text, and decimal
  text diagnostics
- MySQL-runtime fixture diff against `mysql-set-coercion.expected.tsv`

## Follow-Up Work

- Wire MyLite's parser, AST, catalog, and DML write-table descriptor loader to
  `SET(...)` declarations.
- Add SQL-mode-aware warning demotion for invalid set assignments.
- Integrate label lookup with the column collation registry.
- Finish expression metadata and numeric-context fidelity for wide masks in
  all result shapes.
