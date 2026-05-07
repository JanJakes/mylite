# Runtime Handles And Statement Context

## Status

This feature specifies MyLite's first runtime ownership layer. It adds minimal
database and statement-context handles before user-visible SQL execution grows.
It is internal compatibility infrastructure and does not move any
`COMPATIBILITY.md` row out of unsupported status by itself.

The first implementation should prove that MyLite owns the SQLite connection,
session state, diagnostics, statement lifecycle, and planned-result metadata
boundary before DDL, DML, catalog descriptors, type conversion, or SQLite fork
hooks depend on those objects.

## Sources

- MyLite baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MyLite parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- SQLite source snapshot notes:
  `third_party/sqlite/README.md`
- SQLite application-defined functions:
  https://www.sqlite.org/appfunc.html
- SQLite application-defined collations:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite authorizer:
  https://sqlite.org/c3ref/set_authorizer.html
- SQLite tracing:
  https://www.sqlite.org/c3ref/trace_v2.html
- SQLite update, commit, and rollback hooks:
  https://www.sqlite.org/c3ref/update_hook.html

This specification is independently authored from project documentation,
observed branch experiments discussed in the baseline strategy, and public
SQLite documentation. It does not copy MySQL, MariaDB, Percona, TiDB, or other
restrictively licensed implementation sources.

## Scope

The first runtime slice must add:

- opaque database handle type ownership for `mylite_db`;
- internal SQLite connection ownership behind that handle;
- minimal public open and close APIs;
- internal session state for default schema, SQL modes, time zone placeholder,
  character-set placeholder, user identity placeholder, system-variable
  placeholder, and catalog generation counters;
- connection-owned diagnostics area;
- statement context for one top-level statement boundary;
- planned-result metadata structs sufficient to describe non-SQLite result
  sets later;
- runtime tests that prove handle lifecycle, diagnostics reset, statement
  context lifecycle, and SQLite connection ownership.

The first slice may open only in-memory SQLite databases. File-backed ordinary
SQLite opening and `.mylite` preamble-aware opening belong to the next storage
slice.

## Non-Goals

This feature must not implement:

- user-visible SQL execution support;
- `CREATE TABLE`, DDL, DML, catalog tables, or information-schema virtual
  tables;
- SQLite fork patches;
- type descriptors or assignment conversion;
- MySQL wire protocol;
- MySQL runtime comparison fixtures, because no MySQL SQL behavior is supported
  by this feature.

## Public API Shape

The public API should stay deliberately small:

```c
typedef struct mylite_db mylite_db;

#define MYLITE_OK 0
#define MYLITE_ERROR 1
#define MYLITE_MISUSE 21
#define MYLITE_NOMEM 7

MYLITE_API int mylite_open_memory(mylite_db **out_db);
MYLITE_API void mylite_close(mylite_db *db);

MYLITE_API int mylite_errcode(const mylite_db *db);
MYLITE_API const char *mylite_sqlstate(const mylite_db *db);
MYLITE_API const char *mylite_errmsg(const mylite_db *db);
```

`mylite_open_memory()` creates an independent MyLite database handle backed by a
private SQLite in-memory connection. `out_db` is required. On success, it stores
a non-NULL handle and returns `MYLITE_OK`. On failure, it stores `NULL` and
returns a MyLite status code.

`mylite_close(NULL)` is a no-op.

Diagnostic accessors return handle-owned state. For `NULL` handles,
`mylite_errcode()` returns `MYLITE_MISUSE`; `mylite_sqlstate()` returns
`"HY000"`; `mylite_errmsg()` returns a stable static misuse message.

The exact numeric values above intentionally mirror the broad meaning of common
SQLite result codes where doing so is harmless, but MyLite public constants are
owned by MyLite and must not expose SQLite as public ABI.

## Internal Runtime Objects

The first implementation should add internal modules under
`packages/libmylite/src/runtime/`:

- `mylite_connection.*` owns `struct mylite_db`, SQLite connection lifetime,
  session state, and high-level diagnostics helpers.
- `mylite_statement_context.*` owns one statement boundary, including warning
  reset, diagnostics reset, statement-stable time, affected rows, previous row
  count, first generated insert id, wrapper transaction state, and planned
  result metadata.
- `mylite_diagnostics.*` owns MySQL-shaped diagnostics records and warning
  lists.
- `mylite_result_metadata.*` owns planned-result column descriptors used by
  future `SHOW`, `DESCRIBE`, diagnostics, utility, and table-maintenance
  statements.

Internal module names may be adjusted if implementation pressure shows a better
split, but state ownership must remain explicit and connection-local.

## Session State

The initial connection state should include:

- selected default schema, initially no schema selected;
- current user identity placeholder, initially `root@%`;
- client user identity placeholder, initially `root@%`;
- SQL mode bitset initialized to the MySQL 8.4.9 default mode set once that
  value is verified by a runtime fixture;
- time-zone placeholder, initially system/local behavior is unspecified until
  temporal runtime work specifies it;
- character-set placeholders for client, connection, results, and collation
  state, initially defaulting to future `utf8mb4_0900_ai_ci` policy once the
  charset/collation foundation lands;
- catalog generation and SQLite schema generation counters, initially zero.

If a value is not yet MySQL-verified in this feature, it should be represented
as explicit placeholder state rather than guessed visible behavior.

## Diagnostics Area

The diagnostics area is handle-owned and statement-scoped. It must support:

- current error code;
- current SQLSTATE;
- current primary message;
- ordered warning records;
- warning count;
- reset at statement start;
- preservation after statement completion until the next statement boundary.

