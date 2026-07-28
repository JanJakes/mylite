# Length-Aware Database Paths

## Status

Specified. Implementation and qualification are pending. This feature closes
security review finding SEC-03.

## Scope And Sources

This feature defines database-path transport from the public C ABI through the
core PHP, mysqli replacement, and PDO MyLite adapters. It is independently
specified from:

- `README.md` and `docs/architecture/engineering-standards.md`;
- `docs/specs/file-backed-mylite-opening-vfs/specs.md`;
- the existing public `mylite_open*()` ABI;
- PHP's public length-aware string and PDO driver interfaces; and
- observed operating-system file behavior in first-party tests.

Database filenames are an embedded MyLite concern rather than a MySQL server
behavior. There is no corresponding MySQL 8.4.9 runtime fixture. SQL behavior,
the `.mylite` format, catalog bootstrap, and logical database selection do not
change.

## Problem

PHP strings retain an explicit byte length and may contain NUL bytes. The
current adapters eventually pass those strings to NUL-terminated native open
functions. A value such as:

```text
/authorized/application.mylite\0/attacker-controlled-suffix
```

is therefore validated and routed as one PHP value but opened by the operating
system as its prefix. The PDO adapter also duplicates its data source with
`estrdup()`, discarding the known PDO data-source length before native open.

This is both an identity error and an authorization-boundary hazard. Rejection
must occur before SQLite, the offset VFS, or the platform filesystem observes a
path.

## Public C ABI

The library adds these ABI functions:

```c
MYLITE_API int mylite_open_with_size(
    const char *path,
    size_t path_size,
    mylite_db **out_db
);

MYLITE_API int mylite_open_with_size_and_diagnostic(
    const char *path,
    size_t path_size,
    mylite_db **out_db,
    struct mylite_open_diagnostic *out_diagnostic
);
```

`path` is a borrowed byte span valid for the duration of the call. `path_size`
is the exact number of path bytes and excludes any optional C terminator. The
library copies the span into private NUL-terminated storage only after complete
validation. The caller retains ownership of the input. Successful handle
ownership and diagnostic ownership remain identical to the existing open
functions.

The sized functions apply this validation order:

1. initialize `out_diagnostic` when supplied;
2. reject a null `out_db`;
3. set `*out_db = NULL`;
4. reject a null path, zero size, a size that cannot be terminated without
   overflow, or any NUL within `[path, path + path_size)`;
5. allocate and terminate a private copy;
6. continue through the existing file-backed open protocol.

Invalid arguments return `MYLITE_MISUSE`. Allocation failure returns
`MYLITE_NOMEM`. No invalid path reaches database-handle allocation, SQLite, the
offset VFS, or a platform file operation. On every failure `*out_db` remains
null and a supplied diagnostic is independently valid.

The existing functions remain source- and ABI-compatible convenience wrappers:

```c
MYLITE_API int mylite_open(const char *path, mylite_db **out_db);
MYLITE_API int mylite_open_with_diagnostic(
    const char *path,
    mylite_db **out_db,
    struct mylite_open_diagnostic *out_diagnostic
);
```

They measure nonnull paths with `strlen()` and delegate to the sized contract.
Consequently they accept ordinary NUL-terminated C paths but cannot describe
bytes after the first NUL; callers that already know a length must use the
sized API.

The native file-open functions remain file-backed. They do not reinterpret the
literal filename `:memory:`. Native callers use `mylite_open_memory*()` for
memory databases. PHP adapters retain their established convention that only
the exact eight-byte token `:memory:` selects a memory handle.

## Core PHP Adapter

Both `mylite_open()` and `MyLite\Connection::__construct()` retain the
Zend-provided string length:

- the exact `:memory:` token calls `mylite_open_memory_with_diagnostic()`;
- every other nonempty value calls
  `mylite_open_with_size_and_diagnostic()` with its exact byte count;
- empty or NUL-bearing strings fail with `MyLite\Exception` and no handle.

Beginning, middle, or final NUL bytes are rejected equally. An invalid value
must not create its visible prefix, open an existing prefix, or change any
prefix file bytes.

