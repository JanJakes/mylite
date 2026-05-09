# Baseline Multi-Table Rename Lifecycle

## Status

This feature specifies the next narrow table-lifecycle slice after single-pair
`RENAME TABLE`: descriptor-driven multi-pair persistent base-table rename
through `mylite_execute()`.

The feature extends the existing parser, statement context, catalog, stable
physical table-name policy, and single-pair rename execution. It remains a
limited MyLite baseline slice, not full MySQL `RENAME TABLE` support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline multi-table drop lifecycle:
  `docs/specs/baseline-multi-table-drop-lifecycle/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `RENAME TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/rename-table.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, identifier case sensitivity:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-case-sensitivity.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for comma-separated `RENAME TABLE` pairs;
- two or more rename pairs in one statement, while preserving the existing
  single-pair behavior;
- persistent MyLite base-table descriptors only;
- unqualified and schema-qualified source and target names using the existing
  selected/default schema policy;
- cross-schema moves when referenced schemas exist;
- MySQL-compatible left-to-right rename semantics for the supported subset;
- statement atomicity: if any pair fails, no prior pair remains renamed;
- reserved `_mylite_*` schema/table rejection before catalog mutation;
- descriptor-driven target existence checks using MyLite catalog state, not
  SQLite schema text;
- stable physical SQLite names, unchanged physical table rows, and unchanged
  SQLite schema generation;
- result behavior matching the current non-row DDL result conventions:
  `affected_rows == 0`, no result rows, and `warning_count == 0` on success;
- MySQL 8.4.9 runtime-verified expectations for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- temporary tables, views, triggers, routines, events, foreign keys, locks,
  metadata-lock semantics, privilege checks, or `INFORMATION_SCHEMA` updates
  beyond MyLite's existing descriptor-backed behavior;
- `ALTER TABLE ... RENAME` forms;
- arbitrary SQLite SQL pass-through;
- physical SQLite table renames, table rebuilds, or SQLite fork patches;
- case-folding behavior for `lower_case_table_names` values other than the
  verified runtime's `0`;
- new object descriptors beyond existing persistent base tables.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation and result-handle ownership.
- Statement context owns diagnostics reset, warning count, affected rows, and
  statement-boundary result finalization.
- Lexer/parser/AST own syntax admission, source spans, and the rename-pair list
  shape. They remain independent of runtime, catalog, storage, and SQLite.
- Runtime analyzer/planner code resolves source and target names, enforces
  reserved-name and object-kind limits, and executes rename pairs inside one
  catalog mutation.
- The catalog module owns durable descriptor rows, descriptor versions, catalog
  generation, and descriptor-cache invalidation.
- SQLite owns durable transaction rollback for catalog rows and physical row
  storage. SQLite schema text is not the MySQL-visible metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not write bytes before the shifted SQLite database.

## Supported SQL Grammar

The feature admits:

```sql
RENAME TABLE source_table TO target_table
    [, source_table TO target_table] ...
```

Each table name uses the existing table-name subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

### MyLite Lemon-Syntax Snippet

This independently authored snippet describes the intended MyLite grammar
shape:

```lemon
rename_table_statement ::= RENAME TABLE rename_pair_list.

rename_pair_list ::= rename_pair.
rename_pair_list ::= rename_pair_list COMMA rename_pair.

rename_pair ::= table_name TO table_name.

table_name ::= identifier.
table_name ::= identifier DOT identifier.
```

