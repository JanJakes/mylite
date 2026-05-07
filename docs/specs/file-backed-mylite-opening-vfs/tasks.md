# File-Backed MyLite Opening VFS Tasks

## Goal

Add the first file-backed `.mylite` open path on top of `mylite_db`: validate or
initialize the 4096-byte MyLite preamble, open the shifted SQLite payload
through a MyLite-owned VFS shim, and reuse the existing SQLite bootstrap policy.

## Tasks

1. Design and documentation
   - Create `docs/specs/file-backed-mylite-opening-vfs/specs.md`.
   - Explain why this internal slice does not require MySQL runtime probes.
   - Keep `COMPATIBILITY.md` unchanged.

2. Public API
   - Add `mylite_open(const char *path, mylite_db **out_db)` to the public
     umbrella header.
   - Keep SQLite types out of public headers.
   - Validate required arguments and leave `*out_db == NULL` on failure.

3. File preamble open state
   - Add storage-owned file preparation helpers.
   - Create missing files with a version-1 preamble.
   - Validate existing preambles before SQLite opens the payload.
   - Reject truncated preambles, invalid preambles, and plain SQLite files.
   - Make open-state cleanup tolerate zero-initialized state and remove newly
     created files on unpublished failure.

4. Shifted-offset VFS
   - Add a MyLite VFS shim wrapping the default SQLite VFS.
   - Shift `xRead`, `xWrite`, `xTruncate`, `xFileSize`, and size-related file
     controls for `SQLITE_OPEN_MAIN_DB` files only.
   - Delegate non-main files, journals, temporary files, path methods, locking,
     sync, sector, and device-characteristic behavior to the wrapped VFS.
   - Keep the shim on public SQLite VFS APIs only; do not patch SQLite.
   - Keep zero-initialized and failed `xOpen` cleanup safe.

5. Runtime integration
   - Factor shared handle allocation/open cleanup where useful.
   - Open file-backed SQLite connections with the MyLite VFS.
   - Reuse `mylite_sqlite_bootstrap_connection()` for every file-backed handle.
   - Apply the phase journal and mmap policy after SQLite open.
   - Preserve memory-open behavior.

6. Runtime/storage tests
   - Add a fast C test under `packages/libmylite/tests/`.
   - Register it with a dotted CTest name.
   - Cover new-file creation, preamble bytes, shifted SQLite header, and
     internal SQLite writes.
   - Cover reopen durability.
   - Cover invalid, truncated, and plain SQLite rejection.
   - Cover independent file-backed handles and bootstrap state.
   - Cover zero-initialized cleanup helpers and created-file failure cleanup.

7. Build integration
   - Add new storage/runtime sources to `mylite`.
   - Add the new test executable and include paths to
     `packages/libmylite/CMakeLists.txt`.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry.
   - Run `cmake --workflow --preset check`.
   - Review the diff for architecture boundaries, public ABI exposure,
     file-format safety, VFS correctness, zero-init safety, cleanup on failure,
     scope control, and test relevance.

## Out Of Scope

- SQL execution through MyLite public APIs.
- Catalog tables, descriptors, and `INFORMATION_SCHEMA`.
- MySQL runtime comparison fixtures.
- MySQL functions, collations, types, DDL, DML, or parser behavior.
- SQLite fork patches.
- WAL, backup, VACUUM, `VACUUM INTO`, mmap, and advanced journal completeness.
- Compatibility-matrix status changes.
