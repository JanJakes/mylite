# File-Backed MyLite Opening VFS

## Status

This feature specifies the first file-backed `.mylite` runtime open path. It is
internal storage and runtime infrastructure layered on top of `mylite_db` and
the shared SQLite bootstrap policy. It does not add public SQL execution, does
not expose SQLite in public headers, and does not move any row in
`COMPATIBILITY.md` out of unsupported status.

The goal is to prove that a version-1 `.mylite` file can contain a 4096-byte
MyLite preamble followed by a shifted SQLite payload accessed through a
MyLite-owned VFS shim. This proof must happen before considering any SQLite
pager or file-layer fork patch.

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
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Bundled SQLite public header: `third_party/sqlite/amalgamation/sqlite3.h`

This specification is independently authored from MyLite project
documentation, the bundled SQLite public C API documentation, and existing
MyLite source code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or other restrictively licensed implementation
sources.

## Scope

The implementation must add:

- a narrow public file-open API for `.mylite` files;
- missing-file creation with a version-1 MyLite preamble;
- existing-file validation before SQLite sees the payload;
- rejection of truncated preambles, invalid preambles, and plain SQLite files
  at byte 0;
- a MyLite-owned SQLite VFS shim that shifts only main database file I/O by
  `MYLITE_FILE_SQLITE_PAYLOAD_OFFSET`;
- internal runtime opening that reuses the shared SQLite bootstrap path;
- initial rollback-journal-mode and mmap policy for the proof;
- fast C tests that prove preamble safety, shifted payload writes, reopen
  durability, independent handles, bootstrap preservation, and cleanup paths.

## Non-Goals

This feature must not implement:

- public SQL execution through MyLite;
- catalog tables, descriptors, `INFORMATION_SCHEMA`, or MySQL metadata;
- MySQL runtime comparison fixtures;
- MySQL function, collation, type, DDL, DML, or parser behavior;
- SQLite fork patches unless this spec is updated first with the demonstrated
  VFS blocker;
- WAL, backup, VACUUM, `VACUUM INTO`, mmap, or advanced journal completeness
  beyond disabling or deferring those paths for this phase;
- compatibility-matrix status changes.

## Public API Decision

Add this public API to `mylite/mylite.h`:

```c
MYLITE_API int mylite_open(const char *path, mylite_db **out_db);
```

`path` is a required NUL-terminated byte string borrowed for the duration of
the call. `out_db` is required. On success, `*out_db` receives an owned
`mylite_db *` released with `mylite_close()`. On failure, `*out_db` is set to
`NULL` when `out_db` is non-NULL.

The API intentionally starts with a NUL-terminated path form because the
current public ABI already exposes only simple C entry points and the first
storage proof does not need embedded NUL bytes or platform-specific path
objects. A length-aware or platform-native path form can be added later without
breaking this function once integration packages prove the need.

SQLite types remain private. Callers cannot observe or configure the SQLite
connection directly.

Return behavior:

- `MYLITE_OK` on success;
- `MYLITE_MISUSE` when `path` is `NULL`, `path` is empty, or `out_db` is
  `NULL`;
- `MYLITE_NOMEM` for allocation failures reported by MyLite or SQLite;
- `MYLITE_ERROR` for invalid files, truncated preambles, plain SQLite files,
  I/O failures, SQLite open failures, or bootstrap failures.

Open failure has no returned handle, so no handle-owned diagnostics are
available yet. The return code is the complete public failure signal for this
slice. Future diagnostics work may add an explicit open-error object or
allocator-owned message API if callers need details before a handle exists.

## File Create And Open Behavior

Missing file:

- Create the file.
- Write the exact 4096-byte version-1 MyLite preamble.
- Open the shifted SQLite payload through the MyLite VFS.
- If later SQLite open or bootstrap fails, close owned resources and remove
  the newly created file where possible.

Existing valid `.mylite` file:

- Read exactly 4096 bytes from byte 0.
- Validate magic, format version, and reserved zero bytes with the existing
  file-format helper.
- Open the shifted SQLite payload without modifying the preamble.

Existing invalid preamble:

- Reject the open before calling SQLite.
- Leave the file unchanged.

Existing truncated file:

- If fewer than 4096 bytes can be read from an existing file, reject it as a
  truncated preamble.
- Leave the file unchanged.

Plain SQLite file at byte 0:

- Reject it because byte 0 contains SQLite's header instead of the MyLite
  preamble.
- Do not attempt to import, shift, or repair it in this phase.

## VFS Architecture

The storage layer registers a process-local MyLite VFS with SQLite using
`sqlite3_vfs_register()`. The VFS wraps the default SQLite VFS found through
`sqlite3_vfs_find(NULL)` and keeps the wrapped VFS in immutable VFS state after
registration. MyLite opens `.mylite` payloads with `sqlite3_open_v2()` and the
MyLite VFS name.

The shim subclasses `sqlite3_file` and owns an inner file object opened by the
wrapped VFS. It uses SQLite public VFS APIs only. It must not depend on SQLite
pager internals or copy SQLite implementation code.

The VFS shifts offsets only for files opened with `SQLITE_OPEN_MAIN_DB`.
Rollback journals, temporary files, transient files, subjournals, super-journals,
and other auxiliary files are delegated unchanged to the wrapped VFS. This keeps
the proof focused on the main database payload while relying on SQLite's normal
journal naming and lifecycle.

The VFS presents itself as VFS version 1 for this phase. That intentionally
omits shared-memory and mmap methods, so WAL and memory-mapped I/O are not part
of the initial proof.

