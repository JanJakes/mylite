# Baseline ALTER TABLE RENAME TO Lifecycle

## Status

This feature specifies the next narrow table DDL slice for file-backed
`.mylite` handles. It adds descriptor-driven `ALTER TABLE ... RENAME` table
rename execution on top of `mylite_execute()`, statement context, the MyLite
parser scaffold, shifted `.mylite` storage, durable catalog descriptors, and
the existing `RENAME TABLE` lifecycle.

The feature is intentionally not full MySQL `ALTER TABLE` support. It supports
one table-level rename action for persistent base tables. It reuses MyLite's
logical descriptor rename path, keeps stable SQLite physical table names, and
does not implement table rebuilds, column changes, index changes, options,
locks, algorithms, or combined alter actions.

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
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline truncate table lifecycle:
  `docs/specs/baseline-truncate-table-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `RENAME TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/rename-table.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for `ALTER TABLE table_name RENAME table_name`;
- parser and AST support for the MySQL-accepted `RENAME TO` and `RENAME AS`
  spellings;
- one source table and one target name only;
- one rename action only;
- persistent MyLite base-table descriptors only;
- unqualified and schema-qualified source and target names;
- cross-schema moves when both source and target schemas exist;
- independent target-name default-schema resolution matching observed MySQL
  8.4.9 behavior;
- reserved `_mylite_*` schema and table name rejection before any generated
  SQLite physical SQL can be produced;
- unknown source, duplicate target, unknown schema, no selected schema, syntax,
  unsupported combined-action, unsupported object-kind, and catalog failure
  diagnostics;
- successful same-source/same-target no-op behavior for `ALTER TABLE`, unlike
  the existing `RENAME TABLE` duplicate-target behavior;
- catalog descriptor mutation with statement-boundary rollback for real renames;
- descriptor generation advancement and descriptor-cache invalidation on real
  rename success;
- unchanged descriptor state for same-object no-op success;
- unchanged column descriptors;
- descriptor-driven `SHOW TABLES`, `SHOW CREATE TABLE`, `SELECT`, `INSERT`,
  `UPDATE`, `DELETE`, and `TRUNCATE` visibility through the renamed logical
  name;
- reopen persistence and independent file-backed handle behavior;
- MySQL 8.4.9 runtime-verified expectations for the supported and deliberately
  rejected user-visible behavior.

## Non-Goals

This feature must not implement:

- general `ALTER TABLE` parsing or execution;
- combined `ALTER TABLE` actions such as `RENAME ..., ADD COLUMN ...`;
- multiple `RENAME` clauses on one `ALTER TABLE` statement;
- `ALTER TABLE ... RENAME COLUMN`, `CHANGE COLUMN`, `MODIFY COLUMN`, `ADD
  COLUMN`, `DROP COLUMN`, index, key, constraint, option, partition,
  tablespace, algorithm, lock, validation, or visibility actions;
- `ALTER ONLINE TABLE`, `ALTER TABLE ... WAIT`, or `NOWAIT`;
- temporary tables, views, triggers, privileges, metadata locks, foreign keys,
  routines, events, or `INFORMATION_SCHEMA`;
- table rebuilds, generated columns, defaults, checks, auto-increment state, or
  privilege migration;
- reconstructing MyLite descriptors from SQLite schema text;
- SQLite fork patches.

MySQL accepts several of these wider forms. MyLite rejects them in this phase
because their semantics require later descriptor, table-rebuild, temporary
object, or metadata-lock work.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, and public misuse behavior.
- Statement context owns diagnostics reset, warning count, affected rows, the
  previous-row-count handoff, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves source and target names against connection
  session state and MyLite catalog descriptors, rejects unsupported scope, and
  builds a fixed-size rename plan.
- The catalog module owns `_mylite_catalog_*` tables, table descriptor updates,
  descriptor-version changes, generation advancement, and descriptor-cache
  invalidation.
- The result builder owns the empty DDL result returned for supported
  successful renames and no-ops.
- SQLite owns durable b-tree storage and transaction rollback. SQLite schema
  text and `PRAGMA` output remain physical implementation details, not
  MySQL-visible metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call:

```sql
ALTER TABLE source_table RENAME target_table
ALTER TABLE source_table RENAME TO target_table
ALTER TABLE source_table RENAME AS target_table
```

