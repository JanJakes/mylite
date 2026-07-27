# Catalog Migrations

MyLite catalog upgrades converge under concurrent opens and publish only a
complete current-format catalog.

## Transaction Contract

An opener may inspect the catalog version before migration to select the open
path, but that observation does not authorize a migration step. The complete
migration chain follows this protocol:

1. acquire one SQLite `BEGIN IMMEDIATE` transaction;
2. reread the durable catalog state under that writer lock;
3. apply every required version step in order;
4. validate the final current-format catalog and publish its integrity seal;
5. commit once.

Each version step uses a private savepoint. A step failure first rolls back its
savepoint and then rolls back the outer migration transaction. No intermediate
catalog version becomes durable.

Concurrent openers can observe the same old version before either has the
writer lock. One opener wins the lock and performs the chain. Other openers wait
under the normal SQLite busy policy, acquire the lock in turn, reread the
winner's durable current version, and commit an empty migration transaction.
They do not replay an already completed step.

## Failure Behavior

A migration failure before outer commit leaves the exact pre-migration catalog
and physical SQLite schema. An I/O failure at the commit boundary may leave
either that complete pre-state or the complete committed post-state; the
[durable DDL fault-atomicity contract](durable-ddl-fault-atomicity.md) defines
the required recovery checks. Opening reports the original migration error.
Recovery never treats an intermediate schema version as current, and integrity
sealing occurs only after the whole chain succeeds.

Process death at any migration write, sync, or truncate boundary follows the
same pre/post rule. A later opener first performs SQLite hot-journal recovery,
then migrates an intact old state or validates an intact current state. Tests
terminate a child at every measured callback and require preserved user rows,
no migration scratch objects, a valid final seal, and two stable reopens.

Thread and process regression tests synchronize immediately after the old
version read and before writer-lock acquisition. Both openers must succeed,
observe the current version and minimum-reader version, agree on the published
integrity generation, pass SQLite integrity checking, and permit a clean
subsequent reopen.
