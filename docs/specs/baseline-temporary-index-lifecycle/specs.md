# Baseline Temporary Index Lifecycle

## Summary

This phase extends the existing temporary-table baseline with post-create
secondary index DDL on session temporary tables:

```sql
CREATE INDEX k ON tmp (col)
CREATE UNIQUE INDEX u ON tmp (col)
ALTER TABLE tmp ADD INDEX k (col)
ALTER TABLE tmp ADD UNIQUE KEY u (col)
DROP INDEX k ON tmp
ALTER TABLE tmp DROP INDEX k
```

The implementation reuses MyLite's descriptor-owned secondary and unique index
planning, validation, physical SQLite index generation, duplicate checking, and
metadata rendering. The new ownership boundary is the mutation destination:
temporary-table indexes are stored in the connection-local temporary catalog,
not in durable catalog rows.

This is not full MySQL temporary index DDL. The slice admits the currently
implemented secondary/unique key-part and option subset only, excludes
temporary fulltext creation and temporary spatial creation, and keeps temporary
indexes invisible to `INFORMATION_SCHEMA` just like existing temporary tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- Temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- Standalone and `ALTER TABLE` persistent index lifecycle specs:
  `docs/specs/baseline-create-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`,
  `docs/specs/baseline-drop-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `CREATE INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `DROP INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/drop-index.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE TEMPORARY TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-temporary-table.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_temporary_index_lifecycle_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `CREATE INDEX`, `CREATE UNIQUE INDEX`, `ALTER TABLE ... ADD INDEX`, and
  `ALTER TABLE ... ADD UNIQUE KEY` succeed on InnoDB temporary tables.
- Successful post-create temporary-table index creation reports
  `ROW_COUNT() == current_temp_row_count` and `@@warning_count == 0` for
  ordinary admitted forms. Empty temporary tables report zero rows.
- `DROP INDEX` and `ALTER TABLE ... DROP INDEX` on temporary tables also report
  the current temporary table row count and zero warnings for ordinary admitted
  forms.
- `SHOW CREATE TABLE` and `SHOW INDEX` reflect post-create temporary indexes.
- `INFORMATION_SCHEMA.STATISTICS` omits temporary tables and their indexes.
- A same-schema temporary table hides a persistent table with the same name for
  index creation and drop; dropping the temporary table reveals the unchanged
  persistent table metadata.
- Creating a unique index over duplicate non-`NULL` values fails with
  `1062 / 23000`; duplicate `NULL` values are permitted.
- `CREATE FULLTEXT INDEX` on an InnoDB temporary table fails with
  `1796 / HY000` and message text containing
  `Cannot create FULLTEXT index on temporary InnoDB table`.
- MySQL accepts `CREATE SPATIAL INDEX` on an eligible temporary spatial table,
  reporting the current row count and one warning for the verified empty-table
  probe. MyLite defers this because the existing temporary-table lifecycle does
  not admit temporary spatial index descriptors.
- `CREATE INDEX` and `DROP INDEX` on temporary tables behave as index DDL for
  transaction boundaries: the verified probes preserve preceding persistent
  writes after a later `ROLLBACK`.

## Scope

Supported:

- visible session temporary base tables created by the existing temporary-table
  lifecycle;
- `CREATE INDEX index_name ON table_name (key_part[, ...])`;
- `CREATE UNIQUE INDEX index_name ON table_name (key_part[, ...])`;
- `ALTER TABLE table_name ADD INDEX|KEY [index_name] (key_part[, ...])`;
- `ALTER TABLE table_name ADD UNIQUE [INDEX|KEY] [index_name] (key_part[, ...])`;
- `DROP INDEX index_name ON table_name`;
- `ALTER TABLE table_name DROP INDEX|KEY index_name`;
- unqualified and schema-qualified target table names using the current
  selected-schema policy and temporary-table shadowing;
- the existing descriptor-driven secondary and unique index key-part subset
  already supported for persistent `CREATE INDEX` / `ALTER TABLE ADD INDEX`,
  including supported full-column key parts, supported prefix key parts,
  composite key parts, optional `ASC` / `DESC`, supported `USING` options,
  index comments, and visibility metadata where the current persistent planner
  admits them;
- duplicate index-name, bad `PRIMARY` name, unknown column, unsupported key
  target, duplicate unique-value, and auto-increment supporting-key drop
  diagnostics inherited from the existing descriptor planners;
- temp-only affected-row reporting equal to the current physical table row
  count for successful add/drop index DDL;
- `SHOW CREATE TABLE`, `SHOW INDEX`, descriptor-driven DML duplicate checking,
  `CREATE TEMPORARY TABLE ... LIKE`, temporary `CREATE TABLE ... SELECT`,
  and close-time temporary cleanup seeing the new temporary descriptors;
- no-row result shape with warning count matching the underlying admitted
  index options;
- no durable catalog rows, no catalog generation mutation, and no `.mylite`
  file-format change.

Deferred:

- temporary fulltext index creation beyond MySQL-compatible rejection;
- temporary spatial index creation;
- temporary primary-key add/drop through post-create index DDL;
- `ALTER TABLE ... RENAME INDEX`, `ALTER TABLE ... ALTER INDEX
  VISIBLE|INVISIBLE`, and `ALTER TABLE ... DROP PRIMARY KEY` on temporary
  tables;
- temporary foreign keys, check constraints, triggers, generated columns,
  optimizer statistics, metadata locks, privileges, binary logging, replication,
  and complete transaction side effects;
- anything not already admitted by the current persistent secondary/unique
  index grammar and planner.

## Ownership Boundaries

- Public API: no ABI change. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warnings, `ROW_COUNT()` state,
  implicit-commit handling, and successful no-row result publication.
- Lexer/parser/AST: no new grammar is needed. The existing independently
  authored index DDL AST is reused.
- Analyzer/planner: resolves the target through visible writable table
  resolution so temporary descriptors shadow durable descriptors; resolves key
  parts from MyLite descriptors rather than SQLite metadata; preserves existing
  unsupported-shape diagnostics.
- Durable catalog: remains authoritative only for persistent indexes and is not
  mutated for temporary index DDL.
- Temporary catalog: owns session-local temporary index descriptors,
  index-column descriptors, generated negative IDs, generated physical index
  names, append/delete mutations, and close-time cleanup.
- Result builder/introspection: existing descriptor-driven `SHOW INDEX` and
  `SHOW CREATE TABLE` render the temporary descriptors; existing
  `INFORMATION_SCHEMA` paths stay durable-only.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite creates or drops ordinary SQLite indexes on
  generated temporary-table physical names. SQLite schema text is not metadata
  authority.

## Supported SQL Grammar

The parser already admits the needed statement shapes. This phase changes
target resolution and mutation behavior only.

MyLite Lemon-syntax sketch for the reused shapes:

```lemon
create_index_statement(A) ::=
    CREATE INDEX identifier(N) ON table_name(T)
    LPAREN secondary_index_part_list(P) RPAREN index_option_tail(O). {
    A = mylite_sql_parser_make_create_index_statement(
        state, false, N, T, P, O);
}