`source_table` and `target_table` each use the existing table-name subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The following are rejected in this slice:

- `ALTER TABLE source RENAME target, ADD COLUMN c INT`;
- `ALTER TABLE source ADD COLUMN c INT, RENAME target`;
- `ALTER TABLE source RENAME target, RENAME final_target`;
- `ALTER TABLE source RENAME TABLE target`;
- `ALTER TABLE source RENAME COLUMN old_column TO new_column`;
- `ALTER TABLE source RENAME INDEX old_index TO new_index`;
- every other `ALTER TABLE` action or modifier.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= alter_table_rename_statement.

alter_table_rename_statement ::= ALTER TABLE table_name RENAME table_rename_connector_opt table_name.

table_rename_connector_opt ::= .
table_rename_connector_opt ::= TO.
table_rename_connector_opt ::= AS.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply.

## Schema Resolution

Source and target table names are resolved as separate table names. Qualified
names use their explicit schema. Unqualified names use the selected schema.

Target default-schema resolution is visible for this statement. MySQL 8.4.9
reports `No database selected` for a qualified source with an unqualified target
when no default schema is selected, even if the qualified source schema is
unknown. MyLite must therefore detect a missing selected schema for an
unqualified target before reporting explicit source-schema existence errors.

| SQL form | Source schema | Target schema | MyLite behavior |
| --- | --- | --- | --- |
| `ALTER TABLE old RENAME new` | selected schema | selected schema | Supported when a default schema is selected. Without one, fail with no selected schema. |
| `ALTER TABLE db.old RENAME new` | `db` | selected schema | Supported. Without a selected schema for `new`, fail with no selected schema. This may be a cross-schema move. |
| `ALTER TABLE old RENAME db.new` | selected schema | `db` | Supported when a default schema is selected for `old`. Without one, fail with no selected schema. This may be a cross-schema move. |
| `ALTER TABLE db.old RENAME other_db.new` | `db` | `other_db` | Supported when both schemas exist. This is a cross-schema move. |

Unknown schema names fail with MySQL error `1049`, SQLSTATE `42000`, and
message `Unknown database '<schema>'`, except for the missing-target-default
case described above.

Cross-schema moves update the MyLite table descriptor's `schema_id` and
`name`. The physical SQLite table remains in the same shifted payload with the
same stable physical name.

## Identifier And Reserved Name Handling

Identifiers follow the existing table-lifecycle rules:

- AST spans are copied into owned, NUL-terminated internal buffers.
- Backtick-quoted identifiers are unquoted and doubled backticks are collapsed.
- User-authored schema and table names beginning with `_mylite_`, using ASCII
  case-insensitive comparison, are reserved.

Reserved source or target names are rejected before any SQLite physical DDL is
generated. Since this feature preserves the physical SQLite table name, it
generates no SQLite physical DDL, but the same reserved namespace policy is kept
for catalog and future physical safety.

This slice preserves the current catalog case-insensitive name lookup and
duplicate policy. Same-object detection uses resolved schema ids and catalog
name comparison. Broader platform-specific table-name case behavior remains a
future identifier-policy feature.

## Catalog Behavior

`ALTER TABLE ... RENAME` reads the source table descriptor from the MyLite
catalog, requires `kind == MYLITE_CATALOG_TABLE_KIND_BASE`, checks the target
name, and updates the source table descriptor unless the resolved target is the
same object.

For a real rename, on success:

- `table_id` is unchanged;
- `physical_name` is unchanged;
- `schema_id` changes only for cross-schema moves;
- `name` becomes the target logical name;
- table `descriptor_version` increments by `1`;
- table `created_catalog_generation` is unchanged;
- table `updated_catalog_generation` becomes the mutation generation;
- all column descriptors are unchanged, including column ids, names, logical
  types, physical types, nullability, descriptor versions, and generation
  fields;
- durable catalog generation increments by `1`;
- connection-local catalog generation is refreshed;
- descriptor caches are invalidated.

For a same-object no-op, on success:

- no catalog row is mutated;
- catalog generation is unchanged;
- descriptor versions and generation fields are unchanged;
- descriptor caches keep their previous state;
- SQLite schema generation is unchanged;
- result status is still a successful empty DDL result.

The existing generated physical name policy is preserved:

```text
_mylite_user_table_<table_id>
```

## Physical SQLite Handling

