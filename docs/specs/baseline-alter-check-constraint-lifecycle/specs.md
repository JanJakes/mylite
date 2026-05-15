# Baseline ALTER CHECK Constraint Lifecycle

## Status

This phase extends the descriptor-owned CHECK constraint surface from
`CREATE TABLE` to the first ALTER lifecycle operations for persistent MyLite
base tables:

- `ALTER TABLE table_name ADD [CONSTRAINT [name]] CHECK (expr) [[NOT] ENFORCED]`;
- `ALTER TABLE table_name DROP CHECK name`;
- `ALTER TABLE table_name ALTER CHECK name [NOT] ENFORCED`.

The slice preserves MyLite catalog descriptors as the metadata authority and
keeps CHECK enforcement synchronized with SQLite by rebuilding the physical
user table whenever the set of enforced physical CHECK clauses changes. It does
not add multi-action ALTER statements, `DROP CONSTRAINT`, `ALTER CONSTRAINT`,
temporary-table checks, or broader CHECK expression support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing CHECK descriptor design:
  `docs/specs/baseline-check-constraint-lifecycle/specs.md`
- Existing parser, catalog, table rebuild, metadata, and row-write code under
  `packages/libmylite/src/sql/` and `packages/libmylite/src/runtime/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE` CHECK clauses:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, CHECK constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-check-constraints.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-check-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849` using the existing repository expectation-script pattern.
The probe SQL and durable expectations are captured in
`packages/libmylite/tests/mysql_baseline_alter_check_constraint_lifecycle_expectations.sh`.

Observed behavior shaping this slice:

- `ALTER TABLE t ADD CHECK (expr)` creates an enforced CHECK by default and
  generates `<table>_chk_<n>`.
- `ALTER TABLE t ADD CONSTRAINT CHECK (expr)` is accepted and also generates a
  name.
- `ALTER TABLE t ADD CONSTRAINT name CHECK (expr) [NOT] ENFORCED` preserves the
  explicit name.
- Generated names use the first currently available table-local generated
  ordinal. A dropped generated name such as `<table>_chk_1` can be reused.
- If the generated logical name collides with any CHECK name in the schema,
  MySQL returns duplicate CHECK name instead of skipping to a later ordinal.
- CHECK names are unique per schema across CHECK constraints. The same CHECK
  name is allowed in another schema and may match index, unique-key, or
  foreign-key names.
- Adding an enforced CHECK validates all existing rows. `ROW_COUNT()` reports
  the number of rows scanned, including rows for which the expression evaluates
  to `UNKNOWN`. `@@warning_count` is `0` on success.
- Adding a `NOT ENFORCED` CHECK does not validate existing rows and reports
  `ROW_COUNT() = 0`, `@@warning_count = 0`.
- Enforcing a previously not-enforced CHECK validates all existing rows and
  reports the number of rows scanned. Unenforcing a CHECK reports
  `ROW_COUNT() = 0`.
- If existing rows violate an enforced add or an enforce operation, the
  statement fails with `3819 / HY000`, the CHECK catalog is unchanged, and
  later diagnostic reads observe MySQL's failed-DDL diagnostics snapshot.
- `DROP CHECK name` removes the descriptor and reports `ROW_COUNT() = 0`,
  `@@warning_count = 0`.
- Unknown `DROP CHECK` or `ALTER CHECK` names fail with `3821 / HY000` and
  message `Check constraint '<name>' is not found in the table.`
- Unsupported or invalid add expressions use the same MySQL diagnostics as the
  create-table CHECK slice except that an unknown column in an ALTER CHECK
  expression was observed as `1054 / 42S22` with message
  `Unknown column '<column>' in 'check constraint <name> expression'`.
- `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` reflect added, dropped, and toggled
  CHECK descriptors. CHECK lines are rendered in logical name order.

## Scope

Supported DDL:

- persistent base tables only;
- unqualified or schema-qualified table names using the existing selected
  schema policy;
- `ADD CHECK`, `ADD CONSTRAINT CHECK`, and `ADD CONSTRAINT name CHECK`;
- optional `ENFORCED`, omitted enforcement, and `NOT ENFORCED`;
- `DROP CHECK name`;
- `ALTER CHECK name ENFORCED` and `ALTER CHECK name NOT ENFORCED`;
- exact current CHECK expression subset from
  `baseline-check-constraint-lifecycle`;
- existing supported primary-key, unique-index, secondary-index,
  auto-increment, foreign-key, column metadata, table charset/collation, and
  CHECK descriptors preserved across the required physical rebuild.

Supported validation and enforcement:

- enforced add and enforcing toggle scan existing rows by rebuilding into a
  temporary physical table with the target enforced CHECK set;
- `NOT ENFORCED` add and idempotent enforcement toggles are descriptor-only;
- unenforcing an enforced CHECK and dropping an enforced CHECK rebuild to
  remove obsolete physical CHECK clauses;
- dropping a not-enforced CHECK is descriptor-only;
- successful enforced scans report the copied/scanned row count;
- successful nonvalidating changes report zero affected rows;
- existing DML enforcement remains SQLite physical CHECK enforcement mapped
  back through MyLite descriptors.

Supported metadata:

- descriptor-driven `SHOW CREATE TABLE` reflects add/drop/toggle immediately;
- `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` and
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` reflect add/drop/toggle immediately;
- generated CHECK names preserve their generated ordinal and name-generation
  flag.