create_index_statement(A) ::=
    CREATE UNIQUE INDEX identifier(N) ON table_name(T)
    LPAREN secondary_index_part_list(P) RPAREN index_option_tail(O). {
    A = mylite_sql_parser_make_create_index_statement(
        state, true, N, T, P, O);
}

alter_table_action(A) ::=
    ADD secondary_index_definition(D). {
    A = mylite_sql_parser_make_alter_table_add_index_action(state, D);
}

drop_index_statement(A) ::= DROP INDEX identifier(N) ON table_name(T). {
    A = mylite_sql_parser_make_drop_index_statement(state, N, T);
}

alter_table_action(A) ::= DROP INDEX identifier(N). {
    A = mylite_sql_parser_make_alter_table_drop_index_action(state, N);
}
```

The implementation must not inspect SQLite grammar support for the logical
surface. Unsupported wider MySQL forms remain rejected by the existing parser
or planner.

## Resolution Semantics

Target resolution uses the same visible table policy as descriptor-driven DML:

1. An unqualified table requires a selected schema.
2. A schema-qualified table requires an existing durable schema descriptor.
3. Reserved `_mylite_*` schema or table names are rejected before physical SQL
   generation.
4. A session temporary table with the effective schema and table name is chosen
   before any persistent table lookup.
5. If no temporary descriptor exists, the persistent base-table index path is
   unchanged.

Temporary index names and key-part column names use the existing descriptor
identifier comparison policy. Current catalog comparisons are ASCII
case-insensitive for index-name collision checks where existing persistent
index behavior already does so; table and schema lookup remain aligned with
the existing descriptor catalog policy rather than introducing a new collation.

Unknown schemas, missing default schemas, unknown tables, duplicate index names,
unknown key columns, and bad key targets must use the same deterministic
diagnostics as the corresponding persistent path unless this spec lists a
temporary-specific diagnostic.

## Temporary Descriptor Mutation

Adding a temporary index must:

1. Plan and validate the index using the current descriptor-loaded temporary
   table columns and indexes.
2. For unique indexes, validate existing rows before mutation using the current
   MyLite-owned duplicate-key semantics.
3. Allocate a negative temporary index ID and generated physical name
   `_mylite_temp_index_<n>`.
4. Allocate negative temporary index-column IDs for every key part.
5. Execute physical SQLite `CREATE [UNIQUE] INDEX` for non-metadata-only
   admitted indexes.
6. Append copied index and index-column descriptors to the connection-local
   temporary catalog.
7. Bump only `session.sqlite_schema_generation` when a physical SQLite index is
   created.

Dropping a temporary index must:

1. Resolve the target and index from temporary descriptors when a temporary
   table shadows the name.
2. Reject `PRIMARY` through the existing `DROP INDEX` primary-key diagnostic.
3. Preserve auto-increment supporting-key validation for temporary tables.
4. Execute physical SQLite `DROP INDEX` for non-metadata-only temporary
   indexes.
5. Delete the matching temporary index descriptor and all matching index-column
   descriptors from the connection-local temporary catalog.
6. Bump only `session.sqlite_schema_generation` when a physical SQLite index is
   dropped.

Temporary mutations must not insert, update, or delete durable
`_mylite_catalog_*` rows and must not change durable table identity,
descriptor versions, catalog generation, or the file preamble.

## Physical SQLite Handling

Generated SQL uses only MyLite-generated and quoted identifiers:

```sql
CREATE [UNIQUE] INDEX "_mylite_temp_index_N"
ON "_mylite_temp_table_M" ("physical_column" [DESC], ...);