The first implementation only needs a compact fixed or growable internal
representation sufficient for tests. It should not expose warning iteration
publicly yet.

Diagnostic records should be able to represent parser, analyzer, runtime,
SQLite callback, SQLite native constraint, and future fork hook conditions,
even if only runtime and misuse conditions are populated in this slice.

## Statement Context

Each top-level statement should get a statement context before parser, analyzer,
SQLite prepare, or SQLite step work begins. The context owns:

- the input SQL view, if any;
- statement-stable time placeholder;
- affected-row and previous-row-count staging;
- first generated insert id staging;
- warning reset;
- diagnostics reset;
- planned result metadata;
- wrapper transaction or savepoint state;
- backend execution status.

The first implementation should expose internal init/deinit/begin/end helpers
and tests. It does not need to execute SQL yet.

Future statement execution must use this context for every top-level statement,
including statements lowered to multiple SQLite statements and statements that
return planned results without a SQLite result set.

## SQLite Connection Ownership

The SQLite connection is private to `mylite_db`. External callers must not
receive a public SQLite handle from the first runtime API.

Connection bootstrap should be factored so later slices can register:

- MySQL scalar, aggregate, and window callbacks;
- MySQL collations;
- virtual tables;
- authorizer, trace, busy, progress, update, pre-update, commit, and rollback
  hooks;
- SQLite connection policy such as trusted-schema and foreign-key settings;
- future fork client-data pointers.

The first implementation may only open SQLite and optionally set no-op-safe
connection policy. Function and collation registration belongs to a later
bootstrap slice unless a single smoke callback is useful for testing the
bootstrap shape.

## Planned Result Metadata

The runtime should define internal result metadata descriptors before `SHOW`
and `INFORMATION_SCHEMA` land. The first descriptor shape should be enough to
hold:

- MySQL-visible column label;
- schema, table, and origin names;
- logical type;
- flags;
- character set and collation ids;
- display length;
- decimals;
- nullability.

This feature does not need to expose result sets. It only needs the data
structures and construction/deinitialization tests so future planned-result
builders have a stable target.

## First Vertical Proof

The first vertical proof should be intentionally narrow:

1. Open a `mylite_db` with `mylite_open_memory()`.
2. Verify the handle owns a working SQLite in-memory connection through an
   internal test helper, not a public SQLite escape hatch.
3. Begin an internal statement context with borrowed SQL text.
4. Verify diagnostics and warnings reset at statement start.
5. Set an internal runtime diagnostic and one warning.
6. End the statement and verify diagnostics remain readable from the handle.
7. Begin a second statement and verify the previous statement diagnostics reset.
8. Build and deinitialize a small planned-result metadata object.
9. Close the database and verify cleanup paths tolerate partially initialized
   objects and `NULL`.

This proof demonstrates the runtime ownership path without pretending SQL
compatibility exists.

## Tests

Add fast C tests next to `packages/libmylite/tests/`:

- `libmylite.runtime.open_memory`: public open/close API, invalid arguments,
  repeated independent handles, close `NULL`.
- `libmylite.runtime.sqlite_owner`: internal test verifies each MyLite handle
  owns a distinct working SQLite connection.
- `libmylite.runtime.diagnostics`: set, read, reset, warning count, stable
  misuse diagnostics for `NULL` accessors.
- `libmylite.runtime.statement_context`: begin/end lifecycle, statement reset,
  previous diagnostics preservation until the next statement, zero-initialized
  deinit behavior.
- `libmylite.runtime.result_metadata`: construct/deinit planned-result metadata
  for a synthetic one-column result.

Tests should be simple C executables registered with CTest and should print
concise diagnostics to `stderr`.

No MySQL runtime fixture is attached to this feature. The first MySQL
comparison fixture should land with the first user-visible SQL behavior that
depends on this runtime layer.

## Compatibility Status

This feature is internal infrastructure. It does not change `COMPATIBILITY.md`
support marks. Future SQL features may reference this spec as their runtime
foundation.

## Follow-On Work

After this runtime slice is implemented and verified, the next baseline slices
should proceed in this order:

1. Add SQLite connection bootstrap policy on top of `mylite_db`: trusted-schema
   settings, foreign-key policy, callback client data, and the registration
   surface for built-in functions and collations. This may stay minimal until
   the first SQL feature needs real callbacks, but the setup belongs behind the
   runtime handle before file-backed opening and catalog work depend on it.
2. Add the `.mylite` storage open path and VFS proof for the 4096-byte preamble.
   The proof must cover ordinary file-backed opening, journal and WAL paths,
   backup, vacuum, truncate, mmap policy, and rejection by plain SQLite tools.
3. Add the catalog foundation: durable MyLite schemas, tables, columns, logical
   type descriptors, physical SQLite encodings, descriptor caching, and
   invalidation.
4. Add the first basic table lifecycle slice, starting with `CREATE TABLE` and
   `DROP TABLE`, plus planned result builders needed to inspect the result such
   as `SHOW TABLES` or `DESCRIBE`.
5. Add the first descriptor-aware write-boundary conversion proof and the first
   MySQL 8.4.9 comparison fixture for user-visible SQL behavior.

## Implementation Notes

- Keep public headers C++ compatible.
- Public status constants must be stable once exposed.
- Do not leak SQLite headers through `mylite/mylite.h`.
- Keep SQLite-specific details behind internal runtime modules.
- Keep parser and lexer modules independent from runtime modules.
- Use caller-before-callee ordering and public-before-private ordering in new C
  files.
- Add comments only for ownership, lifetime, and compatibility rationale.