Out of scope:

- temporary tables;
- multi-action ALTER statements;
- `DROP CONSTRAINT` and `ALTER CONSTRAINT`;
- `ALTER TABLE ... ADD CHECK` with algorithm/lock modifiers;
- deterministic function expansion, variables, subqueries, parameters,
  string/temporal/JSON CHECK expressions, generated columns, or broader
  expression evaluation;
- check-specific rewrites for `DROP COLUMN`, `RENAME COLUMN`, `MODIFY`,
  `CHANGE`, `ORDER BY`, or `FORCE` beyond preserving existing current
  rejections where they are not part of this slice;
- SQLite trigger-based enforcement, native SQLite schema reflection, or SQLite
  fork patches.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  the existing result accessors.
- Statement context: owns diagnostics, warning counts, affected rows,
  `ROW_COUNT()`, implicit DDL transaction behavior, cleanup, and rollback.
- Lexer/parser/AST: admits the new single-action ALTER CHECK syntax and carries
  the table name, optional CHECK name, expression tree, and enforcement mode.
  It does not resolve descriptors or inspect SQLite schema text.
- Analyzer/planner: resolves schema/table/columns, object kind, reserved names,
  generated names, duplicate schema CHECK names, existing CHECK lookups,
  expression admissibility, affected-row reporting class, and physical rebuild
  inputs from descriptors.
- Catalog: owns durable CHECK descriptors, names, physical names, enforcement
  flags, generated-name metadata, generated ordinals, descriptor versions, and
  catalog generations.
- Result and introspection builders: render `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` from descriptors, not SQLite schema
  text.
- SQLite physical storage: stores rows and enforces the physical CHECK clauses
  currently emitted by MyLite. Rebuild uses standard SQLite DDL/DML through
  MyLite-generated SQL; no fork patch is required.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Syntax

MyLite Lemon-syntax sketch for the admitted grammar:

```lemon
statement ::= alter_table_add_check_statement.
statement ::= alter_table_drop_check_statement.
statement ::= alter_table_alter_check_statement.

alter_table_add_check_statement ::=
    ALTER TABLE table_name ADD check_constraint_definition.

alter_table_drop_check_statement ::=
    ALTER TABLE table_name DROP CHECK identifier.

alter_table_alter_check_statement ::=
    ALTER TABLE table_name ALTER CHECK identifier check_enforcement_required.

check_constraint_definition ::=
    check_constraint_name_opt CHECK LPAREN expression RPAREN check_enforcement_opt.

check_constraint_name_opt ::= .
check_constraint_name_opt ::= CONSTRAINT.
check_constraint_name_opt ::= CONSTRAINT identifier.

check_enforcement_opt ::= .
check_enforcement_opt ::= ENFORCED.
check_enforcement_opt ::= NOT ENFORCED.

check_enforcement_required ::= ENFORCED.
check_enforcement_required ::= NOT ENFORCED.
```

`DROP CONSTRAINT` and `ALTER CONSTRAINT` remain unsupported in this slice even
though MySQL accepts them for CHECK constraints.

## Name Semantics

`ADD CHECK` uses the same logical-name rules as `CREATE TABLE` CHECK:

- explicit names are stored as provided after identifier decoding;
- omitted names are generated as `<table>_chk_<n>`;
- for `ALTER TABLE ... ADD CHECK`, generated ordinal `n` is the first
  currently unused generated ordinal on that table, so dropped generated names
  can be reused;
- for a new `CREATE TABLE`, generated ordinals are assigned monotonically
  within the new table definition;
- duplicate CHECK names are rejected schema-wide before physical SQL is
  generated;
- generated names are checked against the schema-wide CHECK namespace because
  another table can hold an explicit name that would collide.

`DROP CHECK` and `ALTER CHECK` resolve only descriptors on the target table.
Unknown target names fail with `3821 / HY000`.

## Expression Semantics