No SQLite physical schema rename is required in this phase. MyLite does not use
the logical MySQL table name as the SQLite table name; it uses the stable
descriptor-owned `physical_name`. Because `ALTER TABLE ... RENAME` changes only
logical metadata, the physical table remains valid and continues to be
addressed by the unchanged physical name for future DML and DDL lowering.

Generated SQLite identifiers must still be quoted wherever future code uses the
physical name. This feature does not issue SQLite `ALTER TABLE RENAME`, does
not rewrite SQLite schema text, and does not inspect `sqlite_master` as a
metadata authority.

Because no SQLite schema object changes, successful real renames advance the
MyLite catalog generation but do not advance `sqlite_schema_generation`.
Successful no-op renames advance neither generation.

## Transaction And Failure Unwinding

Each supported real rename runs inside one MyLite catalog mutation transaction:

1. Reject unsupported syntax before runtime execution.
2. Resolve the target's selected-schema dependency if the target is
   unqualified.
3. Resolve source and target schemas and logical table names.
4. Reject reserved names.
5. Read the source table descriptor.
6. Reject unsupported object kinds.
7. If source and target resolve to the same schema id and table name, return a
   successful no-op.
8. Reject an existing target table descriptor in the target schema.
9. Begin a catalog mutation transaction.
10. Update the source descriptor's `schema_id`, `name`, `descriptor_version`,
    and `updated_catalog_generation`.
11. Commit the mutation, advancing catalog generation.

If any step after the mutation begins fails, MyLite rolls back the transaction.
Rollback must leave the source descriptor readable by its old logical name,
leave the target descriptor absent, leave column descriptors unchanged, and
leave the durable catalog generation unchanged.

There is no physical SQLite DDL step to unwind. If a future implementation adds
physical rename or rebuild work, the catalog update and physical SQLite work
must stay in the same statement transaction or savepoint.

This phase preserves the `.mylite` preamble because all catalog changes occur
inside the shifted SQLite payload through the existing VFS. The preamble bytes
at physical offsets `[0, 4096)` are not touched after file open.

## Result Behavior

Successful `ALTER TABLE ... RENAME` returns an empty DDL result:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows == 0`;
- `warning_count == 0`.

`ROW_COUNT()` observes `0` after supported successful base-table renames and
same-object no-ops. MySQL reports row counts for some temporary-table rename
paths; temporary tables are out of scope for this phase.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported parsed scope, including combined actions | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported or syntax message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown source or target schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown source table | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Duplicate target table | `MYLITE_ERROR` | `1050`, `42S01`, `Table '<table>' already exists` |
| Same source and target object | `MYLITE_OK` | successful no-op |
| Reserved `_mylite_*` schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved `_mylite_*` table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unsupported object kind | `MYLITE_ERROR` | `1064`, `42000`, `ALTER TABLE RENAME supports only persistent base tables` |
| Catalog mutation failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal failure message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |

Warnings are not generated by the supported subset.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-08 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --skip-column-names
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `ALTER TABLE old_name RENAME new_name` after selecting a schema | `Query OK, 0 rows affected`, no warnings; `SHOW TABLES` shows only `new_name`. |
| `ALTER TABLE old_name RENAME TO new_name` | Same visible behavior as bare `RENAME`. |
| `ALTER TABLE old_name RENAME AS new_name` | Same visible behavior as bare `RENAME`. |
| `ALTER TABLE old_name RENAME new_name` without `USE` | Error `1046`, SQLSTATE `3D000`, `No database selected`. |
| `ALTER TABLE db.old_name RENAME new_name` without `USE` | Error `1046`, SQLSTATE `3D000`, because the target is unqualified. |
| `ALTER TABLE missing_db.old_name RENAME new_name` without `USE` | Error `1046`, SQLSTATE `3D000`, because the unqualified target requires a selected schema before source-schema existence is reported. |
| `ALTER TABLE db.old_name RENAME db.new_name` | `Query OK, 0 rows affected`, no warnings. |
| `ALTER TABLE db.old_name RENAME other_db.new_name` | `Query OK, 0 rows affected`, no warnings; the table moves between schemas. |
| `ALTER TABLE db.old_name RENAME new_name` while default schema is `other_db` | Succeeds and moves the table to `other_db.new_name`. |
| `ALTER TABLE old_name RENAME other_db.new_name` while default schema is `db` | Succeeds and moves the table to `other_db.new_name`. |
| `ALTER TABLE missing_source RENAME target_name` | Error `1146`, SQLSTATE `42S02`, `Table '<db>.missing_source' doesn't exist`. |
| `ALTER TABLE source_name RENAME existing_target` | Error `1050`, SQLSTATE `42S01`, `Table 'existing_target' already exists`. |
| `ALTER TABLE same_name RENAME same_name` | Succeeds as a no-op with `ROW_COUNT() == 0` and no warnings. |
| `ALTER TABLE source_name RENAME missing_db.target_name` | Error `1049`, SQLSTATE `42000`. |
| `ALTER TABLE view_source RENAME view_target` | Error `1347`, SQLSTATE `HY000`, because the object is not a base table. MyLite has no view descriptors yet and rejects non-base descriptors for this slice. |
| `ALTER TABLE temp_source RENAME temp_target` for a temporary table | Succeeds in MySQL, with `ROW_COUNT()` reflecting rows copied for non-empty temporary tables. Temporary tables are out of scope. |
| `ALTER TABLE a RENAME b, ADD COLUMN c INT` | Succeeds in MySQL; MyLite rejects combined actions in this phase. |
| `ALTER TABLE a RENAME b, RENAME c` | Succeeds in MySQL; MyLite rejects multiple actions in this phase. |
| `ALTER TABLE old_name RENAME TABLE new_name` | Syntax error `1064`, SQLSTATE `42000`. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_alter_table_rename_to_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support:

- `ALTER TABLE ... RENAME`: limited single persistent base-table rename action,
  including bare `RENAME`, `RENAME TO`, and `RENAME AS` spellings;
- `RENAME TO`: table rename through `ALTER TABLE` for unqualified,
  schema-qualified, and cross-schema names between existing MyLite catalog
  schemas;
- base tables: logical descriptor rename with stable physical SQLite table
  names;
- atomic DDL: catalog rename commits or rolls back atomically for this subset.

Full `ALTER TABLE`, combined actions, temporary tables, view rename execution,
table rebuilds, triggers, privilege checks, lock behavior, algorithm clauses,
foreign-key/check constraint side effects, and information-schema metadata
remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.alter_table_rename_to`.

Coverage must include:

- parser/AST acceptance for `ALTER TABLE source RENAME target`, `RENAME TO`,
  `RENAME AS`, and schema-qualified source/target forms;
- parser rejection for missing names, missing rename target, `RENAME TABLE`,
  combined actions, multiple rename actions, column rename, index rename, and
  unrelated `ALTER TABLE` actions;
- successful unqualified rename after `USE`;
- successful schema-qualified same-schema rename;
- successful cross-schema rename;
- qualified source with unqualified target using the current default schema;
- unqualified source with qualified target;
- no selected schema for unqualified source or target;
- explicit missing source schema with unqualified target and no selected schema
  returning no-selected-schema before unknown-schema;
- unknown source schema with qualified target;
- unknown source table;
- unknown target schema;
- duplicate target table;
- same source/target no-op preserving catalog generation, descriptor versions,
  physical table rows, and `sqlite_schema_generation`;
- real rename catalog descriptor changes: schema/table logical names, stable
  table id, stable physical name, table descriptor version, generation changes,
  and unchanged column descriptors;
- physical SQLite table behavior inside the shifted payload without touching
  the MyLite preamble;
- row data and DML visibility after rename;
- reopen persistence for renamed descriptors and physical rows;
- update/drop/truncate behavior after rename;
- `SHOW TABLES` and `SHOW CREATE TABLE` before and after rename;
- failure unwinding for duplicate target, unknown target schema, reserved
  source/target names, unsupported combined syntax, and induced catalog
  mutation failure;
- independent file-backed handles with independent rename state;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, table rename lifecycle, row values,
  update, truncate, client-data, and registration tests still pass.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.alter_table_rename_to$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.(table_rename_lifecycle|basic_table_lifecycle|row_values_lifecycle|update_lifecycle|truncate_table_lifecycle))$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_alter_table_rename_to_expectations.sh
cmake --workflow --preset check
```

Then review the diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor/physical-schema atomicity, file-format safety, VFS
preservation, zero-init safety, cleanup on failure, scope control,
compatibility-matrix accuracy, and test relevance.
