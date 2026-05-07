# Baseline Table Rename Lifecycle

## Status

This feature specifies the next narrow user-visible table lifecycle slice for
file-backed `.mylite` handles. It adds a limited `RENAME TABLE` statement on
top of the existing public `mylite_execute()` entry point, statement context,
parser scaffold, shifted `.mylite` storage, durable MyLite catalog, and basic
`CREATE TABLE` / `DROP TABLE` / `SHOW TABLES` lifecycle.

The feature is intentionally not full MySQL `RENAME TABLE` support. It supports
one persistent base table source and one target name. It preserves MyLite's
stable physical SQLite table name and updates only MyLite logical catalog
descriptors.

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
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `RENAME TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/rename-table.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for `RENAME TABLE old_name TO new_name`;
- one source table and one target table only;
- persistent MyLite base-table descriptors only;
- unqualified and schema-qualified source and target names;
- cross-schema moves when both source and target schemas exist;
- independent source-name and target-name default-schema resolution matching
  observed MySQL 8.4.9 behavior;
- reserved `_mylite_*` schema and table name rejection before generated SQLite
  physical SQL can be produced;
- unknown source, duplicate target, unknown schema, no selected schema, syntax,
  unsupported multi-table, unsupported object-kind, and catalog failure
  diagnostics;
- catalog descriptor mutation with statement-boundary rollback;
- descriptor generation advancement and descriptor-cache invalidation on
  success;
- descriptor-version advancement for the renamed table row only;
- unchanged column descriptors;
- descriptor-driven `SHOW TABLES` visibility of the new logical name and
  absence of the old logical name;
- reopen persistence and independent file-backed handle behavior;
- MySQL 8.4.9 runtime-verified expectations for the supported and deliberately
  rejected user-visible behavior.

## Non-Goals

This feature must not implement:

- multi-table `RENAME TABLE` execution;
- `ALTER TABLE ... RENAME TO`, `ALTER TABLE ... RENAME COLUMN`, `RENAME
  COLUMN`, or any other `ALTER TABLE` form;
- temporary tables, view rename execution, triggers, privileges, locks, foreign
  keys, routines, events, or `INFORMATION_SCHEMA`;
- general `SELECT`, `INSERT`, `UPDATE`, `DELETE`, joins, expressions, or
  arbitrary SQLite SQL pass-through;
- `TRUNCATE TABLE`;
- table rebuilds, indexes, keys, constraints, defaults, generated columns,
  table options, partitions, comments, or auto-increment state;
- reconstructing MyLite descriptors from SQLite schema text;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, and public misuse behavior.
- Statement context owns diagnostics reset, warning count, affected rows, and
  the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves source and target names against connection
  session state and MyLite catalog descriptors, rejects unsupported scope, and
  builds a fixed-size rename plan.
- The catalog module owns `_mylite_catalog_*` tables, table descriptor updates,
  descriptor-version changes, generation advancement, and descriptor-cache
  invalidation.
- SQLite owns durable b-tree storage and transaction rollback. SQLite schema
  text and `PRAGMA` output remain physical implementation details, not
  MySQL-visible metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call:

```sql
RENAME TABLE source_table TO target_table
```

`source_table` and `target_table` each use the existing table-name subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The following are rejected in this slice:

- `RENAME TABLE a TO b, c TO d`;
- `RENAME TABLE` with missing `TO` or either table name;
- `ALTER TABLE old_name RENAME new_name`;
- temporary-table syntax;
- table names with more than two identifier parts.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement ::= rename_table_statement.

rename_table_statement ::= RENAME TABLE table_name TO table_name.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing parser rule that treats a reserved word after `.` as an identifier
continues to apply.

## Schema Resolution

Source and target table names are resolved independently.

| SQL form | Source schema | Target schema | MyLite behavior |
| --- | --- | --- | --- |
| `RENAME TABLE old TO new` | selected schema | selected schema | Supported when a default schema is selected. Without one, fail with no selected schema. |
| `RENAME TABLE db.old TO new` | `db` | selected schema | Supported. Without a selected schema for `new`, fail with no selected schema. This may be a cross-schema move. |
| `RENAME TABLE old TO db.new` | selected schema | `db` | Supported when a default schema is selected for `old`. Without one, fail with no selected schema. This may be a cross-schema move. |
| `RENAME TABLE db.old TO other_db.new` | `db` | `other_db` | Supported when both schemas exist. This is a cross-schema move. |

Unknown schema names fail with MySQL error `1049`, SQLSTATE `42000`, and
message `Unknown database '<schema>'`.

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
generated. Since rename preserves the physical SQLite table name, this feature
generates no SQLite physical DDL, but the same reserved namespace policy is
kept for catalog and future physical safety.

## Catalog Behavior

`RENAME TABLE` reads the source table descriptor from the MyLite catalog,
requires `kind == MYLITE_CATALOG_TABLE_KIND_BASE`, checks that the target name
does not already exist in the resolved target schema, and updates the source
table descriptor.

On success:

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

The existing generated physical name policy is preserved:

```text
_mylite_user_table_<table_id>
```

## Physical SQLite Handling

No SQLite physical schema rename is required in this phase. MyLite does not use
the logical MySQL table name as the SQLite table name; it uses the stable
descriptor-owned `physical_name`. Because `RENAME TABLE` changes only logical
metadata, the physical table remains valid and continues to be addressed by
the unchanged physical name for future `DROP TABLE` and later DML lowering.

Generated SQLite identifiers must still be quoted wherever future code uses
the physical name. This feature does not issue SQLite `ALTER TABLE RENAME`,
does not rewrite SQLite schema text, and does not inspect `sqlite_master` as a
metadata authority.

Because no SQLite schema object changes, successful rename advances the MyLite
catalog generation but does not advance `sqlite_schema_generation`.

## Transaction And Failure Unwinding

Each supported `RENAME TABLE` statement runs inside one MyLite catalog mutation
transaction:

1. Resolve source and target schemas and logical table names.
2. Reject reserved names.
3. Read the source table descriptor.
4. Reject unsupported object kinds.
5. Reject an existing target table descriptor in the target schema.
6. Begin a catalog mutation transaction.
7. Update the source descriptor's `schema_id`, `name`, `descriptor_version`,
   and `updated_catalog_generation`.
8. Commit the mutation, advancing catalog generation.

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

Successful `RENAME TABLE` returns an empty DDL result:

- `column_count == 0`;
- `row_count == 0`;
- `affected_rows == 0`;
- `warning_count == 0`.

`SHOW TABLES` remains descriptor-driven. After a successful rename it lists the
new logical name and no longer lists the old logical name in the source schema.
For cross-schema moves, the old source schema no longer lists the table and the
target schema lists the new name.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported parsed scope, including multi-table rename | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported or syntax message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown source or target schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown source table | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Duplicate target table | `MYLITE_ERROR` | `1050`, `42S01`, `Table '<table>' already exists` |
| Reserved `_mylite_*` schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved `_mylite_*` table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unsupported object kind | `MYLITE_ERROR` | `1064`, `42000`, `RENAME TABLE supports only persistent base tables` |
| Catalog mutation failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal failure message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |

Warnings are not generated by the supported subset.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-07 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --force
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `RENAME TABLE old_name TO new_name` after selecting a schema | `Query OK, 0 rows affected`, no warnings; `SHOW TABLES` shows only `new_name`. |
| `RENAME TABLE old_name TO new_name` without `USE` | Error `1046`, SQLSTATE `3D000`, `No database selected`. |
| `RENAME TABLE db.old_name TO new_name` without `USE` | Error `1046`, SQLSTATE `3D000`, because the target is unqualified. |
| `RENAME TABLE db.old_name TO db.new_name` | `Query OK, 0 rows affected`, no warnings. |
| `RENAME TABLE db.old_name TO other_db.new_name` | `Query OK, 0 rows affected`, no warnings; the table moves between schemas. |
| `RENAME TABLE db.old_name TO new_name` while default schema is `other_db` | Succeeds and moves the table to `other_db.new_name`. |
| `RENAME TABLE old_name TO other_db.new_name` while default schema is `db` | Succeeds and moves the table to `other_db.new_name`. |
| `RENAME TABLE missing_source TO target_name` | Error `1146`, SQLSTATE `42S02`, `Table '<db>.missing_source' doesn't exist`. |
| `RENAME TABLE source_name TO existing_target` | Error `1050`, SQLSTATE `42S01`, `Table 'existing_target' already exists`. |
| `RENAME TABLE same_name TO same_name` | Error `1050`, SQLSTATE `42S01`. |
| `RENAME TABLE source_name TO missing_db.target_name` | Error `1049`, SQLSTATE `42000`. |
| `RENAME TABLE a TO b, c TO d` | Succeeds atomically in MySQL; MyLite rejects this form in this phase. |
| `RENAME TABLE view_source TO view_target` | MySQL renames the view; MyLite rejects non-base descriptors in this phase. |
| `RENAME TABLE temp_source TO temp_target` for a temporary table | Error `1146`, SQLSTATE `42S02`; temporary table rename is out of scope. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_table_rename_lifecycle_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support:

