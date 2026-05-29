# Baseline Built-In Schema Catalog

## Summary

MyLite exposes a narrow MySQL-compatible schema catalog surface for the four
built-in schemas that applications commonly discover before issuing metadata
queries:

- `information_schema`
- `mysql`
- `performance_schema`
- `sys`

These schemas are synthetic runtime rows. They are not persisted as MyLite
catalog descriptors, do not receive schema ids, and do not add SQLite objects.
User schemas remain descriptor-owned and durable.

This slice covers:

- `SHOW DATABASES` and `SHOW SCHEMAS`, including existing `LIKE` filters.
- `INFORMATION_SCHEMA.SCHEMATA` rows for the built-in schemas and user schemas.
- `USE` for the built-in schema names and matching `DATABASE()` /
  `@@character_set_database` / `@@collation_database` session reads.
- deterministic protection against creating, dropping, altering, or writing
  through these built-in schema names in the supported DDL/DML subset.

This slice does not implement the table catalogs inside `mysql`,
`performance_schema`, or `sys`. The later
`baseline-built-in-schema-table-directory` slice adds metadata-only table
directory rows for built-in schemas, but unsupported system tables remain
non-queryable. This slice also does not expand privilege filtering, full account
semantics, complete charset/collation catalogs, table-status statistics,
constraint metadata, or complex `INFORMATION_SCHEMA` joins. A later
`baseline-show-databases-where` slice covers limited `SHOW DATABASES WHERE`
filtering over the displayed `Database` column.

## Compatibility Authority

The supported surface is based on:

- MySQL 8.4 Reference Manual, `SHOW DATABASES` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-databases.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.SCHEMATA` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-schemata-table.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_builtin_schema_catalog_expectations.sh`.

Observed MySQL 8.4.9 metadata for this slice:

| Schema | Default charset | Default collation | SQL path | Default encryption |
| --- | --- | --- | --- | --- |
| `information_schema` | `utf8mb3` | `utf8mb3_general_ci` | `NULL` | `NO` |
| `mysql` | `utf8mb4` | `utf8mb4_0900_ai_ci` | `NULL` | `NO` |
| `performance_schema` | `utf8mb4` | `utf8mb4_0900_ai_ci` | `NULL` | `NO` |
| `sys` | `utf8mb4` | `utf8mb4_0900_ai_ci` | `NULL` | `NO` |

MyLite user schemas keep descriptor-owned default charset/collation values.

## Syntax

No parser grammar expansion is required. Existing MyLite grammar already admits
the statement shapes used by this slice:

```lemon
cmd ::= SHOW DATABASES like_opt.
cmd ::= SHOW SCHEMAS like_opt.
cmd ::= USE ident.
table_factor ::= qualified_name.
```

`SHOW DATABASES WHERE ...` remains outside this historical slice even though
MySQL accepts a `WHERE` form. The later `baseline-show-databases-where` slice
covers limited displayed-column filtering.

## Semantics

### Built-In Schema Rows

MyLite owns a fixed in-process table of built-in schema descriptors containing
the canonical name, default charset, and default collation. `information_schema`
keeps its existing case-insensitive lookup behavior. The other built-in schemas
are matched by their canonical lowercase names, matching the observed
case-sensitive MySQL 8.4.9 runtime on this platform. Returned rows and session
state use canonical lowercase names.

`SHOW DATABASES` / `SHOW SCHEMAS` return a one-column result containing the
merged set of built-in schemas and descriptor-owned user schemas. The result is
sorted using MySQL-like case-insensitive ASCII name comparison. Existing `LIKE`
filter behavior applies to both built-in and user schemas. The column label
continues to be `Database` or `Database (<pattern>)`.

`INFORMATION_SCHEMA.SCHEMATA` returns one row for each built-in schema followed
by descriptor rows for user schemas in the query engine's normal filtering and
ordering pipeline. The synthetic built-in rows have:

- `CATALOG_NAME = 'def'`
- `SCHEMA_NAME` equal to the canonical built-in schema name
- the fixed default charset/collation listed above
- `SQL_PATH = NULL`
- `DEFAULT_ENCRYPTION = 'NO'`

User schema rows continue to use durable descriptor metadata.

### USE and Session Variables

`USE information_schema` succeeds case-insensitively. `USE mysql`,
`USE performance_schema`, and `USE sys` succeed for the canonical lowercase
names. These statements do not create catalog rows. The selected schema is
stored canonically. `DATABASE()` / `SCHEMA()` return the canonical selected
name.

`@@character_set_database` and `@@collation_database` return the built-in
schema defaults when a built-in schema is selected. For no selected schema, the
existing global/default fallback remains unchanged. For user schemas, descriptor
metadata remains authoritative.

Unqualified `INFORMATION_SCHEMA` metadata table reads remain supported only when
the selected schema is `information_schema`. Selecting `mysql`,
`performance_schema`, or `sys` does not make MyLite synthesize their tables in
this slice.

