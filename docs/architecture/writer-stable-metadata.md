# Writer-Stable Metadata

Persistent statements may analyze catalog descriptors before SQLite acquires a
writer lock. Another connection can commit DDL between those events. MyLite
must never execute or publish a write derived from that stale analysis.

## Contract

Metadata-dependent writes establish a writer-stable SQLite transaction before
analysis. Each statement execution attempt also records the catalog generation
visible after statement-entry synchronization. Any nested path that can make a
persistent write validates that generation before performing a physical or
catalog mutation.

- Autocommit statements use an outer `BEGIN IMMEDIATE`; statement transactions
  and catalog mutations use savepoints inside it.
- Explicit and autocommit-disabled transactions already provide the stable
  snapshot and use their normal statement savepoints.
- Statement transactions validate after entering the stable transaction.
- Catalog mutations validate after their durable catalog state read.
- A generation mismatch is an internal retry condition, not a client-visible
  error.
- A generation mismatch before mutation discards partial results and
  diagnostics, synchronizes the new catalog snapshot, and analyzes the
  original AST again. The outer writer lock makes this a defense-in-depth path
  rather than the normal autocommit path.
- Prepared DML plans remain reusable only while their analysis key matches the
  current catalog and SQLite schema generations. A race after that check is
  caught by the same writer-lock validation.
- Validation happens before physical writes, catalog writes, auto-increment
  reservation, foreign-key actions, or integrity-seal publication.

Generation retry remains defense in depth for nested execution and any future
write path that reaches a lock without the outer planning transaction. No stale
attempt can commit or publish an integrity seal.

## Structural Validation

Catalog mutations nested inside a structural statement update the durable
generation but defer integrity-seal publication. After the complete statement
has assembled its physical and catalog state, MyLite validates catalog
relationships and physical object correspondence, publishes one seal for the
final SQLite schema cookie, and commits the outer transaction.

The immutable internal catalog table/index/trigger layout is validated on open.
Statement completion revalidates mutable descriptor relationships and physical
user objects, avoiding a full rescan of the unchanged internal catalog schema
after every DDL statement.

Views retain source schema and table names after a source rename or drop so
`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` remains a dependency snapshot. A dropped
source is represented by zero source IDs; positive IDs must resolve to a live
base table. This distinguishes a deliberately invalidated view from arbitrary
orphaned catalog IDs and remains valid after reopen.

Temporary-table operations use the same statement-entry generation guard.
They can be conservatively retried after unrelated persistent DDL, but they do
not publish persistent catalog state.
