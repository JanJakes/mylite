# SQLite Connection Bootstrap Policy Tasks

## Goal

Factor SQLite bootstrap behind `mylite_open_memory()` and add internal policy,
registration, callback client-data, and hook scaffolding without exposing SQLite
or adding user-visible SQL behavior.

## Tasks

1. Design and documentation
   - Create `docs/specs/sqlite-connection-bootstrap-policy/specs.md`.
   - Explain why this internal slice does not require MySQL runtime probes.
   - Keep `COMPATIBILITY.md` unchanged.

2. Bootstrap module
   - Add a private runtime bootstrap module.
   - Store connection-local bootstrap state in `mylite_db`.
   - Make bootstrap and deinit tolerate zero-initialized objects.
   - Call the shared bootstrap path from `mylite_open_memory()`.

3. Connection policy
   - Disable SQLite trusted schema for every new handle.
   - Apply the current foreign-key placeholder policy consistently.
   - Record observed policy state per handle for internal tests and future
     diagnostics.

4. Callback client data
   - Attach the owning `mylite_db` to each SQLite connection through a private
     client-data key.
   - Add internal owner lookup helpers for `sqlite3 *` and
     `sqlite3_context *`.
   - Keep the owner pointer borrowed.

5. Function and collation registration surface
   - Add internal function descriptors for scalar, aggregate, and window
     registrations.
   - Add internal collation descriptors.
   - Validate descriptors and map SQLite failures to MyLite status codes.
   - Bootstrap empty MyLite-owned registration lists only; do not implement
     MySQL functions or collations.

6. Hook surface
   - Add connection-local hook-registration state.
   - Provide a deinit path that clears currently supported hook slots safely.
   - Leave real hook behavior for future slices.

7. Runtime tests
   - Add a fast C test under `packages/libmylite/tests/`.
   - Register it with a dotted CTest name.
   - Cover policy state on independent handles.
   - Cover callback owner lookup from a test-only scalar callback.
   - Cover registration success and misuse paths.
   - Cover zero-initialized bootstrap cleanup.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry.
   - Run `cmake --workflow --preset check`.
   - Review the diff for architecture boundaries, public ABI exposure,
     zero-init safety, and scope control.

## Out Of Scope

- SQL execution through MyLite public APIs.
- File-backed opening.
- Catalog descriptors.
- MySQL function or collation semantics.
- MySQL runtime comparison fixtures.
- SQLite fork patches.
- Compatibility-matrix status changes.