## mysqli Replacement

mysqli continues to resolve embedded paths from its supported sources:

- `mylite:<path>` host values;
- path-like hosts and `localhost:<path>` hosts;
- nonempty socket values; and
- path-like database arguments.

Resolution uses the original argument lengths and returns a length-bearing
`zend_string`. The selected path is passed to the sized native API. Any NUL in
the selected span fails connection setup with the normal MyLite connection
error surface before the link publishes the path or touches a file.

An empty host/socket/database selection keeps the existing memory fallback.
The exact `:memory:` token selects memory. A token with a leading, embedded, or
trailing NUL is not a memory token and is rejected.

Unselected conventional username, password, or logical-database values are not
filesystem paths and are outside this feature. Logical database identifiers
continue through their separate length-aware quoting path.

## PDO MyLite Adapter

PDO MyLite uses both `dbh->data_source` and `dbh->data_source_len`. Path
resolution returns a `zend_string` rather than an `estrdup()` result:

- `path=<value>` strips the exact five-byte prefix and preserves the remaining
  length;
- other data sources preserve the complete data-source span;
- zero remaining bytes fail as a missing path;
- any NUL in the resolved span fails before native open; and
- only the exact `:memory:` span selects memory.

The constructor and PHP 8.4 `PDO::connect()` share the same driver factory, so
the matrix applies to both. Rejection uses PDO's ordinary connection exception
surface and leaves no persistent handle or filesystem side effect.

## Framework Packages

`mylite/laravel-driver` and `mylite/doctrine-dbal-driver` retain their
configuration-boundary NUL checks. They are defense-in-depth checks; the PDO
driver remains authoritative when called directly or through another
framework.

## Encoding And Platform Semantics

MyLite treats a path as platform filename bytes expressed through SQLite's
UTF-8 filename interface. It does not normalize Unicode, separators, dot
segments, case, or symbolic links. A valid non-ASCII UTF-8 path is transported
unchanged and is subject to the platform filesystem's normal behavior.

The validation rule is platform-independent and applies on POSIX and Windows.
Windows qualification uses the same first-party native test through the
existing Windows CI matrix. No adapter-specific Windows PHP build is claimed.

## Test Matrix

### Native

The file-backed open test covers:

- null path, null output, empty span, and impossible terminator size;
- a non-terminated valid byte span;
- NUL at the beginning, middle, and final byte;
- an exact `:memory:`-prefix bypass shape with an included NUL;
- an authorized-looking existing/absent prefix followed by NUL and suffix;
- a valid non-ASCII path;
- unchanged output handles and diagnostics after rejection;
- absence or byte-for-byte preservation of every visible prefix; and
- zero create, open, truncate, and delete VFS calls for rejected spans.

The same test runs in Release, Debug, Windows, ASan/UBSan, and deterministic
VFS-fault CI profiles.

### PHP

The core, mysqli, and PDO package suites cover beginning, middle, and final NUL
placement, empty and exact-memory values, non-ASCII file paths, missing prefix
creation, and existing prefix preservation. mysqli covers host, localhost-host,
socket, and database-path routing. PDO covers plain and `path=` data sources.

All adapter failures are checked with their normal exception/error class and
must occur before a visible prefix changes.

## Compatibility Boundary

This feature does not claim:

- arbitrary binary filenames containing NUL, which platform file APIs cannot
  represent;
- path authorization, canonicalization, sandboxing, symlink defense, or access
  control;
- Unicode normalization or cross-platform filename equivalence;
- network MySQL host, socket, TLS, or protocol behavior; or
- a change to the logical MySQL database-name contract.

## Qualification

Before the compatibility row becomes supported:

- exact ABI and public-header snapshots include both additive functions;
- the native path matrix passes Release, Debug, Windows, ASan/UBSan, and VFS
  fault profiles;
- all three PHP adapter suites pass in Release and ASan/UBSan builds;
- formatting, static analysis, installation, and production-size gates pass;
- documentation identifies sized APIs as authoritative for length-bearing
  callers; and
- the compatibility-claim validator passes.
