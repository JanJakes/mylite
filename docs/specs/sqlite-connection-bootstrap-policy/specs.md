# SQLite Connection Bootstrap Policy

## Status

This feature specifies the next internal runtime slice after
`runtime-handles-statement-context`. It factors SQLite connection bootstrap out
of `mylite_open_memory()` and establishes the MyLite-owned policy and callback
registration surface that future SQL execution, functions, collations,
diagnostics, catalog invalidation, and file-backed open modes will share.

This is internal compatibility infrastructure. It does not implement
user-visible SQL behavior, does not expose SQLite through the public ABI, and
does not move any `COMPATIBILITY.md` row out of unsupported status.

## Sources

- MyLite baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement-context spec:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Runtime handles and statement-context tasks:
  `docs/specs/runtime-handles-statement-context/tasks.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- SQLite source snapshot notes:
  `third_party/sqlite/README.md`
- Bundled SQLite header:
  `third_party/sqlite/amalgamation/sqlite3.h`
- SQLite application-defined functions:
  https://www.sqlite.org/appfunc.html
- SQLite application-defined collations:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite database connection configuration:
  https://www.sqlite.org/c3ref/c_dbconfig_defensive.html
- SQLite connection client data:
  https://www.sqlite.org/c3ref/get_clientdata.html
- SQLite tracing and hooks:
  https://www.sqlite.org/c3ref/trace_v2.html,
  https://www.sqlite.org/c3ref/update_hook.html

This specification is independently authored from project documentation and
public SQLite documentation. It does not copy MySQL, MariaDB, Percona, TiDB, or
other restrictively licensed implementation sources.

## Scope

The implementation must add:

- a shared internal bootstrap path for every SQLite handle opened for
  `mylite_db`, initially used by `mylite_open_memory()`;
- connection-local bootstrap state stored inside `mylite_db`;
- trusted-schema policy applied to every new SQLite handle;
- an explicit foreign-key policy placeholder applied consistently to every new
  SQLite handle;
- SQLite connection client data that lets callbacks recover the owning
  `mylite_db`;
- internal registration helpers for MyLite-owned scalar, aggregate, and window
  functions;
- internal registration helpers for MyLite-owned collations;
- a named hook-registration surface for future busy, progress, trace, update,
  commit, and rollback callbacks;
- tests proving policy state, independent handles, callback owner lookup, and
  registration error paths.

The bootstrap layer is private to `packages/libmylite/src/runtime/`. Public
headers must continue to expose only MyLite types and constants.

## Non-Goals

This feature must not implement:

- SQL execution through the public MyLite API;
- file-backed ordinary SQLite opening or `.mylite` preamble-aware opening;
- catalog tables, descriptor storage, or `INFORMATION_SCHEMA`;
- MySQL scalar, aggregate, window, or collation semantics;
- MySQL runtime comparison fixtures;
- SQLite fork patches;
- compatibility-matrix status changes.

## Compatibility Notes

No MySQL runtime probe is attached to this feature because the slice has no
user-visible SQL surface. The behavior being tested is MyLite internal runtime
ownership of SQLite handles and callback plumbing. MySQL comparison fixtures
must start with the first user-visible SQL behavior that depends on this
bootstrap layer.

`COMPATIBILITY.md` remains unchanged. Future SQL features may reference this
spec as their SQLite bootstrap foundation, but each supported MySQL behavior
still needs its own MySQL 8.4.9 runtime-verified expectations.

## Bootstrap Architecture

`mylite_open_memory()` should allocate and initialize `mylite_db`, open the
private SQLite `:memory:` connection, and then call a shared bootstrap function
before publishing the handle to the caller. Future open modes should call the
same bootstrap function after opening their SQLite connection and before any
SQLite SQL, catalog access, virtual table registration, or prepared statement
work.

The bootstrap function owns this sequence:

1. attach the owning `mylite_db` as named SQLite connection client data;
2. apply connection policy;
3. initialize the MyLite function registration surface;
4. initialize the MyLite collation registration surface;
5. initialize the hook-registration surface.

If any bootstrap step fails, the open operation must close the SQLite handle,
deinitialize MyLite-owned runtime state, free `mylite_db`, return a MyLite
status code, and leave the output handle set to `NULL`.

Bootstrap deinitialization must tolerate zero-initialized state. `mylite_close()`
should run it before closing SQLite so callback pointers and client data are not
left live while the owner is being torn down.

## Connection Policy

The initial policy uses only SQLite public APIs.

### Trusted Schema

Every new SQLite handle must disable trusted schema using
`SQLITE_DBCONFIG_TRUSTED_SCHEMA`. This keeps schema-authored expressions from
calling non-innocuous application-defined functions once MyLite begins
registering callbacks. MyLite will later decide, per function and virtual table,
which callbacks can safely be tagged innocuous.

The resulting trusted-schema state must be recorded in connection-local
bootstrap state so tests and future diagnostics can confirm the policy applied
to each handle.

### Foreign Keys

Foreign-key enforcement remains an explicit placeholder in this slice. MyLite
will need catalog descriptors, MySQL constraint naming, action semantics,
diagnostics, and DDL integration before it can rely on SQLite foreign-key
enforcement as part of user-visible compatibility.

For now, the bootstrap should set SQLite foreign-key enforcement off through
`SQLITE_DBCONFIG_ENABLE_FKEY` and record both the observed state and the fact
that the policy is a placeholder. This prevents accidental SQLite-native
constraint behavior from becoming visible before MyLite owns the corresponding
MySQL semantics.

## Client Data And Callback Owner Lookup

The bootstrap must attach the owning `mylite_db` to the SQLite connection using
a private client-data key. MyLite callbacks should recover ownership through
internal helpers:

- owner from `sqlite3 *`;
- owner from `sqlite3_context *`.

The owner pointer is borrowed. SQLite must not own or free `mylite_db`; the
MyLite handle remains responsible for its lifetime. Bootstrap deinitialization
may clear the client-data slot before the SQLite connection closes.

Tests should prove callback lookup by registering an internal test-only scalar
function on a handle and confirming that the callback can recover the same
`mylite_db`.

## Function Registration Surface

Add an internal registration helper that accepts descriptors for:

- scalar functions;
- aggregate functions;
- window functions.

The descriptor should include the function name, arity, SQLite text
representation and flags, application data pointer, callback pointers, and
optional destructor. The helper should validate descriptors before calling
SQLite and should map SQLite failures to MyLite status codes.

This slice must not register real MySQL functions. It only creates the surface
and calls it with empty MyLite-owned registration lists during bootstrap. Tests
may register test-only callbacks through the same helper.

Future function descriptors must set SQLite flags deliberately:
deterministic only for statement-independent functions, direct-only for
callbacks that must not run from schema objects, innocuous only when safe under
disabled trusted schema, and subtype flags only when the callback actually uses
SQLite subtypes.

## Collation Registration Surface

Add an internal registration helper for MyLite-owned collations. The descriptor
should include the collation name, SQLite encoding, application data pointer,
comparison callback, and optional destructor.

This slice must not implement MySQL collation semantics or register baseline
MySQL collation names. Tests may register a test-only collation through the same
helper to prove the surface works and reports misuse cleanly.

## Hook Registration Surface

The bootstrap must provide a clear internal surface for future SQLite callbacks:

- busy handler;
- progress handler;
- trace callback;
- update hook;
- commit hook;
- rollback hook.

This feature does not need hook behavior yet. The surface should initialize
connection-local hook state and deinitialize safely. Future slices can add real
hook registration without changing `mylite_open_memory()` or exposing SQLite
publicly.

## Parser And Grammar

No MyLite SQL syntax is added. No Lemon grammar snippets apply to this feature.

## Runtime Behavior

After successful `mylite_open_memory()`:

- the returned handle owns a private usable SQLite in-memory connection;
- the connection has bootstrap state marked initialized;
- trusted schema is disabled on that SQLite connection;
- SQLite foreign-key enforcement is disabled and marked as placeholder policy;
- callback client data points back to the owning `mylite_db`;
- MyLite function, collation, and hook registration surfaces are initialized;
- diagnostics, session state, statement context, and planned-result metadata
  behavior from the previous slice are preserved.

Independent `mylite_db` handles must have independent SQLite connections,
policy state, client-data slots, and registration state.

## Error Handling

Internal bootstrap and registration functions return MyLite status codes:

- `MYLITE_OK` on success;
- `MYLITE_MISUSE` for invalid internal arguments or invalid descriptors;
- `MYLITE_NOMEM` when SQLite or MyLite allocation reports memory exhaustion;
- `MYLITE_ERROR` for other SQLite bootstrap or registration failures.

`mylite_open_memory()` must preserve its existing public behavior: invalid
output pointers return `MYLITE_MISUSE`, failures leave the output as `NULL`,
and successful calls return `MYLITE_OK`.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.sqlite_bootstrap`.

Coverage must include:

- bootstrap state exists on every `mylite_open_memory()` handle;
- trusted-schema and foreign-key policy are applied on independent handles;
- policy state is connection-local;
- callback owner lookup works from an internal smoke scalar function;
- function registration accepts zero-count lists and rejects invalid
  descriptors;
- collation registration accepts zero-count lists, can register a test-only
  collation, and rejects invalid descriptors;
- bootstrap deinitialization tolerates zero-initialized state;
- existing runtime handle, diagnostics, statement context, and planned-result
  metadata tests continue to pass.

## Build Integration

Add new runtime sources and the new test executable to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must continue to apply to new MyLite sources and tests. Vendored SQLite warning
policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\\.runtime\\.sqlite_bootstrap$' --output-on-failure
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, scope control, public
API exposure, zero-initialized cleanup, and test relevance.