`ADD CHECK` reuses the current CHECK expression planner from
`baseline-check-constraint-lifecycle`. Supported expressions include
unqualified same-table integer columns, integer/boolean/`NULL` literals,
parentheses, unary `+`, unary `-`, unary `NOT`, arithmetic `+`, `-`, `*`,
comparisons, `<=>`, `IS NULL`, `IS NOT NULL`, `AND`, and `OR`.

The planner renders two independent strings:

- logical `check_clause`, used in MySQL-facing metadata;
- physical `sqlite_expression`, used inside generated SQLite CHECK clauses.

Unsupported expression categories keep the current MySQL-shaped diagnostics
where already verified. Unknown columns in `ALTER TABLE ... ADD CHECK` use the
observed ALTER-specific `1054 / 42S22` diagnostic instead of the create-table
`3820 / HY000` diagnostic.

## Physical SQLite Handling

SQLite cannot portably add, drop, or toggle table CHECK clauses with `ALTER
TABLE`. MyLite therefore rebuilds the physical table when the set of enforced
physical CHECK clauses changes:

1. Begin a SQLite immediate transaction for the physical rebuild.
2. Create a temporary physical table name derived from the stable user table id
   and current SQLite schema generation.
3. Emit a descriptor-built `CREATE TABLE` with all current descriptor columns
   and the target set of enforced CHECK descriptors.
4. Copy rows using `INSERT INTO temp(columns...) SELECT columns... FROM old`.
   For validating operations, SQLite rejects the copy if any row violates a
   newly enforced CHECK; MyLite maps the physical CHECK name back to the
   logical descriptor and reports `3819 / HY000`.
5. Drop the old physical table and rename the temporary table back to the
   stable physical name.
6. Recreate generated physical primary/secondary indexes from descriptors.
7. Commit the physical transaction and increment
   `sqlite_schema_generation`.

The rebuild SQL quotes every generated SQLite identifier. Literal values are
not interpolated because the rebuild copies existing columns and embeds only
descriptor-authored CHECK expressions that were independently rendered by the
CHECK planner.

Metadata-only changes update the descriptor catalog without rewriting physical
rows. The catalog mutation and physical rebuild are coordinated so failure
leaves the logical descriptor set and the physical table unchanged.

## Result Semantics

Successful ALTER CHECK statements return through the existing non-row statement
result conventions:

- no row result set;
- `warning_count == 0`;
- `affected_rows` / `ROW_COUNT()` equals the existing-row scan count for
  successful enforced add and successful `ALTER CHECK ... ENFORCED`;
- `affected_rows == 0` for successful `ADD ... NOT ENFORCED`, `DROP CHECK`,
  and `ALTER CHECK ... NOT ENFORCED`.

Failed statements use existing diagnostics and do not commit descriptor or
physical mutations.

## Diagnostics

The slice requires deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` table names;
- unsupported target object kind;
- duplicate CHECK name;
- unknown CHECK name for `DROP CHECK` / `ALTER CHECK`;
- unknown assignment/expression column in `ADD CHECK`;
- unsupported CHECK expression categories;
- non-boolean CHECK expression;
- CHECK reference to an auto-increment column;
- existing-row violation during enforced add or enforcing toggle;
- physical SQLite failures during rebuild or index recreation;
- allocation failures.

## Tests

Add a focused plain C runtime test, plus parser coverage if a new AST node is
added, under `packages/libmylite/tests/`. Expected MySQL behavior is captured
by `packages/libmylite/tests/mysql_baseline_alter_check_constraint_lifecycle_expectations.sh`.

Coverage must include:

- successful enforced add, not-enforced add, bare `CONSTRAINT CHECK`, explicit
  names, generated names, generated ordinal reuse after drop, generated-name
  schema collision, and schema-qualified targets;
- existing-row validation and affected-row counts;
- successful drop and enforcement toggle behavior;
- violating DML after add/toggle;
- metadata through `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`;
- unknown check names, duplicate names, unknown schemas/tables, reserved names,
  unsupported object kinds, invalid expressions, unknown columns, and
  auto-increment references;
- rebuild preservation for current supported columns, indexes, foreign keys,
  auto-increment counters, existing rows, persistence after reopen, and MyLite
  file preamble invariants;
- independent file-backed handles;
- cleanup on failed rebuild and zero-initialized cleanup.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\\.(parser|runtime\\.(check_constraint_lifecycle|alter_check_constraint_lifecycle|primary_key_lifecycle|secondary_index_lifecycle|foreign_key_constraints))' --output-on-failure
packages/libmylite/tests/mysql_baseline_alter_check_constraint_lifecycle_expectations.sh
cmake --workflow --preset check
```
