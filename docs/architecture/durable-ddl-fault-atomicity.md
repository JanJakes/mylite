# Durable DDL Fault Atomicity

## Contract

Persistent MyLite DDL and catalog migration are durable SQLite writer
transactions. If an operating-system I/O operation fails, a later open may
observe either:

- the complete state before the operation; or
- the complete state after the operation reached durable commit.

A mixed state is corruption. Catalog descriptors, physical SQLite objects,
catalog generation, user rows, and the integrity seal must describe the same
side of the operation. Reopen must complete ordinary SQLite hot-journal
recovery before MyLite validates the catalog.

An I/O error reported near commit does not by itself identify which durable
state won. Tests therefore accept both complete outcomes, but never weaken the
structural invariants to require a particular return code or assume that an
error implies rollback.

## Covered Operations

The deterministic storage matrix covers:

- table creation and deletion;
- a physical-rebuild `ALTER TABLE ... MODIFY COLUMN`;
- table rename;
- secondary-index creation and deletion;
- `TRUNCATE TABLE`;
- N-1 catalog migration to the current catalog schema;
- VFS write and sync failures for every statement and migration case; and
- VFS truncate, delete, and close failures on representative physical-rebuild
  and migration paths that exercise those callbacks;
- process death at every measured migration write, sync, and truncate callback.

Each statement case contains data that distinguishes the pre-state from the
post-state. Rebuilding and rename cases must preserve committed rows in either
valid outcome. Index cases require matching catalog and physical index
presence. Truncate requires either all original rows or no rows.

## Deterministic Failpoint Protocol

For every statement/failpoint pairing selected by that matrix:

1. create an isolated committed pre-state;
2. run the operation once with a non-triggering failpoint to count matching VFS
   calls;
3. recreate the pre-state for every call index;
4. fail exactly that call;
5. clear the failpoint and close or abandon the affected handle according to
   the public ownership contract;
6. reopen normally and classify the complete state as pre or post; and
7. validate all common recovery invariants.

The failpoint is thread-local so concurrent tests and unrelated handles cannot
consume another case's call index. A case that claims coverage for a VFS
operation must prove that at least one matching call occurred.

Catalog migration uses the same protocol around database open. Before normal
reopen completes the migration, exact-handle raw inspection must find either
the complete N-1 schema or the complete current schema, with no migration
scratch table or partial integrity-seal surface.

The process-death matrix first measures every matching VFS call, recreates the
N-1 source for each call index, and terminates a child process at that exact
callback. Truncate cases use a test-scoped truncate journal mode during the
migration open so the claimed callback is proven reachable. The parent applies
ordinary hot-journal recovery and the same raw pre/post classification before
allowing MyLite to converge the catalog.

## Recovery Invariants

Every recovered current-format database must satisfy:

- `PRAGMA integrity_check` returns `ok`;
- full MyLite open-time catalog/physical validation succeeds;
- the catalog integrity generation equals the catalog generation;
- the sealed SQLite schema version equals the current SQLite schema version;
- catalog and physical table/index presence agree;
- no persistent physical object uses a rebuild or migration scratch name;
- no rollback journal remains after successful reopen; and
- a second reopen produces the same classified state.

Close failure is tested through checked close. If checked close reports an
error, the caller clears the one-shot fault and retries close as required by the
public ownership contract before inspecting the file.

## Platform Coverage

The matrix is platform-neutral. Process-death initialization, migration, and
hot-journal children use `fork` on POSIX and spawned child modes on Windows.
Release qualification must execute both platform paths.
