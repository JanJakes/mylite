# CHECK constraints

## Scope

This feature covers the first enforcement slice for CHECK constraints recorded
by `CREATE TABLE`:

- inline and table-level `CHECK` clauses already recorded in
  `__mylite_check_constraint_catalog`
- `ENFORCED` and default-enforced constraints on supported persistent and
  temporary base tables
- supported `INSERT`, `INSERT ... SET`, `INSERT ... ON DUPLICATE KEY UPDATE`,
  `INSERT IGNORE`, `REPLACE`, single-table `UPDATE`, `UPDATE IGNORE`, and
  joined `UPDATE`
- warning/error code 3819 behavior for covered DML paths

`ALTER TABLE ... ADD/DROP/ALTER CHECK`, full MySQL deterministic expression
validation, schema-level duplicate CHECK-name validation, generated-column
dependencies, and `SHOW CREATE TABLE` CHECK formatting remain deferred.

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

## Known Gaps

- CHECK expressions are evaluated through SQLite for this first slice; broader
  MySQL expression semantics, deterministic-function validation, character-set
  behavior, SQL-mode interactions, and exact coercion warnings remain deferred.
- `ALTER TABLE ... ADD/DROP/ALTER CHECK` remains unsupported.
- `SHOW CREATE TABLE` does not yet render CHECK clauses.
- Schema-level duplicate CHECK-name validation is still incomplete.