- `RENAME TABLE`: limited single-pair persistent base-table rename, including
  schema-qualified names and cross-schema moves between existing MyLite catalog
  schemas;
- base tables: logical descriptor rename with stable physical SQLite table
  names;
- atomic DDL: catalog rename commits or rolls back atomically for this subset.

Full multi-table atomic rename, `ALTER TABLE` rename forms, temporary tables,
view rename execution, trigger behavior, privilege checks, lock behavior, and
information-schema metadata remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.table_rename_lifecycle`.

Coverage must include:

- parser/AST acceptance for supported `RENAME TABLE old TO new` and
  schema-qualified forms;
- parser rejection for missing `TO`, missing names, `ALTER TABLE ... RENAME`,
  and multi-table rename syntax;
- successful unqualified rename after `USE`;
- successful schema-qualified same-schema rename;
- successful cross-schema rename;
- catalog descriptor rows after rename: schema/table logical names, stable
  table id, stable physical name, table descriptor version, generation changes,
  and unchanged column descriptors;
- physical SQLite table behavior inside the shifted payload without touching
  the MyLite preamble;
- reopen persistence for renamed descriptors and physical tables;
- `SHOW TABLES` before and after rename;
- failure unwinding for unknown source table, duplicate target table, no
  selected schema, unknown target schema, reserved source/target names,
  unsupported multi-table syntax, and induced catalog mutation failure;
- independent file-backed handles with independent rename state;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, client-data, and registration tests
  still pass.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.table_rename_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.basic_table_lifecycle)$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_table_rename_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor/physical-schema atomicity, file-format safety, VFS
preservation, zero-init safety, cleanup on failure, scope control,
compatibility-matrix accuracy, and test relevance.
