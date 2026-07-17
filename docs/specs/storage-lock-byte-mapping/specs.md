# Storage Lock-Byte Mapping

## Status

This specification defines MyLite file-format version 3 and the mapping between
SQLite logical database offsets and physical `.mylite` offsets around SQLite's
pending-lock byte range.

## Problem

SQLite reserves physical bytes beginning at `0x40000000` for database locks and
never allocates the logical database page beginning at that same address.
MyLite file-format versions 1 and 2 add a 4096-byte physical preamble while
presenting an unshifted logical database to SQLite. Consequently, the logical
range immediately before SQLite's skipped page maps onto the underlying VFS
lock range.

The overlap is unsafe on VFS implementations that enforce byte-range locks and
can produce platform-dependent write failures or corruption near the 1 GiB
boundary.

## Version 3 Mapping

Version 3 retains the 4096-byte preamble and adds a 4096-byte physical lock gap.
The gap starts at SQLite's physical pending-lock byte:

```text
payload offset       = 0x00001000
physical lock byte   = 0x40000000
logical split        = 0x3ffff000
physical lock gap    = [0x40000000, 0x40001000)
```

Logical offsets below `0x3ffff000` map to `logical + 0x1000`. Logical offsets at
or above `0x3ffff000` map to `logical + 0x2000`. Reads and writes crossing the
split are divided into two underlying VFS operations.

This leaves the complete physical lock page untouched. SQLite continues to skip
its ordinary logical pending-byte page at `0x40000000`; no SQLite fork hook or
global pending-byte change is required.

Logical file sizes at or below the split map through the preamble only. Larger
logical sizes include both physical gaps. Physical sizes that terminate inside
the lock gap are invalid.

## Legacy Versions

Versions 1 and 2 retain their original linear `logical + 4096` mapping. They may
be opened only when their physical size does not exceed `0x40000000`, which is a
logical size of `0x3ffff000`.

The VFS rejects legacy writes, truncations, and size hints that would cross that
logical limit. Existing legacy files beyond the limit are rejected rather than
opened with an ambiguous or unsafe mapping.

## VFS Controls

- `SQLITE_FCNTL_SIZE_HINT` and `SQLITE_FCNTL_SIZE_LIMIT` translate logical and
  physical sizes using the active format mapping.
- Physical chunk sizing is disabled because underlying-VFS rounding could end a
  file inside the reserved gap and make its logical size ambiguous.
- Memory mapping remains disabled because the offset VFS exposes version-1 I/O
  methods without `xFetch` or `xUnfetch`.
- Atomic-write capability bits are cleared because a logical page crossing the
  lock split requires two physical writes.
- The underlying sector size must divide the 4096-byte preamble and lock-gap
  alignment; incompatible VFSes are rejected. Synchronization, locking, and
  reserved-lock checks continue to delegate to the underlying VFS.
- Auxiliary journal files are not shifted. Their page records contain logical
  database pages and remain compatible with the split main-file mapping.

## Failure Behavior

Invalid offsets, arithmetic overflow, malformed physical sizes, and legacy
boundary violations return SQLite I/O or full-file errors. MyLite surfaces the
ordinary statement diagnostic and never wraps into the preamble or lock gap.

## Compatibility

The mapping is internal to the `.mylite` container and does not alter MySQL SQL
semantics. Version 3 readers accept valid versions 1 and 2 subject to the legacy
size limit. Older readers reject version 3 through the existing format-version
validation contract.

## Verification

Coverage must verify:

- below-boundary, crossing-boundary, at-boundary, and above-boundary mapping;
- the complete physical lock gap remains unchanged;
- sparse logical size reporting and truncation on both sides of the split;
- deterministic rejection of legacy growth and oversized legacy files;
- size-control translation and disabled atomic-write claims;
- ordinary rollback-journal transactions before and after reopen;
- supported SQLite page sizes whose page I/O crosses the split;
- POSIX and Windows builds.
