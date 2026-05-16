# Baseline Key-Aware ALTER DROP COLUMN Tasks

## Goal

Extend `ALTER TABLE ... DROP [COLUMN]` so descriptor-owned keys and
auto-increment metadata remain coherent when a key column is dropped, while
foreign-key column dependencies fail with MySQL-compatible diagnostics.

## Tasks

1. Design and MySQL evidence
   - Add `docs/specs/baseline-key-aware-alter-drop-column/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Capture reproducible MySQL expectation probes for primary-key,
     secondary-key, prefix-key, fulltext, auto-increment, duplicate narrowed
     key, and foreign-key dependency cases.

2. Runtime planning
   - Load target-table columns, key descriptors, child foreign keys, and parent
     foreign keys during drop-column planning.
   - Reject supported child and parent foreign-key column dependencies with
     MySQL-compatible `1828` / `1829` diagnostics.
   - Build a key-update plan: delete one-part affected keys and shrink
     composite affected keys.
   - Validate narrowed unique and primary keys for duplicate remaining tuples
     before mutation.
   - Determine MySQL-compatible affected rows for dropped auto-increment
     columns and removed one-column primary keys.

3. Catalog and physical mutation
   - Add catalog support for deleting one key part from an existing index and
     compacting remaining key-part ordinals.
   - Reuse existing index deletion for one-part dropped keys.
   - Drop affected physical indexes before SQLite `DROP COLUMN`.
   - Recreate surviving affected non-fulltext physical indexes from
     descriptor key parts after SQLite `DROP COLUMN`.
   - Keep metadata-only fulltext physical behavior unchanged.
   - Keep catalog and physical changes inside one rollback-safe mutation.

4. Tests
   - Extend the existing drop-column runtime test with key-aware cases.
   - Cover one-part secondary, unique, prefix, fulltext, primary, and
     auto-increment drops.
   - Cover composite primary, unique, nonunique, and prefix key shrinkage.
   - Cover duplicate narrowed primary/unique diagnostics.
   - Cover child and parent foreign-key dependency diagnostics.
   - Verify row readback, DML, `SHOW CREATE TABLE`, `SHOW INDEX`,
     `INFORMATION_SCHEMA.STATISTICS`, affected rows, warning count, reopen
     persistence, physical index count, and independent handles.

5. Documentation
   - Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-ddl.md`, and the
     earlier drop-column spec only for the exact expanded subset.
   - Do not claim general `ALTER TABLE`, multi-action drops, algorithms, locks,
     CHECK dependency rewrites, generated columns, views, triggers, broader
     foreign-key behavior, or optimizer parity.

6. Verification
   - Run `cmake --build --preset dev`.
   - Run the MySQL expectation script.
   - Run focused CTest entries for parser, drop-column, key/index, fulltext,
     foreign-key, and auto-increment lifecycles.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for descriptor authority, duplicate validation,
     physical SQLite safety, `.mylite` preamble preservation, public ABI
     stability, scope control, compatibility accuracy, and test relevance.
