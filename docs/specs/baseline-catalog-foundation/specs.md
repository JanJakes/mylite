# Baseline Catalog Foundation

## Status

This feature specifies the first durable MyLite-owned catalog foundation for
file-backed `.mylite` databases. It is internal runtime and storage
infrastructure layered on top of `mylite_db`, the SQLite bootstrap policy, and
the shifted-offset file-backed opening VFS.

The feature does not add user-visible SQL execution, DDL, DML, `SHOW`,
`DESCRIBE`, or `INFORMATION_SCHEMA` behavior. It does not move any row in
`COMPATIBILITY.md` out of unsupported status.

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
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Bundled SQLite public header: `third_party/sqlite/amalgamation/sqlite3.h`

This specification is independently authored from MyLite project
documentation, public SQLite APIs, and observed behavior of the current MyLite
runtime/storage code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## Scope

The implementation must add:

- catalog schema version 1 stored as ordinary SQLite tables inside the shifted
  SQLite payload of `.mylite` files;
- idempotent catalog initialization for every successful file-backed
  `mylite_open()` after preamble validation, shifted SQLite open, SQLite
  bootstrap, and file-backed SQLite payload policy;
- durable generation state for catalog descriptor changes;
- connection-owned catalog state on `mylite_db` with schema version, generation,
  and descriptor-cache invalidation flags;
- internal descriptor structs for schemas, tables, and columns;
- private APIs sufficient for tests to create, read, update, and delete catalog
  descriptor rows;
- rejection of corrupt, partially initialized, or incompatible catalog metadata;
- tests proving catalog persistence, idempotence, file-format safety,
  independent handles, incompatible metadata rejection, and zero-initialized
  cleanup.

## Non-Goals

This feature must not implement:

- SQL execution through MyLite's public API;
- parser, analyzer, DDL, DML, `CREATE TABLE`, `DROP TABLE`, `ALTER TABLE`, or
  `INFORMATION_SCHEMA` query surfaces;
- user-visible `SHOW`, `DESCRIBE`, or information-schema virtual tables;
- MySQL runtime comparison fixtures, because no MySQL-visible SQL behavior is
  added by this slice;
- MySQL function, collation, type-conversion, auto-increment, default,
  constraint, or index semantics;
- SQLite fork patches;
- reconstructing MyLite descriptors from arbitrary existing SQLite schema
  objects;
- compatibility-matrix status changes.

## Compatibility Notes

No MySQL runtime probe is attached to this feature. The behavior being tested is
MyLite's internal durable metadata contract and file-backed initialization
order, not a MySQL-visible SQL surface. The first user-visible feature that
depends on this catalog must add its own MySQL 8.4.9 runtime-verified
expectations before changing compatibility status.

`COMPATIBILITY.md` remains unchanged.

## Ownership Boundary

`mylite_db` owns connection-local catalog state:

- whether catalog initialization completed for the handle;
- the catalog schema version accepted by the runtime;
- the durable catalog generation read from the file;
- the generation currently represented by any connection-local descriptor
  cache;
- whether the descriptor cache is valid.

The catalog module owns:

- internal catalog table names and schema;
- catalog initialization and validation SQL;
- private descriptor structs for schema, table, and column rows;
- private descriptor mutation helpers used by tests and future table lifecycle
  work;
- generation updates and cache invalidation hooks.

SQLite owns only durable b-tree storage and transaction durability. SQLite
schema text, `sqlite_master`, and `PRAGMA` output are physical implementation
details. MyLite logical descriptors stored in `_mylite_catalog_*` tables are
the source of truth for future analyzer, DDL, result metadata, `SHOW`, and
information-schema layers. Future analyzer/runtime code must resolve logical
objects from MyLite descriptors, then generate or validate physical SQLite
objects from those descriptors.

## Internal Catalog Tables

All version-1 catalog tables use reserved names beginning with
`_mylite_catalog_`. They live in the main SQLite database payload. Temporary
objects are not represented by these tables in this slice.

### `_mylite_catalog_state`

Singleton row with `singleton_id = 1`.

| Column | Type | Constraints | Meaning |
| --- | --- | --- | --- |
| `singleton_id` | `INTEGER` | `PRIMARY KEY`, `CHECK(singleton_id = 1)` | singleton row identity |
| `schema_version` | `INTEGER` | `NOT NULL` | catalog schema version, currently `1` |
| `minimum_reader_schema_version` | `INTEGER` | `NOT NULL` | oldest runtime schema reader accepted, currently `1` |
| `catalog_generation` | `INTEGER` | `NOT NULL` | monotonic descriptor generation |
| `created_with_file_format_version` | `INTEGER` | `NOT NULL` | MyLite file-format version used at initialization |

New catalog initialization inserts generation `1`. Every successful descriptor
mutation increments `catalog_generation` before commit. Reopening a file reads
the stored generation without incrementing it.