## VFS Method Requirements

`xOpen`:

- Set the wrapper file `pMethods` to `NULL` before attempting inner open.
- Allocate zero-initialized inner file storage sized for the wrapped VFS.
- Delegate the original filename and flags to the wrapped VFS.
- Mark files opened with `SQLITE_OPEN_MAIN_DB` as shifted.
- On failure, close any partially opened inner file, free allocations, keep the
  wrapper `pMethods` `NULL`, and return the SQLite status.

`xRead`:

- For shifted main database files, add 4096 to the logical offset before
  delegating.
- For all other files, delegate the offset unchanged.
- Preserve SQLite's short-read rule by leaving the wrapped VFS responsible for
  zero-filling `SQLITE_IOERR_SHORT_READ` buffers.

`xWrite`:

- For shifted main database files, add 4096 to the logical offset before
  delegating.
- Never write through the preamble via SQLite payload I/O.

`xTruncate`:

- For shifted main database files, truncate to logical size plus 4096.
- For non-shifted files, delegate the requested size unchanged.
- Never truncate a shifted main database below the preamble boundary.

`xFileSize`:

- For shifted main database files, report `max(physical_size - 4096, 0)`.
- For non-shifted files, report the wrapped file size unchanged.

`xSync`, `xLock`, `xUnlock`, `xCheckReservedLock`, `xSectorSize`, and
`xDeviceCharacteristics`:

- Delegate unchanged to the wrapped file.

`xFileControl`:

- Delegate unrecognized file controls unchanged.
- When SQLite requests `SQLITE_FCNTL_SIZE_HINT` or `SQLITE_FCNTL_SIZE_LIMIT` on
  a shifted main database file, translate the pointed-to logical size to the
  physical size before delegation and translate the returned value back where
  the opcode is bidirectional.
- Return the wrapped VFS result code.

Temporary files and path names:

- `xFullPathname`, `xDelete`, `xAccess`, dynamic-loading methods, randomness,
  sleep, current-time, and last-error methods delegate to the wrapped VFS.
- `NULL` temporary filenames are delegated unchanged; those files are not
  shifted because they are not `SQLITE_OPEN_MAIN_DB`.

## Journal, WAL, Mmap, Backup, And Vacuum Policy

This phase allows SQLite rollback-journal operation only. The public MyLite open
path sets the opened SQLite connection to `journal_mode=DELETE` for the shifted
payload. DELETE-mode rollback journals may appear temporarily during writes, but
the durable database remains the single `.mylite` file after clean close.

WAL is deferred because it requires shared-memory and WAL file behavior to be
specified and tested together with shifted main database offsets. The version-1
VFS shim does not provide shared-memory methods.

Mmap is deferred. The open path sets `mmap_size=0`, and the version-1 VFS shim
does not provide fetch/unfetch methods.

Backup, `VACUUM`, and `VACUUM INTO` are follow-on proofs. Tests in this phase
should not claim those paths are complete.

## Runtime Behavior

`mylite_open()` must allocate and initialize `mylite_db` with the same session,
diagnostics, and bootstrap state shape as `mylite_open_memory()`.

Every successful file-backed handle must:

- own a private SQLite connection;
- have a valid MyLite preamble at physical byte 0;
- expose a SQLite payload whose logical byte 0 maps to physical byte 4096;
- run the same SQLite bootstrap policy as memory handles;
- keep trusted-schema, foreign-key placeholder, client-data, registration, hook,
  diagnostics, statement context, result metadata, and session behavior intact.

Independent file-backed handles must have independent `mylite_db` objects,
SQLite connections, bootstrap state, and file paths.

## Cleanup And Failure Unwinding

All non-trivial open-state and VFS file objects must be zero-initialized before
use. Their cleanup functions must tolerate zero-initialized objects.

On failure after allocating `mylite_db`, the implementation must:

1. deinitialize SQLite bootstrap state when it was initialized;
2. close the SQLite connection when it was opened;
3. deinitialize diagnostics;
4. free the database handle;
5. remove a newly created `.mylite` file when the open operation did not
   publish a handle;
6. leave `*out_db == NULL`.

On failure inside VFS `xOpen`, the wrapper must close and free any inner file
state it allocated and must leave wrapper `pMethods == NULL` unless it is safe
for SQLite to call `xClose()`.

## Parser And Grammar

No MyLite SQL syntax is added. No Lemon grammar snippets apply to this feature.

## Compatibility Notes

No MySQL runtime probe is attached to this feature because there is no
user-visible SQL behavior. The tests verify MyLite's internal storage contract
and SQLite VFS integration. The first MySQL comparison fixture should land with
the first public SQL feature that depends on file-backed storage.

`COMPATIBILITY.md` remains unchanged.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with
dotted CTest names.

Required coverage:

- creating a new `.mylite` file writes a valid preamble at byte 0 and places the
  SQLite payload header at byte 4096 after an internal SQLite write;
- reopening an existing `.mylite` file preserves data written through internal
  SQLite test helpers;
- invalid preambles, truncated preambles, and plain SQLite databases at byte 0
  are rejected without modification;
- independent file-backed handles have independent SQLite connections and
  bootstrap policy state;
- zero-initialized open-state and VFS/open cleanup paths are safe;
- temporary rollback-journal side effects from internal SQLite operations do
  not overwrite the preamble.

## Build Integration

Add any new first-party runtime or storage sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.file_backed_open$' --output-on-failure
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, public ABI exposure,
file-format safety, VFS correctness, zero-initialized cleanup, failure
unwinding, scope control, and test relevance.
