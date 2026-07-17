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
- `recovery-required`: initialization failed after exclusive creation and the
  file must not be opened as a database.

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
2. writes and syncs a version-2 `initializing` preamble through the exact
   underlying `sqlite3_file` handle;
3. exposes logical offset zero at the shifted payload boundary; and
4. records ownership on that VFS file instance.

If exclusive creation fails, MyLite retries a read-write open without create.
The offset VFS validates the preamble and SQLite header through that exact
underlying handle. It accepts only committed version-1 or version-2 files.

After SQLite bootstrap and catalog initialization succeed, MyLite resolves the
exact main file through `SQLITE_FCNTL_FILE_POINTER` and asks the storage VFS to
write and sync the `committed` lifecycle byte. The database handle is not
returned before that publication succeeds.

## Failure And Recovery

If initialization fails after exclusive creation, MyLite resolves that same VFS
file instance and writes and syncs `recovery-required` before closing. It does
not unlink by pathname. A pathname replacement therefore cannot be removed by
the failing opener.

Opening an `initializing` or `recovery-required` file fails deterministically.
Automatic repair is out of scope until catalog and payload recovery can prove
which initialization steps reached durable storage. A future repair API may
inspect or rebuild such files explicitly.

An existing empty file, a truncated preamble, a committed preamble without a
complete SQLite header, an unsupported format version, invalid lifecycle
state, or nonzero reserved bytes is rejected and left unchanged.

## Offset VFS Requirements

- Lifecycle reads and writes target physical preamble offsets and never pass
  through logical payload shifting.
- Main-database payload reads, writes, truncation, size reporting, and size
  controls retain the existing 4096-byte logical-to-physical shift.
- Journal and other auxiliary files are not shifted and do not carry MyLite
  lifecycle state.
- Preamble state transitions are synced before success is reported.
- Existing committed files are never rewritten merely because they are opened.

## Tests

Coverage must include:

- successful version-2 exclusive creation and committed publication;
- reopen of version-2 and legacy version-1 files;
- invalid, empty, truncated, plain-SQLite, preamble-only, initializing, and
  recovery-required files;
- a second opener while initialization is not committed;
- process death before publication, preserving `initializing` and rejecting
  ordinary reopen;
- failure publication through the owning VFS file;
- pathname replacement during failure cleanup, proving the replacement remains;
- independent files and handles;
- normal and ASan/UBSan execution on supported CI platforms.
