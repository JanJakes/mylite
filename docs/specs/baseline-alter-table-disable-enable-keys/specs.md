# Baseline ALTER TABLE DISABLE/ENABLE KEYS

## Status

This feature specifies a narrow compatibility slice for MyISAM-era bulk-load
syntax over MyLite's current descriptor-backed base tables:

```sql
ALTER TABLE table_name DISABLE KEYS
ALTER TABLE table_name ENABLE KEYS
```

MyLite's current user tables model the InnoDB baseline rather than MyISAM key
maintenance. The statements therefore resolve and validate the target table,
then complete as no-op metadata/data operations with MySQL-compatible storage
engine notes. They do not disable physical SQLite indexes, change optimizer
behavior, or mutate MyLite catalog descriptors.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing table lifecycle, index lifecycle, temporary-table, diagnostics, and
  ALTER TABLE specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_alter_table_disable_enable_keys_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `ALTER TABLE t DISABLE KEYS` succeeds for an InnoDB table, reports
  `ROW_COUNT() == 0`, leaves `@@error_count == 0`, and records one note.
- `ALTER TABLE t ENABLE KEYS` has the same result shape for an InnoDB table.
- The note has code `1031`, SQLSTATE `HY000`, level `Note`, and text
  `Table storage engine for 't' doesn't have this option`.
- Persistent `ALGORITHM=COPY` forms emit the same note against MySQL's
  transient internal `#sql-...` table name.
- The statements do not change rows, columns, index metadata, or
  `SHOW CREATE TABLE` output for InnoDB tables.
- Temporary InnoDB tables are accepted. MySQL reports the same `1031` note
  using an internal `#sql-...` temporary name; MyLite reports the logical
  target name for deterministic embedded diagnostics.
- Schema-qualified targets work without a selected default database.
- Unqualified targets without a selected default database fail with error
  `1046`, SQLSTATE `3D000`.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown target tables fail with error `1146`, SQLSTATE `42S02`.
- Targets in `information_schema` fail with error `1044`, SQLSTATE `42000`.
- Missing default schema, unknown schema, unknown table, and denied
  `information_schema` targets are reported before operation-specific
  incompatible `LOCK=NONE` / `LOCK=SHARED` diagnostics. For
  `ALGORITHM=INSTANT` with an explicit non-default lock, MySQL reports missing
  default schema and `information_schema` access before the option-usage error,
  but reports the option-usage error before full unknown-schema/table lookup.
- Optional `ALGORITHM` values `DEFAULT`, `INSTANT`, `INPLACE`, and `COPY` are
  accepted when the lock option is omitted or compatible. Observed InnoDB
  `ALGORITHM=COPY` forms report the current table row count as `ROW_COUNT()`;
  non-`COPY` forms report `0`.
- Optional `LOCK=DEFAULT` and `LOCK=EXCLUSIVE` are accepted. `LOCK=SHARED` and
  `LOCK=NONE` are rejected with MySQL's combined
  `LOCK=NONE/SHARED is not supported for this operation` diagnostic unless
  `ALGORITHM=COPY` uses the existing `COPY`/`LOCK=NONE` diagnostic;
  `ALGORITHM=INSTANT` with any explicit non-default lock is rejected as
  incorrect usage.
- MySQL accepts broader multi-action lists such as `DISABLE KEYS, ENABLE KEYS`
  and combinations with table-copying actions. MyLite intentionally defers
  multi-action integration for this slice.

## Scope

The implementation must add:

- parser and AST support for single-action `ALTER TABLE ... DISABLE KEYS` and
  `ALTER TABLE ... ENABLE KEYS` statements;
- optional comma-prefixed `ALGORITHM` / `LOCK` option tails using the existing
  limited ALTER option grammar;
- unqualified and schema-qualified target table resolution through the existing
  selected/default schema policy;
- reserved `_mylite_*` target-name rejection before any physical SQL could be
  generated;
- writable `information_schema` target rejection through the existing
  access-denied policy;
- persistent base-table and shadowing session temporary-table descriptor
  validation;
- no-op runtime execution that preserves rows, indexes, descriptors,
  descriptor versions, catalog generation, `sqlite_schema_generation`, and
  physical SQLite schema;
