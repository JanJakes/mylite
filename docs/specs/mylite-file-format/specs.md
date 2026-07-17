# MyLite File Format Preamble

## Status

The `.mylite` container uses a 4096-byte MyLite-owned preamble followed by a
shifted SQLite payload. Format version 2 is current. Format version 1 remains
readable as a legacy committed file.

The identity-bound creation and publication protocol is specified in
`docs/specs/storage-file-initialization-lifecycle/specs.md`. Offset translation
is specified in `docs/specs/file-backed-mylite-opening-vfs/specs.md`.

## Sources

- SQLite Database File Format: https://www.sqlite.org/fileformat.html
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`

## Preamble Layout

All integer fields are big-endian. The SQLite payload begins at physical byte
4096 in both supported versions.

### Version 2

| Offset | Size | Field | Value |
| --- | ---: | --- | --- |
| 0 | 16 | Magic | `MyLite format 1\0` |
| 16 | 2 | MyLite file format version | `2` |
| 18 | 1 | Lifecycle state | `1`, `2`, or `3` |
| 19 | 4077 | Reserved | zero-filled |

Lifecycle values are:

- `1`: initialization is in progress and the file is not openable;
- `2`: initialization is committed and the file may be opened;
- `3`: recovery is required and ordinary open must reject the file.

The magic text remains unchanged so format-family detection is stable; the
numeric version field governs layout interpretation.

### Legacy Version 1

| Offset | Size | Field | Value |
| --- | ---: | --- | --- |
| 0 | 16 | Magic | `MyLite format 1\0` |
| 16 | 2 | MyLite file format version | `1` |
| 18 | 4078 | Reserved | zero-filled |

An otherwise valid version-1 preamble is interpreted as committed. Any nonzero
legacy reserved byte is invalid.

## Validation

The file-format helper:

- initializes a version-2 preamble in a requested lifecycle state;
- initializes committed version-2 preambles for ordinary test fixtures;
- classifies version-1 and version-2 lifecycle state;
- validates only committed preambles as ordinarily openable; and
- reads big-endian unsigned 16-bit fields.

Validation rejects mismatched magic, unsupported versions, invalid lifecycle
values, and nonzero reserved bytes. It does not perform file I/O. The VFS adds
exact-handle file-size and SQLite-header validation.

## Compatibility Decisions

- Plain SQLite files are not `.mylite` files and are rejected.
- Existing version-1 files remain readable and are not rewritten on open.
- New files use version 2 and are not readable by binaries that only understand
  version 1.
- `created_with_file_format_version` in the catalog records provenance. A
  supported historical value is accepted and is not required to equal the
  current writer version.
- Interrupted or failed version-2 initialization is rejected deterministically;
  automatic repair is deferred until payload and catalog recovery can be
  proven safe.

## Test Plan

Fast C tests cover magic, versions, lifecycle classification, reserved bytes,
legacy acceptance, invalid state and corruption rejection, and big-endian field
reads. File-backed tests cover exact-handle publication and open behavior.
