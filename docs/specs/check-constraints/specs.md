# CHECK constraints

## Scope

This feature covers the implemented CHECK constraint slice for supported
`CREATE TABLE` and CHECK-only `ALTER TABLE` statements:

- inline and table-level `CHECK` clauses already recorded in
  `__mylite_check_constraint_catalog`
- `ALTER TABLE ... ADD CHECK`, `DROP CHECK` / `DROP CONSTRAINT`, and
  `ALTER CHECK` / `ALTER CONSTRAINT` for CHECK-only statements
- `ENFORCED` and default-enforced constraints on supported persistent and
  temporary base tables
- supported `INSERT`, `INSERT ... SET`, `INSERT ... ON DUPLICATE KEY UPDATE`,
  `INSERT IGNORE`, `REPLACE`, single-table `UPDATE`, `UPDATE IGNORE`, and
  joined `UPDATE`
- warning/error code 3819 behavior for covered DML paths

Mixed CHECK plus table-rebuild ALTER statements, full MySQL deterministic
expression validation, `CREATE TABLE` schema-level duplicate CHECK-name
validation, generated-column dependencies, and `SHOW CREATE TABLE` CHECK
formatting remain deferred.

## Sources

- MySQL 8.4 Reference Manual, CREATE TABLE and CHECK constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-check-constraints.html
- MySQL 8.4 Reference Manual, constraint `INFORMATION_SCHEMA` tables:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-information-schema.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 Behavior Summary

Runtime probes used a table shaped like:

```sql
CREATE TABLE checks_runtime (
  id INT PRIMARY KEY,
  score INT,
  note VARCHAR(10),
  CHECK (score > 0),
  CONSTRAINT chk_note CHECK (note <> '') NOT ENFORCED
);
```

Observed behavior:

- unnamed CHECK constraints are generated as `<table>_chk_<n>`
- `CHECK (score > 0)` rejects `score = -1` with error 3819 and message
  `Check constraint '<name>' is violated.`
- `score = NULL` is accepted because CHECK accepts `UNKNOWN`
- the `NOT ENFORCED` constraint is recorded but does not reject matching DML
- `INSERT IGNORE` and `UPDATE IGNORE` skip violating rows and append warning
  3819
- `INSERT ... ON DUPLICATE KEY UPDATE` validates the proposed insert row before
  duplicate-key handling; if that row violates CHECK, the update arm is not
  evaluated
- for the duplicate-key update arm, the updated row is checked before it is
  written
- `REPLACE` validates the candidate row before deleting conflicting rows
- `ALTER TABLE ... ADD CHECK` validates existing rows before recording enforced
  metadata; failures use error 3819 and do not leave catalog rows behind
- `ALTER TABLE ... ADD CHECK ... NOT ENFORCED` records metadata without
  validating existing rows
- `ALTER TABLE ... ALTER CHECK ... ENFORCED` validates existing rows and reports
  the table row count when enabling enforcement; `NOT ENFORCED` reports 0
- `ALTER TABLE ... DROP CHECK missing_name` fails with error 3821
- duplicate explicit CHECK names in a schema fail with error 3822
- generated ALTER CHECK names advance from the table's recorded ordinal
  sequence, so dropping an earlier generated check does not reuse its suffix

## MyLite Behavior

MyLite loads enforced CHECK constraints from the CHECK catalog for the target
table at DML execution time. Temporary tables use
`__mylite_temp_check_constraint_catalog`; otherwise persistent tables use
`__mylite_check_constraint_catalog`.

For each candidate row, MyLite evaluates the recorded CHECK clause against a
single-row SQLite projection of the candidate column values. This supports the
row-local expression subset currently emitted by MyLite's CHECK metadata path,
including column references, comparison operators, boolean composition, string
and numeric literals, and `NULL` propagation. A result of true or unknown
passes; false rejects the row.

Covered DML ordering follows the observed MySQL behavior:

1. resolve and coerce candidate values
2. validate enforced CHECK constraints
3. validate duplicate keys and foreign keys
4. write the row or update candidate

For `IGNORE` statements, CHECK failures append warning 3819, mark the row as
ignored, and continue the statement. Non-`IGNORE` failures set error 3819 and
abort the statement atomically.

CHECK-only `ALTER TABLE` statements use statement atomicity around catalog
updates and existing-row validation. `ADD CHECK` generates `<table>_chk_<n>`
from the table's maximum CHECK ordinal plus one, validates explicit names
against existing CHECK names in the target schema for the chosen persistent or
temporary catalog, writes `CHECK_CONSTRAINTS` and `TABLE_CONSTRAINTS` metadata,
and leaves `affected_rows` as 0. `ALTER CHECK ... ENFORCED` validates existing
rows before flipping metadata to `YES`; when enforcement changes from `NO` to
`YES`, MyLite reports the table row count as observed in MySQL 8.4.9. `DROP
CHECK` removes the table's CHECK row and reports 0.

## Tests

Runtime coverage:

- enforced CHECK accepts valid values and `NULL`
- `NOT ENFORCED` constraints are recorded but do not reject DML
- invalid `INSERT` fails with error 3819 and leaves rows unchanged
- invalid `INSERT IGNORE` skips the row with warning 3819
- invalid insert candidates in ODKU are checked before duplicate-key handling
- invalid ODKU update candidates fail before writing
- invalid `UPDATE` fails and invalid `UPDATE IGNORE` skips with warning 3819
- invalid `REPLACE` fails before deleting the conflicting row
- temporary-table CHECK constraints are enforced through the temporary catalog
- `ALTER TABLE ... ADD CHECK` records metadata and enforces later DML
- enforced `ADD CHECK` rejects invalid existing rows without catalog side
  effects
- `ADD CHECK ... NOT ENFORCED`, `ALTER CHECK ... ENFORCED`, and `ALTER CHECK
  ... NOT ENFORCED` toggle metadata and DML behavior
- `DROP CHECK` removes metadata and missing names use MySQL error 3821
- duplicate explicit ALTER CHECK names use MySQL error 3822
- generated ALTER CHECK names advance after dropping an earlier generated name
- temporary-table ALTER CHECK rows enforce through the temporary catalog

## Known Gaps

- CHECK expressions are evaluated through SQLite for this first slice; broader
  MySQL expression semantics, deterministic-function validation, character-set
  behavior, SQL-mode interactions, and exact coercion warnings remain deferred.
- Mixed CHECK plus column/index/table-option ALTER statements remain
  unsupported.
- `SHOW CREATE TABLE` does not yet render CHECK clauses.
- Schema-level duplicate CHECK-name validation is still incomplete for
  `CREATE TABLE`.