- successful result reporting with zero result columns, zero result rows,
  `affected_rows == 0` for non-`COPY` forms, `affected_rows` equal to the
  current target row count for `ALGORITHM=COPY`, and `warning_count == 1`;
- one MySQL-compatible note for supported current MyLite targets, using the
  logical target name for non-`COPY` forms and a deterministic internal
  `#sql-mylite-copy` name for `ALGORITHM=COPY` forms;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- MyISAM storage, delayed nonunique-index maintenance, disabled-index metadata,
  optimizer index ignoring, or bulk-load acceleration;
- changing, dropping, rebuilding, or creating physical SQLite indexes;
- full multi-action `ALTER TABLE` participation;
- MyISAM-specific `SHOW INDEX.Comment = 'disabled'` behavior;
- privilege checks beyond existing embedded read/write target policy;
- partitioned tables, views, triggers, cascades, foreign-key dependency
  changes, or optimizer statistics;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. Applications continue to call
  `mylite_execute()` and release result objects through the existing result API.
- Statement context owns diagnostics reset, warning/note accumulation,
  affected rows, and `ROW_COUNT()` state.
- Lexer/parser/AST own syntax admission for the two single-action ALTER forms.
  Parser code remains independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves writable table targets against MyLite
  descriptors before emitting any result.
- The catalog remains authoritative for schemas, table identity, object kind,
  temporary shadowing, and physical names. This feature does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or SQLite
  schema generation.
- Runtime execution appends the MySQL-compatible note and returns a no-row
  result. It does not generate SQLite DDL or DML for successful statements.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.
- SQLite owns existing physical row storage and indexes. MyLite does not
  materialize rows or perform any key maintenance in C memory.

## Supported SQL Grammar

The feature extends the existing limited `ALTER TABLE` grammar with two
single-action forms:

```sql
ALTER TABLE table_name DISABLE KEYS [, alter_table_algorithm_lock_option [, ...]]
ALTER TABLE table_name ENABLE KEYS [, alter_table_algorithm_lock_option [, ...]]
```

`table_name` may be unqualified or schema-qualified.

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= alter_table_disable_keys_statement(B). {
    A = B;
}

statement(A) ::= alter_table_enable_keys_statement(B). {
    A = B;
}

alter_table_disable_keys_statement(A) ::=
    ALTER(T) TABLE table_name(N) DISABLE KEYS alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_disable_keys_statement(state, T, N, O);
}

alter_table_enable_keys_statement(A) ::=
    ALTER(T) TABLE table_name(N) ENABLE KEYS alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_enable_keys_statement(state, T, N, O);
}
```

The grammar intentionally excludes `DISABLE KEYS` / `ENABLE KEYS` inside
multi-action `ALTER TABLE` lists for this phase.

## Resolution Semantics

Unqualified target table names require the currently selected schema.
Schema-qualified target table names use the explicit schema and do not require
a selected schema. Missing default schema, unknown explicit schema, and unknown
table diagnostics follow the existing table lifecycle policy.

Target schemas and tables with reserved `_mylite_*` names are rejected before
any physical SQL generation. `information_schema` write targets are rejected
with the existing MySQL-compatible access-denied diagnostic.

If a session temporary table shadows a persistent table of the same
schema/name, the temporary descriptor is the target and the statement succeeds
as a no-op note. Otherwise the persistent base-table descriptor is the target.
Non-base persistent object kinds remain unsupported once introduced.

Descriptor catalog identifier matching follows MyLite's current catalog name
policy. SQLite schema text is not consulted.

## Runtime Semantics

For supported current MyLite base tables, both statements:

1. validate target-name preconditions that MySQL reports before some option
   errors, including missing default schema and `information_schema` access;
2. validate option values and option combinations whose errors precede full
   table lookup, such as invalid option tokens and `ALGORITHM=INSTANT` with an
   explicit non-default lock;
3. resolve the writable target descriptor;
4. validate operation-specific incompatible lock combinations;
5. append exactly one note with code `1031` and SQLSTATE `HY000`;
6. return the existing non-row statement result.

For persistent non-`COPY` tables, the note text uses the logical target table
name:

```text
Table storage engine for '<table_name>' doesn't have this option
```

Temporary table targets use the same deterministic logical-name text in MyLite.
MySQL's observed temporary-table note names an internal `#sql-...` object, so
tests record that upstream behavior without requiring MyLite to expose an
internal transient name.