### Protected Names and Writes

The built-in schema names are protected runtime names. MyLite must not create
catalog schema descriptors named `information_schema`, `mysql`,
`performance_schema`, or `sys`, and must not drop or alter them.

For schema-level DDL:

- `CREATE DATABASE information_schema` keeps the existing MySQL-compatible
  `1044 / 42000` access-denied diagnostic.
- `CREATE DATABASE mysql` returns the observed `3552 / HY000`,
  `Access to system schema 'mysql' is rejected.`
- `CREATE DATABASE performance_schema` returns the observed `1044 / 42000`
  access-denied diagnostic.
- `CREATE DATABASE sys` returns a deterministic protected-system diagnostic in
  this MyLite slice rather than creating a mutable `sys` schema.
- `CREATE DATABASE IF NOT EXISTS <built-in>` rejects for protected built-ins in
  this slice instead of creating or mutating catalog descriptors.
- `DROP DATABASE <built-in>` and `ALTER DATABASE <built-in> ...` reject before
  any catalog mutation. `information_schema` uses the existing access-denied
  diagnostic; the other built-ins use `3552 / HY000`.
- `DROP DATABASE IF EXISTS <built-in>` is still rejected, not treated as a
  missing-schema no-op.

For table-level writes and DML, any explicit built-in schema target or
unqualified target under a selected built-in schema is rejected before physical
SQLite SQL is generated. `information_schema` and `performance_schema` use
access-denied diagnostics. `mysql` and `sys` use deterministic protected-system
schema diagnostics unless a narrower MySQL-runtime-specific diagnostic is
already implemented for that statement class.

This read-only policy is intentionally stricter than some root-user MySQL paths
inside `mysql` and `sys`, because MyLite does not implement mutable server
system tables. The diagnostics are MySQL-shaped and deterministic, and this
policy prevents synthetic schema names from shadowing durable user catalog
state.

## Architecture

- Public API: unchanged. The feature is visible only through existing
  `mylite_execute()` statements and result APIs.
- Parser/AST: unchanged; existing nodes already represent `SHOW DATABASES`,
  `SHOW SCHEMAS`, `USE`, and qualified table/schema names.
- Statement context: unchanged.
- Runtime/analyzer: owns synthetic built-in schema lookup, merged listing,
  session selection, protected-name checks, and diagnostics.
- Catalog module: unchanged. Built-ins are not persisted and do not participate
  in `mylite_catalog_for_each_schema()`.
- Result builder: continues to build ordinary text results and metadata rows.
- Storage/VFS/SQLite: unchanged. No SQLite schema object is created for a
  built-in schema, and no SQLite fork hook is required.

## Performance

The built-in schema list has four rows. `SHOW DATABASES` merges those rows with
catalog descriptors into a small dynamic array for sorting and filtering.
`INFORMATION_SCHEMA.SCHEMATA` appends four synthetic rows before existing
catalog iteration. No user table rows are materialized and no physical SQLite
data is scanned.

## Diagnostics

Supported diagnostics:

- allocation failure: existing `MYLITE_NOMEM` / diagnostics policy.
- unknown user schema: existing MySQL-shaped unknown database diagnostics.
- reserved `_mylite_*` names: existing reserved-name diagnostics.
- protected `information_schema` writes: existing `1044 / 42000` access denied.
- protected `mysql`, `performance_schema`, and `sys` writes:
  `3552 / HY000`, `Access to system schema '<schema>' is rejected.`
- `SHOW DATABASES WHERE`: supported only by the later
  `baseline-show-databases-where` slice.
- unknown metadata tables under `information_schema`: existing
  `1109 / 42S02` behavior.
- selected `mysql` / `performance_schema` / `sys` unqualified table reads:
  existing unknown-table behavior for the selected schema until their table
  catalogs are implemented.

## Tests

MySQL 8.4.9 expectations:

- `SHOW DATABASES LIKE ...` and `SHOW SCHEMAS LIKE ...` include user schemas.
- `SHOW DATABASES LIKE 'mysql'`, `'performance_schema'`, and `'sys'` return the
  built-in rows.
- `INFORMATION_SCHEMA.SCHEMATA` rows match the charset/collation defaults above.
- `USE` succeeds for each built-in schema and session variables match observed
  defaults.
- protected schema DDL emits the expected diagnostics/no-op note shapes.

MyLite C coverage:

- initial and post-create schema listings include built-ins plus user schemas.
- `SHOW DATABASES` / `SHOW SCHEMAS` `LIKE` filters match built-in and user
  schemas.
- `INFORMATION_SCHEMA.SCHEMATA` returns built-in and user rows.
- `USE` succeeds for each built-in schema, normalizes case, and exposes the
  expected database charset/collation.
- catalog descriptors are not created for built-ins.
- schema-level and table-level writes against built-ins are rejected before
  catalog/SQLite mutation.
- existing `information_schema` read/write behavior remains intact.
