# Runtime Handles And Statement Context Tasks

## Goal

Implement the first runtime ownership slice without adding user-visible SQL
compatibility. The target proof is a MyLite-owned in-memory SQLite connection,
connection-local diagnostics/session state, statement-context lifecycle, and
planned-result metadata scaffolding.

## Tasks

1. Public API and headers
   - Add opaque `mylite_db` declaration to `mylite/mylite.h`.
   - Add stable public status constants for `MYLITE_OK`, `MYLITE_ERROR`,
     `MYLITE_MISUSE`, and `MYLITE_NOMEM`.
   - Add `mylite_open_memory()`, `mylite_close()`, `mylite_errcode()`,
     `mylite_sqlstate()`, and `mylite_errmsg()`.
   - Keep SQLite headers out of the public MyLite API.

2. Runtime connection module
   - Add `packages/libmylite/src/runtime/mylite_connection.*`.
   - Define `struct mylite_db` internally.
   - Own a private `sqlite3 *` per handle.
   - Initialize placeholder session state: selected schema, identity, SQL modes,
     time zone, character-set state, and catalog generation counters.
   - Add internal accessors needed by tests without making SQLite part of the
     public ABI.

3. Diagnostics module
   - Add `mylite_diagnostics.*`.
   - Store current error code, SQLSTATE, primary message, warning count, and an
     ordered internal warning list.
   - Add reset, set-error, append-warning, and read helpers.
   - Ensure `NULL` public diagnostics accessors return stable misuse
     diagnostics.

4. Statement context module
   - Add `mylite_statement_context.*`.
   - Add begin/end/deinit helpers for one top-level statement.
   - Reset diagnostics and warnings at begin.
   - Stage statement time, affected rows, previous row count, first insert id,
     wrapper transaction state, and backend status.
   - Make deinit tolerate zero-initialized objects.

5. Planned result metadata module
   - Add `mylite_result_metadata.*`.
   - Represent MySQL-visible column label, origin names, logical type, flags,
     charset/collation ids, length, decimals, and nullability.
   - Add init/append/deinit helpers.
   - Keep row storage out of this slice.

6. Runtime tests
   - Add `mylite_runtime_test.c` or focused smaller test executables under
     `packages/libmylite/tests/`.
   - Cover open/close, invalid arguments, independent handles, close `NULL`,
     diagnostics lifecycle, statement context lifecycle, planned-result metadata
     lifecycle, and internal SQLite ownership.
   - Register tests with CTest using dotted names.

7. Build integration
   - Add new runtime sources to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification
   - Run `cmake --build --preset dev`.
   - Run the new runtime CTest entries.
   - Run `cmake --workflow --preset check` before marking the slice complete.

## First Vertical Proof Checklist

- `mylite_open_memory()` returns a non-NULL handle and `MYLITE_OK`.
- Each opened handle owns a distinct usable SQLite in-memory connection.
- `mylite_close(NULL)` is a no-op.
- `mylite_open_memory(NULL)` returns `MYLITE_MISUSE`.
- Public diagnostic accessors work on a live handle and on `NULL`.
- Starting a statement resets previous diagnostics and warnings.
- Ending a statement keeps completion diagnostics readable until the next
  statement starts.
- A second statement boundary resets the first statement's diagnostics.
- Planned-result metadata can describe a synthetic one-column result and clean
  up without leaks.
- Zero-initialized internal objects can be deinitialized safely.

## Next Slice Handoff

Once this task list is complete, continue with:

1. SQLite connection bootstrap and policy registration on `mylite_db`.
2. File-backed `.mylite` opening with the 4096-byte preamble VFS proof.
3. Catalog descriptor storage and cache invalidation.
4. Basic table lifecycle DDL and catalog-backed planned result builders.
5. Descriptor-aware assignment conversion and the first MySQL 8.4.9 comparison
   fixture for user-visible SQL behavior.

## Out Of Scope

- SQL execution.
- MySQL runtime comparison fixtures.
- File-backed `.mylite` opening.
- SQLite fork patches.
- Function/collation registration beyond optional no-op bootstrap plumbing.
- Catalog tables and descriptors.
