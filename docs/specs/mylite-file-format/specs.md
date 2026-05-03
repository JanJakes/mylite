# MyLite File Format Preamble

## Status

This feature introduces the first `.mylite` container experiment. A MyLite file
starts with a MyLite-owned preamble and exposes a shifted SQLite database payload
through a SQLite VFS shim. The goal is to determine whether MyLite can keep the
SQLite amalgamation pristine while still owning the outer file format.

Sidecar `-journal`, `-wal`, and `-shm` files are allowed in this experiment.
They are delegated to SQLite's native VFS rather than embedded into the
`.mylite` file.

The `sqlite-begin-concurrent-prototype` experiment currently requests WAL mode
for file-backed `mylite_open()` handles, so `-wal` and `-shm` sidecars are
expected during ordinary prototype execution.

## Sources

- SQLite Database File Format:
  https://www.sqlite.org/fileformat.html
- SQLite VFS documentation:
  https://www.sqlite.org/vfs.html
- SQLite VFS C API:
  https://www.sqlite.org/c3ref/vfs.html
- SQLite I/O methods C API:
  https://www.sqlite.org/c3ref/io_methods.html

## Preamble Layout

The MyLite preamble is 4096 bytes. This is a fixed format-version-1 boundary:
SQLite's logical byte `0` maps to physical byte `4096`, and the preamble before
that offset belongs to MyLite.

All integer fields are big-endian.

| Offset | Size | Field | Value |
| --- | ---: | --- | --- |
| 0 | 16 | Magic | `MyLite format 1\0` |
| 16 | 2 | MyLite file format version | `1` |
| 18 | 4078 | Reserved | zero-filled |

The SQLite payload offset is not stored in the file. It is fixed by MyLite file
format version 1. This avoids duplicated truth in the preamble while keeping
the payload aligned for ordinary page sizes and mmap-friendly file access. Future
formats that need a different payload offset must use a new format version.

## VFS Behavior

The `mylite` SQLite VFS wraps the platform default VFS.

For `SQLITE_OPEN_MAIN_DB` files:

- empty files are initialized with the MyLite preamble
- existing files must have a valid MyLite preamble
- SQLite logical offset `0` maps to physical offset `4096`
- reads, writes, truncation, file size, size hints, and mmap fetches are
  translated by the payload offset
- locking, syncing, shared-memory methods, device characteristics, pathname
  handling, randomness, sleeping, and dynamic-loading hooks delegate to the
  underlying VFS

For journal, WAL, shared-memory, temporary, transient, and other non-main
database files, the VFS delegates directly to the underlying platform VFS
without adding a MyLite preamble or offset.

## Compatibility Decisions

- Plain SQLite files are not accepted by `mylite_open()`.
- `.mylite` files are not directly readable by default SQLite tools because
  the SQLite header starts at offset `4096`, not offset `0`.
- Sidecar files are acceptable for this experiment. A future single-file
  guarantee may virtualize or embed them, but that is not part of this slice.
- The SQLite amalgamation remains unpatched. If future work requires SQLite
  internal changes, MyLite should patch canonical SQLite sources and regenerate
  the amalgamation rather than editing `sqlite3.c` directly.

## Test Plan

Fast C tests must cover:

- `mylite_open()` creates a file with the MyLite magic at offset `0`
- the preamble records format version `1` and keeps reserved bytes zero-filled
- direct SQLite access through the `mylite` VFS writes the SQLite header at
  physical offset `4096`
- default SQLite access to the same file rejects it as a normal SQLite database
- reopening through MyLite still executes the seed `SELECT 123` path