`ALGORITHM=COPY` forms report the note against deterministic internal name
`#sql-mylite-copy`, reflecting MySQL's observed use of a transient `#sql-...`
copy-table name without exposing a non-deterministic suffix.

Successful non-`COPY` statements return:

- zero result columns;
- zero result rows;
- `affected_rows == 0`;
- `warning_count == 1`;
- `ROW_COUNT() == 0`;
- `@@warning_count == 1` until normal diagnostics-reset behavior applies.

Successful `ALGORITHM=COPY` statements keep the same no-op storage behavior but
report `affected_rows` / `ROW_COUNT()` as the current target row count, matching
observed MySQL 8.4.9 InnoDB behavior.

Rows, descriptor metadata, physical SQLite schema, and file format state are
unchanged. There is no mutating SQLite DDL or DML path for the successful
no-op; `ALGORITHM=COPY` may read the physical row count to match MySQL's
affected-row reporting.

## ALTER Option Semantics

The admitted option tail reuses the existing ALTER option parser. Runtime
validation for this feature is:

- invalid/unknown algorithm or lock option values are syntax errors through the
  existing ALTER option policy;
- `ALGORITHM=DEFAULT`, `ALGORITHM=INSTANT`, `ALGORITHM=INPLACE`, and
  `ALGORITHM=COPY` are accepted when no incompatible explicit lock is present;
- `LOCK=DEFAULT` and `LOCK=EXCLUSIVE` are accepted;
- `LOCK=SHARED` and `LOCK=NONE` are rejected with MySQL's combined online-DDL
  diagnostic for this operation;
- `ALGORITHM=COPY, LOCK=NONE` uses MySQL's `COPY algorithm requires a lock`
  diagnostic;
- `ALGORITHM=INSTANT` with `LOCK=NONE`, `LOCK=SHARED`, or `LOCK=EXCLUSIVE`
  uses MySQL's incorrect-usage diagnostic.

The option values do not alter execution because the operation is a no-op for
current MyLite tables.

## Diagnostics

The implementation must preserve or add deterministic diagnostics for:

- syntax errors and unsupported grammar;
- unsupported multi-action use;
- missing default schema;
- unknown explicit schema;
- unknown target table;
- reserved target schema or table names;
- `information_schema` write targets;
- unsupported object kind;
- invalid or incompatible `ALGORITHM` / `LOCK` option values;
- allocation failures;
- public API misuse if the public surface changes.

Supported statements emit the single storage-engine note and no errors.

## Storage, File Format, and SQLite Policy

This feature uses no mutating SQLite SQL for the successful path and does not
require SQLite extension APIs or fork hooks. `ALGORITHM=COPY` may perform a
read-only row-count query. The `.mylite` preamble and shifted SQLite payload
invariants are preserved by leaving the physical database unchanged.

## Tests

Add MySQL-runtime-verified expectations and C tests covering:

- successful persistent `DISABLE KEYS` and `ENABLE KEYS`;
- session temporary target shadowing;
- schema-qualified and unqualified target resolution;
- result shape, `affected_rows == 0`, `ROW_COUNT() == 0`, `warning_count == 1`,
  and note details;
- row, index metadata, `SHOW CREATE TABLE`, catalog generation, descriptor
  version, and `sqlite_schema_generation` preservation;
- reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles;
- diagnostics for missing default schema, unknown schema, unknown table,
  information_schema targets, and reserved target names;
- option-tail acceptance and incompatible option diagnostics;
- parser coverage for accepted forms and deferred multi-action grammar;
- zero-initialized cleanup for any new plan objects.

Run:

```sh
cmake --build --preset dev
packages/libmylite/tests/mysql_baseline_alter_table_disable_enable_keys_expectations.sh
ctest --preset dev --output-on-failure -R '^(libmylite\.parser|libmylite\.runtime\.alter_table_disable_enable_keys|libmylite\.runtime\.alter_table_algorithm_lock_clauses|libmylite\.runtime\.alter_table_add_index|libmylite\.runtime\.temporary_index_lifecycle)$'
cmake --workflow --preset check
```
