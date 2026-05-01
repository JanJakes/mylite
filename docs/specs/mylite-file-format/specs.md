# MyLite File Format Preamble

## Status

This feature defines the first `.mylite` container preamble and adds internal
helpers for initializing and validating it. Runtime file opening, shifted SQLite
I/O, journaling, WAL, shared-memory behavior, and any required SQLite extension
point or fork patch are intentionally out of scope for this slice.

## Sources

- SQLite Database File Format:
  https://www.sqlite.org/fileformat.html

## Preamble Layout

The MyLite preamble is 4096 bytes. This is a fixed format-version-1 boundary:
future SQLite payload bytes begin at physical byte `4096`, and the preamble
before that offset belongs to MyLite.

All integer fields are big-endian.

| Offset | Size | Field | Value |
| --- | ---: | --- | --- |
| 0 | 16 | Magic | `MyLite format 1\0` |
| 16 | 2 | MyLite file format version | `1` |
| 18 | 4078 | Reserved | zero-filled |

The future SQLite payload offset is not stored in the file. It is fixed by
MyLite file format version 1. This avoids duplicated truth in the preamble while
keeping the payload aligned for ordinary page sizes and mmap-friendly file
access. Future formats that need a different payload offset must use a new
format version.

## Helper Behavior

The internal file-format helper module exposes three operations:

- initialize a 4096-byte buffer with the version-1 preamble
- validate a 4096-byte buffer as a version-1 preamble
- read big-endian unsigned 16-bit fields from a preamble buffer

Validation rejects any mismatched magic, unsupported format version, or non-zero
reserved byte. The helper does not perform file I/O.

## Compatibility Decisions

- Plain SQLite files cannot be valid `.mylite` files because they lack the
  MyLite preamble.
- `.mylite` files are not meant to be directly readable by default SQLite tools
  once runtime storage lands, because the SQLite header will start at offset
  `4096`, not offset `0`.
- Sidecar files, VFS behavior, and any required targeted SQLite extension point
  or shifted-offset fork patch are deferred to runtime storage work.

## Test Plan

Fast C tests must cover:

- initialized preambles contain the MyLite magic at offset `0`
- initialized preambles record format version `1`
- initialized preambles keep reserved bytes zero-filled
- validation accepts an initialized preamble
- validation rejects corrupted magic, version, and reserved bytes
- big-endian unsigned 16-bit field reads are stable
