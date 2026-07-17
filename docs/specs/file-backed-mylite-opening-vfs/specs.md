# File-Backed MyLite Opening VFS

## Status

MyLite opens `.mylite` files through a MyLite-owned SQLite VFS that exposes a
logical SQLite database at physical offset 4096. The public API is:

```c
MYLITE_API int mylite_open(const char *path, mylite_db **out_db);
```

`path` and `out_db` are required. Success returns an owned handle released with
`mylite_close()`. Failure leaves `*out_db == NULL` and returns `MYLITE_MISUSE`,
`MYLITE_NOMEM`, or `MYLITE_ERROR` as appropriate. SQLite types remain private.

This infrastructure has no MySQL-visible SQL behavior and therefore requires
no MySQL runtime expectation fixture or compatibility-matrix change.

## Related Specifications

- File-format layout: `docs/specs/mylite-file-format/specs.md`
- Creation/publication lifecycle:
  `docs/specs/storage-file-initialization-lifecycle/specs.md`
- Lock-byte mapping: `docs/specs/storage-lock-byte-mapping/specs.md`
- SQLite bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`

## Open Protocol

MyLite first asks the offset VFS to create the main database exclusively. The
VFS translates this request into the wrapped platform VFS's real exclusive
create flag. A successful creator writes and syncs a version-3 `initializing`
preamble through the exact underlying `sqlite3_file` before returning it to
SQLite.

If exclusive creation reports that the path already exists, MyLite retries a
read-write open without create. Through that exact underlying file object the
VFS verifies:

- a committed supported preamble;
- a physical size large enough for the preamble and minimum SQLite database;
  and
- the SQLite header at physical byte 4096.

After bootstrap and catalog initialization, storage resolves the exact main
`sqlite3_file` with `SQLITE_FCNTL_FILE_POINTER` and syncs the committed lifecycle
state. Failed initialization marks that same file recovery-required. It never
deletes an unverified pathname.

## Offset VFS

The registered version-1 VFS wrapper owns an inner file allocated to the
wrapped default VFS's `szOsFile`. Only `SQLITE_OPEN_MAIN_DB` files shift logical
offsets. Journals, temporary files, and other auxiliary files delegate offsets
unchanged.

For a shifted main file below the lock split:

- `xRead` and `xWrite` add 4096 after overflow checks;
- `xTruncate` adds 4096 and cannot truncate the preamble;
- `xFileSize` reports `max(physical_size - 4096, 0)`;
- `SQLITE_FCNTL_SIZE_HINT` and `SQLITE_FCNTL_SIZE_LIMIT` translate logical and
  physical sizes;
- sync, lock, unlock, reserved-lock checks, and sector size delegate to the
  wrapped file;
- mapped I/O and atomic-write capabilities are disabled; and
- lifecycle transitions write physical preamble offsets directly and sync.

Version 3 inserts a second physical gap at the lock-byte boundary and splits
crossing reads and writes. Versions 1 and 2 retain linear translation but
cannot grow past the safe boundary. The lock-byte specification defines the
exact mapping and size-control behavior.

Path, delete, access, dynamic-loading, randomness, sleep, time, and last-error
VFS methods delegate to the wrapped VFS. Failed `xOpen` closes any partially
opened inner object and leaves the wrapper unusable by SQLite.

## Journal And Mmap Policy

Connections use rollback-journal `DELETE` mode. The journal is auxiliary and
unshifted; after clean close, the durable database remains one `.mylite` file.
`mmap_size` is zero and the VFS exposes no fetch/unfetch methods. WAL and shared
memory remain unsupported until their single-file and shifted-offset behavior
is specified and qualified.

## Runtime Ownership

File-backed and memory handles share database-handle allocation, session state,
diagnostics, bootstrap policy, and teardown. Each successful file-backed handle
owns a private SQLite connection. Bootstrap and catalog state are complete
before the handle is published.

Teardown closes statements and bootstrap/catalog resources before SQLite.
Failed open marks an owned initializing file recovery-required, closes all
resources, frees the handle, and leaves the pathname untouched.

## Tests

Coverage includes:

- version-3 creation, shifted SQLite header, catalog initialization, and reopen;
- legacy version-1 and version-2 reopen;
- invalid, empty, truncated, plain-SQLite, preamble-only, initializing, and
  recovery-required files;
- independent handles and bootstrap state;
- second-open rejection while initialization is active;
- process-death rejection before lifecycle publication;
- POSIX rename/replacement during abort, proving only the opened inode receives
  recovery state and the replacement pathname is unchanged; and
- sparse lock-boundary mapping and legacy growth containment; and
- Release and sanitizer execution.
