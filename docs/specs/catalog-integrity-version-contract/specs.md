# Catalog Integrity And Version Contract

## Status

This specification hardens the internal catalog used by file-backed `.mylite`
databases. It does not add SQL syntax or change a MySQL-visible compatibility
claim.

## Scope

Every successful file-backed open must establish that the catalog can be
trusted before descriptors are exposed to planning or execution. Validation
must cover:

- the complete column shape of every catalog table, including declared types,
  nullability, and primary keys, while allowing semantically equivalent column
  order produced by historical migrations;
- required unique constraints, lookup indexes, and value-domain checks;
- parent/child relationships among schemas, tables, views, columns, indexes,
  foreign keys, and check constraints;
- one-based contiguous ordinals for columns and constraint parts;
- agreement between catalog base-table/index descriptors and SQLite physical
  objects; and
- agreement between catalog base-table columns and physical SQLite columns.

The verifier uses public SQLite metadata interfaces and independently authored
catalog invariants. It does not rely on SQLite foreign-key enforcement because
MyLite deliberately owns logical foreign-key behavior.

## Failure Behavior

Missing catalog objects, altered definitions, missing required indexes,
orphaned children, mismatched object ownership, ordinal gaps, missing physical
tables or indexes, and missing physical columns make `mylite_open()` fail with
`MYLITE_ERROR`. The handle is not published. Validation does not attempt to
repair corruption implicitly.

Catalog validation runs after a supported older schema is migrated and before
the migrated descriptor state is installed on the connection. Migration and
normal DDL remain transactional.

## Reader And Writer Compatibility

The public open API creates a fully write-capable handle. MyLite has no
read-only catalog mode and does not negotiate feature capabilities with a
newer file. Therefore:

- a runtime may migrate an older catalog version through explicitly supported
  one-way migrations;
- a runtime rejects a catalog newer than its own writer version;
- a file's `minimum_reader_schema_version` must not exceed its schema version;
- a runtime must be at least the file's minimum reader version; and
- newly written catalog version 37 declares version 37 as its minimum reader.

The minimum-reader field describes the oldest complete runtime allowed to open
the file, not permission to bypass writer compatibility. Catalog version 37 is
a metadata-only migration that corrects the previous version-36 value of 35.
An actual version-36 runtime must reject a file after version 37 has migrated
it. Supporting N-1 readers in the future requires an explicit read-only API or
a separate minimum-writer capability contract.

## Performance

Integrity checks run only while opening a file-backed handle. Checks should use
indexed existence probes where possible and must not allocate descriptor row
sets. Normal statement execution must not repeat the full verifier.

## Verification

Coverage must tamper otherwise valid files to remove or alter primary keys,
unique constraints, checks, and the parent-foreign-key lookup index. It must
also cover orphaned rows, cross-table ownership mismatches, ordinal gaps,
missing physical tables/indexes/columns, migration from version 36, and
rejection by an actual version-36 binary after a version-37 open.
