# Storage File Initialization Lifecycle

## Purpose

MyLite must create, validate, initialize, and publish a `.mylite` database
without changing file identity between those operations. Concurrent first
openers must not both initialize one pathname, and failed cleanup must never
unlink a replacement file that MyLite did not create.

This is a storage correctness contract. It does not change MySQL-visible SQL,
metadata, diagnostics, or compatibility status.

## File Lifecycle

Preamble format version 2 stores one lifecycle byte after the format version:

- `initializing`: an exclusive creator owns initialization and the SQLite
  payload is not yet published;
- `committed`: the shifted SQLite payload and MyLite catalog completed
  initialization and the file may be opened normally;
- `recovery-required`: initialization failed after exclusive creation and a
  later opener must prove exclusive recovery ownership before inspecting it.

Format-version-1 files have an all-zero reserved area and are interpreted as
legacy committed files. New files use format version 2. Reserved bytes not
defined by version 2 remain zero and are validated on every open.

## Identity And Open Protocol

The initial SQLite open is an exclusive-create attempt. MyLite marks the open
mode in thread-local VFS context, and the offset VFS adds the base-VFS
`SQLITE_OPEN_EXCLUSIVE` flag only to the main-database file. This ensures that
exclusive creation is implemented by the platform VFS rather than by a prior
pathname check.

If exclusive creation succeeds, the offset VFS:

1. verifies that the new file is empty;
2. acquires the wrapped VFS's shared main-file lock and upgrades it to
   exclusive, following SQLite's required `NO_LOCK` to `SHARED` to
   `EXCLUSIVE` transition sequence, then retains the exclusive lock for the
   complete unpublished lifecycle;
3. writes and syncs a version-3 `initializing` preamble through the exact
   underlying `sqlite3_file` handle;
4. exposes logical offset zero at the shifted payload boundary; and
5. records ownership on that VFS file instance.

If exclusive creation fails, MyLite retries a read-write open without create.
The offset VFS validates the preamble through that exact underlying handle.
Committed files require a complete SQLite header. An `initializing` or
`recovery-required` version-2 or version-3 file is accepted only after that
same file handle acquires the wrapped VFS's exclusive lock. Failure to acquire
the lock means a creator or another recovery owner is still live, so the open
fails without changing the file.

After SQLite bootstrap and catalog initialization succeed, MyLite resolves the
exact main file through `SQLITE_FCNTL_FILE_POINTER` and asks the storage VFS to
write and sync the `committed` lifecycle byte and release the retained lock.
The database handle is not returned before that publication succeeds.

## Failure And Recovery

If initialization fails after exclusive creation, MyLite resolves that same VFS
file instance and writes and syncs `recovery-required` before closing. It does
not unlink by pathname. A pathname replacement therefore cannot be removed by
the failing opener.

An exclusive recovery owner may complete initialization without deleting,
replacing, or truncating the file:

- a preamble-only file exposes an empty logical SQLite payload and reruns
  bootstrap and catalog initialization;
- a payload with a complete SQLite header enters normal SQLite hot-journal
  recovery, bootstrap, migration, and full catalog/physical validation; and
- a partial or corrupt payload that SQLite and MyLite cannot validate remains
  recovery-required and opening fails.

Only successful SQLite recovery and full catalog validation permit committed
publication. The lifecycle state alone never authorizes removal or truncation,
so a committed database whose byte was changed to a recovery state is
validated and preserved rather than erased.

An existing empty file, a truncated preamble, a committed preamble without a
complete SQLite header, an unsupported format version, invalid lifecycle
state, or nonzero reserved bytes is rejected and left unchanged.

## Offset VFS Requirements

- Lifecycle reads and writes target physical preamble offsets and never pass
  through logical payload shifting.
- The retained initialization/recovery lock satisfies SQLite lock upgrades on
  the owning handle; SQLite unlock requests do not release it before lifecycle
  publication.
- Lock acquisition and release must preserve the wrapped VFS's per-file and
  per-inode lock accounting. After publication, the creator handle must
  participate in same-process reader/writer exclusion exactly like a handle
  that opened an already committed file.
- Main-database payload reads, writes, truncation, size reporting, and size
  controls retain the existing 4096-byte logical-to-physical shift.
- Journal and other auxiliary files are not shifted and do not carry MyLite
  lifecycle state.
- Preamble state transitions are synced before success is reported.
- Existing committed files are never rewritten merely because they are opened.
- Creator death releases the wrapped VFS lock through the operating system even
  though the lifecycle byte remains initializing.

## Tests

Coverage must include:

- successful version-3 exclusive creation and committed publication;
- reopen of version-3, version-2, and legacy version-1 files;
- invalid, empty, truncated, plain-SQLite, and committed-preamble-only files;
- a second opener while initialization is not committed;
- a reader retained on the successful creator handle blocking a same-process
  writer until its active cursor is released;
- process death before payload creation, during bootstrap/catalog transactions,
  after catalog commit, and before lifecycle publication;
- successful exclusive recovery of preamble-only and fully initialized
  unpublished files;
- deterministic rejection of invalid unpublished payloads without truncation;
- failure publication through the owning VFS file;
- pathname replacement during failure cleanup, proving the replacement remains;
- independent files and handles;
- normal and ASan/UBSan execution on supported CI platforms.