### `_mylite_catalog_schemas`

| Column | Type | Constraints | Meaning |
| --- | --- | --- | --- |
| `schema_id` | `INTEGER` | `PRIMARY KEY` | stable schema descriptor id |
| `name` | `TEXT` | `NOT NULL`, `UNIQUE` | MySQL logical schema name |
| `descriptor_version` | `INTEGER` | `NOT NULL` | row-local descriptor version |
| `created_catalog_generation` | `INTEGER` | `NOT NULL` | generation that created the row |
| `updated_catalog_generation` | `INTEGER` | `NOT NULL` | generation that last updated the row |

### `_mylite_catalog_tables`

| Column | Type | Constraints | Meaning |
| --- | --- | --- | --- |
| `table_id` | `INTEGER` | `PRIMARY KEY` | stable table descriptor id |
| `schema_id` | `INTEGER` | `NOT NULL` | owning schema descriptor id |
| `name` | `TEXT` | `NOT NULL` | MySQL logical table name |
| `kind` | `INTEGER` | `NOT NULL` | object kind, initially base table only |
| `physical_name` | `TEXT` | `NOT NULL`, `UNIQUE` | generated SQLite object name |
| `descriptor_version` | `INTEGER` | `NOT NULL` | row-local descriptor version |
| `created_catalog_generation` | `INTEGER` | `NOT NULL` | generation that created the row |
| `updated_catalog_generation` | `INTEGER` | `NOT NULL` | generation that last updated the row |

`(schema_id, name)` is unique. Version 1 defines table kind `1` as a persistent
base table descriptor. Other object kinds are rejected by the private API until
their feature specs define them.

### `_mylite_catalog_columns`

| Column | Type | Constraints | Meaning |
| --- | --- | --- | --- |
| `column_id` | `INTEGER` | `PRIMARY KEY` | stable column descriptor id |
| `table_id` | `INTEGER` | `NOT NULL` | owning table descriptor id |
| `ordinal_position` | `INTEGER` | `NOT NULL` | one-based logical column position |
| `name` | `TEXT` | `NOT NULL` | MySQL logical column name |
| `logical_type` | `TEXT` | `NOT NULL` | independently authored MyLite logical type token |
| `physical_type` | `TEXT` | `NOT NULL` | SQLite storage declaration chosen by MyLite |
| `is_nullable` | `INTEGER` | `NOT NULL` | `0` for not-null, `1` for nullable |
| `descriptor_version` | `INTEGER` | `NOT NULL` | row-local descriptor version |
| `created_catalog_generation` | `INTEGER` | `NOT NULL` | generation that created the row |
| `updated_catalog_generation` | `INTEGER` | `NOT NULL` | generation that last updated the row |

`(table_id, ordinal_position)` and `(table_id, name)` are unique. This slice
does not define defaults, generated expressions, charset ids, collation ids,
comments, visibility, keys, constraints, or auto-increment metadata; later
feature specs must add those columns or side tables before user-visible table
DDL depends on them.

## Reserved Names

Names beginning with `_mylite_` are reserved for MyLite-owned physical and
catalog objects. Future public SQL DDL must reject or quote-route user-authored
objects that would collide with this namespace before any SQLite SQL is
generated. This slice cannot expose that rejection because there is no public
DDL surface yet.

Existing files that already contain user-created SQLite objects with
`_mylite_` names but no MyLite catalog are accepted only when those names do
not match the version-1 catalog table names. Exact catalog table-name
collisions are treated as catalog metadata and must validate as a complete
version-1 catalog.

## Create And Open Behavior

Missing `.mylite` file:

1. Create the version-1 MyLite preamble.
2. Open the shifted SQLite payload through the MyLite VFS.
3. Apply SQLite bootstrap policy.
4. Apply file-backed SQLite payload policy.
5. Initialize catalog schema version 1 in one SQLite transaction.
6. Publish the handle only after all steps succeed.

Existing valid `.mylite` file with initialized catalog tables:

1. Validate the MyLite preamble before SQLite sees the payload.
2. Open the shifted SQLite payload through the MyLite VFS.
3. Apply SQLite bootstrap policy.
4. Apply file-backed SQLite payload policy.
5. Validate the complete catalog table set and singleton state row.
6. Load catalog version and generation into connection-owned state.

Existing valid `.mylite` file with no catalog tables:

- Initialize the complete version-1 catalog in one transaction. Existing
  non-catalog SQLite objects are left untouched and are not reverse-engineered
  into MyLite descriptors.

Existing file with some, but not all, catalog tables:

- Reject as corrupt/incomplete catalog metadata. Do not attempt repair in the
  runtime open path.

Existing file with incompatible catalog metadata:

- Reject when the state row is missing, duplicated by impossible table shape,
  has `schema_version != 1`, has `minimum_reader_schema_version > 1`, or has
  nonsensical generation values. Future migrations must be specified before
  accepting any other version.

## Initialization Ordering

Catalog initialization for file-backed handles occurs after:

1. MyLite preamble validation or creation;
2. shifted SQLite VFS registration and SQLite open;
3. SQLite bootstrap policy, including trusted-schema and foreign-key placeholder
   policy;
4. file-backed SQLite payload policy, currently rollback journal mode and
   mmap-size policy.

It occurs before the handle is returned to the caller. It does not create a
statement context because opening a file is not a user SQL statement. Future
statement execution must use statement contexts for descriptor mutations that
come from user SQL.

Memory handles opened with `mylite_open_memory()` keep the existing runtime
behavior in this slice. A transient in-memory catalog can be specified later
when user-visible SQL execution needs memory handles to support DDL.

## Version And Migration Behavior

Catalog schema version 1 is the only supported version. A file without any
version-1 catalog tables is initialized to version 1. A file with version-1
tables is accepted only after validating the state row and complete table set.

No upgrade or downgrade migration exists in this slice. Files with any
different catalog schema version are rejected. Future migrations must be
transactional, must preserve the `.mylite` preamble, and must document which
older versions can be read or upgraded before the runtime accepts them.

## Descriptor Caching And Invalidation

The first implementation keeps the descriptor cache minimal: no heap-owned
table or column descriptor cache is populated during open. The connection still
tracks enough state for future cache integration:

- `catalog_generation`: durable generation read from
  `_mylite_catalog_state`;
- `cached_generation`: generation represented by any cached descriptors;
- `descriptor_cache_is_valid`: whether cached descriptors can be reused.

Catalog initialization sets `cached_generation` to `0` and invalidates the
cache. Private descriptor mutations increment the durable catalog generation,
update `mylite_db` session generation, and invalidate the cache. Future
prepared statements must record the catalog generation used during analysis and
must be rejected or re-analyzed if the connection catalog generation changes.

SQLite schema generation remains a separate runtime counter. It is not advanced
by catalog-only mutations unless a physical SQLite schema object also changes.

## Private Descriptor APIs

The implementation may add private APIs for tests and future table lifecycle
work:

- initialize and deinitialize a connection-owned catalog state;
- create/read/delete schema descriptors;
- create/read/update/delete table descriptors;
- create/read/delete column descriptors;
- refresh and expose generation state for internal tests.

These APIs are not public ABI. They must not expose SQLite types through public
headers. They may take `struct mylite_db *` internally because the catalog state
and SQLite connection are both connection-owned.

## Transaction Behavior

Catalog initialization runs in one SQLite transaction. If any statement fails,
the transaction is rolled back and the open fails.

Private descriptor mutations run in one SQLite transaction each for this slice.
The mutation updates descriptor rows and the catalog generation atomically but
does not publish an integrity seal because these test-only helpers may assemble
catalog and physical state in separate transactions. SQL DDL uses lower-level
mutation helpers inside its statement transaction or savepoint so catalog and
physical schema changes commit or roll back together and are validated before
seal publication.

Because SQLite foreign-key enforcement is currently disabled by bootstrap
policy, catalog deletion helpers must explicitly delete dependent rows needed by
their own operation. They must not rely on SQLite foreign-key cascades.

## Cleanup And Failure Unwinding

Connection-owned catalog state is zero-initialized with `mylite_db` and must be
safe to deinitialize even when initialization never ran. Open failure after
catalog initialization must deinitialize catalog state before SQLite closes.

If catalog initialization fails while opening a newly created `.mylite` file,
the existing file-open cleanup path removes the unpublished file where
possible. If initialization fails on an existing file, the file is left intact
except for changes already rolled back by SQLite.

## Parser And Grammar

No MyLite SQL syntax is added. No Lemon grammar snippets apply to this feature.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.catalog_foundation`.

Required coverage:

- new file-backed open creates the catalog schema inside the shifted SQLite
  payload without changing the MyLite preamble;
- reopening an existing `.mylite` file preserves catalog descriptor rows and
  generation state;
- repeated opens are idempotent and do not duplicate catalog rows or increment
  generation;
- independent file-backed handles keep independent catalog rows and generation
  state;
- incompatible or corrupt catalog metadata is rejected according to this spec;
- zero-initialized catalog state cleanup is safe;
- existing file-backed opening, VFS offset proof, bootstrap policy,
  diagnostics, statement context, and result metadata tests still pass.

## Build Integration

Add new first-party runtime/storage/catalog sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.catalog_foundation$' --output-on-failure
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, catalog authority,
public ABI exposure, file-format safety, VFS preservation, zero-init safety,
cleanup on failure, scope control, and test relevance.