DROP INDEX "_mylite_temp_index_N";
```

Prefix key parts reuse the existing MyLite-generated expression shape over
generated physical column identifiers. Assignment values, predicates, and
application SQL literals are not interpolated into index DDL.

This is MyLite wrapper/translation over public SQLite APIs. No SQLite fork
patch is required.

## Result Semantics

Successful temporary-table index add/drop statements return through the
existing non-row result conventions:

- no result rows;
- `affected_rows` equal to the current physical temporary table row count;
- warning count equal to the planned admitted index-option warnings, normally
  zero for ordinary `BTREE` forms;
- `ROW_COUNT()` observes the affected-row value for the completed statement.

Persistent index add/drop result behavior is unchanged.

## Diagnostics

The feature must cover deterministic diagnostics for:

- syntax errors and unsupported grammar from the existing parser;
- missing default schema;
- unknown schema;
- unknown target table;
- writes to `information_schema`;
- reserved `_mylite_*` schema/table names before physical SQL generation;
- unsupported non-base object kinds once visible non-base descriptors exist;
- duplicate index names;
- bad explicit index name `PRIMARY`;
- unknown key-part columns;
- unsupported key-part targets;
- duplicate unique-index values;
- attempts to drop `PRIMARY` through `DROP INDEX`;
- attempts to drop the last auto-increment supporting key;
- unknown indexes;
- temporary fulltext index creation with MySQL-compatible
  `1796 / HY000`;
- deferred temporary spatial index creation, currently through the existing
  deterministic parse or unsupported diagnostic for the non-admitted shape;
- physical SQLite failures;
- allocation failures;
- public API misuse through existing `mylite_execute()` behavior.

## Tests

Fast C tests must cover:

- `CREATE INDEX` and `CREATE UNIQUE INDEX` on nonempty and empty temporary
  tables, with affected rows and warnings;
- `ALTER TABLE ... ADD INDEX` and `ALTER TABLE ... ADD UNIQUE KEY` on
  temporary tables, with affected rows and metadata;
- `DROP INDEX` and `ALTER TABLE ... DROP INDEX` on temporary tables, with
  affected rows and metadata removal;
- `SHOW CREATE TABLE` and `SHOW INDEX` rendering added temporary indexes;
- `INFORMATION_SCHEMA.STATISTICS` omission for temporary indexes;
- unique duplicate rejection and duplicate `NULL` acceptance;
- duplicate-key enforcement after creating a temporary unique index, and
  duplicate acceptance after dropping it;
- shadowing where a temporary table receives index DDL instead of the hidden
  persistent table;
- close/reopen persistence showing temporary indexes disappear and persistent
  rows/descriptors remain;
- independent handles with independent temporary index state;
- fulltext and deferred spatial temporary-index diagnostics;
- unknown schema/table/index/column diagnostics;
- `.mylite` preamble preservation;
- zero-initialized cleanup for the new temporary catalog mutation helpers.

The MySQL expectation script records the MySQL 8.4.9 runtime behavior for all
new user-visible result, metadata, error, warning, and side-effect semantics.