The existing rule that treats a reserved word after `.` as an identifier
continues to apply.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_multi_table_rename_expectations.sh`
records these observations:

- `@@lower_case_table_names` is `0` on the verified runtime.
- `RENAME TABLE a TO c, b TO d` succeeds with `ROW_COUNT() == 0` and
  `@@warning_count == 0`.
- Cross-schema multi-rename succeeds when all schemas exist.
- Rename operations are applied left to right. A swap works through a temporary
  intermediate name, for example `s1 TO tmp, s2 TO s1, tmp TO s2`.
- If a later pair fails, earlier successful steps are rolled back and the
  original table names remain visible.
- `RENAME TABLE a TO b, b TO c` fails with `1050` because `b` exists when the
  first pair is evaluated.
- `RENAME TABLE a TO c, b TO c` fails with `1050` after the first pair would
  create `c`, then rolls back.
- `RENAME TABLE a TO c, a TO d` fails with `1146` because the second source
  lookup sees that `a` no longer exists after the first pair, then rolls back.
- A qualified source with an unqualified target and no selected schema fails
  with `1046`, before mutation.
- An unknown target schema fails with `1049`, before visible mutation.
- Case-distinct names such as `A` and `a` are distinct targets and sources on
  the verified runtime.

## Schema Resolution

Each source and target table name is resolved independently using the existing
single-pair rename policy:

- unqualified names require the selected/default schema;
- schema-qualified names use their explicit schema;
- unknown explicit schemas fail with `1049` and SQLSTATE `42000`;
- if any pair needs a selected schema and none is selected, the whole statement
  fails with `1046` and SQLSTATE `3D000`;
- cross-schema moves are supported when both schemas exist.

Resolution occurs before catalog mutation for schema names and reserved names.
Table existence and target availability are checked left to right inside the
catalog mutation so the state seen by each pair reflects earlier pairs in the
same statement.

## Catalog And Physical Behavior

Each pair updates one existing persistent base-table descriptor:

- `table_id` is unchanged;
- `physical_name` is unchanged;
- `schema_id` changes only for cross-schema moves;
- `name` becomes the target logical name;
- table `descriptor_version` increments once per successful pair;
- table `updated_catalog_generation` becomes the statement mutation
  generation;
- column descriptors are unchanged;
- durable catalog generation increments once for the statement;
- descriptor caches are invalidated only after successful commit.

The stable physical SQLite name, for example `_mylite_user_table_<table_id>`,
is not renamed. No SQLite physical DDL is generated, and
`sqlite_schema_generation` remains unchanged.

## Left-To-Right Atomic Execution

Supported multi-rename statements execute inside one MyLite catalog mutation
transaction.

For each pair in statement order:

1. Read the current source descriptor from the catalog state visible inside the
   mutation.
2. Reject a missing source with the existing table-does-not-exist diagnostic.
3. Reject non-base-table descriptors once such descriptors exist.
4. Check the current target name in the current target schema.
5. Reject an existing target with the existing table-exists diagnostic.
6. Update the source descriptor to the target logical name and schema.

If any pair fails, the mutation is rolled back. The rollback must restore all
prior pair effects and leave catalog generation, descriptor cache state,
SQLite schema generation, physical row data, and the `.mylite` preamble
unchanged.

This deliberately does not pre-detect all duplicate sources or targets. MySQL's
visible behavior for this subset follows left-to-right evaluation, so MyLite
should report the error produced by the failing pair in that order.

## Diagnostics

The feature reuses the existing single-pair diagnostics:

| Case | Diagnostic |
| --- | --- |
| Syntax outside the admitted grammar | `1064` / `42000` syntax error |
| No selected schema for an unqualified source or target | `1046` / `3D000` |
| Unknown explicit schema | `1049` / `42000` |
| Missing source table at that left-to-right step | `1146` / `42S02` |
| Existing target table at that left-to-right step | `1050` / `42S01` |
| Reserved `_mylite_*` schema or table | existing reserved-name diagnostic |
| Unsupported object kind | deterministic MyLite unsupported diagnostic |
| Allocation failure | existing out-of-memory diagnostic |
| Catalog mutation failure | existing internal/physical failure policy |

Successful supported multi-rename statements produce no warnings.

## Tests

Implementation tests must cover:

- parser AST shape for one pair and multiple pairs;
- syntax rejection for trailing commas, missing `TO`, and unsupported table-name
  shapes;
- successful multi-rename in one schema;
- cross-schema multi-rename;
- left-to-right swap through an intermediate name, preserving row data;
- atomic rollback for later missing source, existing target, repeated source,
  and repeated target cases;
- no default schema and unknown schema diagnostics;
- case-distinct table names under `@@lower_case_table_names = 0`;
- reserved names;
- descriptor version/generation behavior, unchanged physical names, unchanged
  columns, unchanged SQLite schema generation, preamble preservation, reopen
  persistence, and independent handles;
- no result rows, `affected_rows == 0`, and `warning_count == 0` for successes;
- unchanged existing single-pair rename, DDL, DML, parser, diagnostics, storage,
  and VFS tests.

## Compatibility Documentation

After implementation, update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-ddl.md`;
- existing table-rename spec wording that currently lists multi-table rename as
  out of scope, if needed.

The wording must remain limited: persistent base tables only; no temporary
tables, views, triggers, privileges, metadata locks, foreign keys, full MySQL
DDL semantics, or SQLite fork patches.
