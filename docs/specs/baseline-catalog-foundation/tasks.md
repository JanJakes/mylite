# Baseline Catalog Foundation Tasks

## Goal

Add the first durable MyLite-owned catalog foundation for file-backed
`.mylite` handles without adding public SQL execution or MySQL-visible metadata
surfaces.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-catalog-foundation/specs.md`.
   - Explain the catalog ownership boundary between `mylite_db`, catalog
     modules, and SQLite physical storage.
   - Explain why this internal slice does not require MySQL runtime probes.
   - Keep `COMPATIBILITY.md` unchanged.

2. Catalog schema
   - Add version-1 `_mylite_catalog_*` table definitions.
   - Store catalog schema version, minimum reader version, durable generation,
     and file-format version in a singleton state table.
   - Add schemas, tables, and columns descriptor tables.
   - Reserve `_mylite_*` names for MyLite-owned objects.

3. Runtime/catalog state
   - Add connection-owned catalog state to `mylite_db`.
   - Track initialization, schema version, catalog generation,
     cached generation, and descriptor-cache validity.
   - Add invalidation hooks that future analyzer/runtime layers can use.
   - Keep SQLite types out of public headers.

4. File-backed open integration
   - Initialize or validate the catalog for every file-backed handle after
     SQLite bootstrap and file-backed SQLite payload policy.
   - Leave `mylite_open_memory()` behavior unchanged in this slice.
   - Make catalog initialization idempotent across repeated opens.
   - Reject incomplete, corrupt, or incompatible catalog metadata.
   - Preserve the MyLite preamble and shifted-offset VFS behavior.

5. Private descriptor APIs
   - Add private schema, table, and column descriptor structs.
   - Add private create/read/update/delete helpers only for test and future
     lifecycle needs.
   - Make descriptor mutations transactional and generation-incrementing.
   - Explicitly delete dependent rows where needed instead of relying on
     SQLite foreign-key cascades.

6. Cleanup and failure paths
   - Make catalog init/deinit tolerate zero-initialized state.
   - Deinitialize catalog state during handle destruction.
   - Roll back failed catalog initialization and descriptor mutations.
   - Preserve existing unpublished-file cleanup on file-backed open failure.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/`.
   - Register it with a dotted CTest name.
   - Cover catalog creation in a shifted payload without preamble changes.
   - Cover reopen persistence for descriptors and generation state.
   - Cover idempotent repeated opens.
   - Cover independent file-backed handles.
   - Cover incompatible/corrupt catalog metadata rejection.
   - Cover zero-initialized catalog cleanup.

8. Build integration
   - Add new catalog source files to `mylite`.
   - Add the new test executable and include paths to
     `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry.
   - Run `cmake --workflow --preset check`.
   - Review the diff for architecture boundaries, catalog authority, public ABI
     exposure, file-format safety, VFS preservation, zero-init safety, cleanup
     on failure, scope control, and test relevance.

## Out Of Scope

- SQL execution through MyLite public APIs.
- Parser, analyzer, DDL, DML, `CREATE TABLE`, `DROP TABLE`, `SHOW`,
  `DESCRIBE`, or `INFORMATION_SCHEMA`.
- MySQL runtime comparison fixtures.
- MySQL functions, collations, type conversion, constraints, indexes, or
  auto-increment semantics.
- SQLite fork patches.
- Reconstructing MyLite descriptors from arbitrary existing SQLite schema.
- Compatibility-matrix status changes.
