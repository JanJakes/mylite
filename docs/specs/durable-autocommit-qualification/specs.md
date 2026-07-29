# Durable Autocommit Qualification

## Status

Specified; implementation pending.

## Summary

MyLite's existing write benchmarks either use rollback transactions, batch
large seed phases into one transaction, or report a mixed request whose durable
commit cost is not isolated. This phase adds a reproducible end-to-end
qualification for durable commits across MyLite, the bundled SQLite baseline,
and MySQL 8.4.9.

The qualification measures one, four, and one hundred row writes per
transaction for both repeated single-row statements and one multi-row
statement. It reports transaction latency percentiles, throughput, sync-system
call counts, journal activity, engine configuration, and storage-environment
metadata on block-backed ext4 and XFS.

Durability is a correctness constraint. The benchmark must fail when an engine
is configured below the required durability level; it must never improve a
result by weakening synchronization, journaling, binary logging, or
doublewrite protection.

## Sources and Evidence

- Existing MyLite performance evidence:
  - `docs/performance/benchmarks.md`
  - `docs/performance/review-2026-07.md`
  - `docs/performance/large-dataset-extended-qualification-2026-07.md`
- Existing MyLite storage and recovery design:
  - `docs/architecture/embedding-and-abi.md`
  - `docs/architecture/durable-ddl-fault-atomicity.md`
  - `packages/libmylite/src/storage/mylite_file_open.c`
- Official SQLite documentation:
  - rollback-journal atomic commit:
    <https://sqlite.org/atomiccommit.html>
  - `journal_mode` and `synchronous` pragmas:
    <https://sqlite.org/pragma.html#pragma_journal_mode>
    and <https://sqlite.org/pragma.html#pragma_synchronous>
- Official MySQL 8.4 documentation:
  - InnoDB log buffer and commit flushing:
    <https://dev.mysql.com/doc/refman/8.4/en/innodb-redo-log-buffer.html>
  - InnoDB startup and flush variables:
    <https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html>
  - InnoDB disk I/O and doublewrite:
    <https://dev.mysql.com/doc/refman/8.4/en/innodb-disk-io.html>

The local pinned MySQL 8.4.9 runtime was observed with binary logging enabled,
`innodb_flush_log_at_trx_commit=1`, `sync_binlog=1`,
`innodb_doublewrite=ON`, `innodb_flush_method=O_DIRECT`, and
`innodb_use_fdatasync=ON`. The benchmark records and validates the live values
rather than assuming these defaults.

This specification is independently authored from official documentation,
observed MySQL 8.4.9 behavior, existing MyLite documentation, and existing
MyLite code. It does not copy implementation sources from MySQL, MariaDB,
Percona, SQLite, or another database.

## Durability Contract

### MyLite

File-backed MyLite opens must explicitly configure:

- rollback `journal_mode=DELETE`;
- `synchronous=EXTRA`;
- memory mapping disabled, as required by the offset-VFS design.

`EXTRA` retains SQLite's `FULL` rollback-journal and database-file
synchronization and additionally synchronizes the containing directory after
unlinking the rollback journal. This is stricter than the current inherited
`FULL` default and closes the documented power-loss window around journal
deletion. The setting is part of real file-backed operation, not a
benchmark-only override.

Memory databases are outside this qualification. The existing test-only
`TRUNCATE` journal mode is also outside it.

### Bundled SQLite baseline

The SQLite comparator uses the exact bundled SQLite target and explicitly
sets:

- `journal_mode=DELETE`;
- `synchronous=EXTRA`;
- `mmap_size=0`.

The benchmark reads every pragma back and rejects a mismatch before measuring.
It uses the ordinary SQLite VFS because the comparator is a conventional
single SQLite file rather than a `.mylite` container.

### MySQL 8.4.9

The MySQL comparator requires InnoDB and validates:

- `innodb_flush_log_at_trx_commit=1`;
- `log_bin=ON` and `sync_binlog=1`;
- `innodb_doublewrite=ON`;
- a durable supported `innodb_flush_method`;
- the live `innodb_use_fdatasync` value, recorded with the results.

The qualification records other relevant variables, including server version,
transaction isolation, page size, binary-log format, redo-log capacity, and
table-per-file policy. It does not disable binary logging, doublewrite, or
flush-at-commit to make the server resemble SQLite internally. The comparison
matches the user-visible durability promise, while retaining each engine's
production recovery mechanism.

## Workload

Each engine uses a fresh database containing one table with:

- a signed integer primary key;
- one non-null integer payload;
- no secondary index, trigger, foreign key, generated column, or default
  expression.

The measured matrix is:

| Writes per transaction | Repeated statement shape | Multi-row statement shape |
| ---: | --- | --- |
| 1 | one autocommit single-row insert | one autocommit one-row insert |
| 4 | four retained single-row inserts in one explicit transaction | one four-row insert in one explicit transaction |
| 100 | one hundred retained single-row inserts in one explicit transaction | one one-hundred-row insert in one explicit transaction |

The one-write case does not issue a redundant explicit `BEGIN` or `COMMIT`;
the successful statement is the autocommit transaction. Grouped cases measure
transaction begin, all row writes, and commit as one latency observation.

Prepared statements are created before timing and reused. Every measured
transaction inserts new primary keys. Setup, DDL, prepare, warmup, table reset,
verification, connection establishment, and teardown are excluded from the
transaction timers.

Each scenario defaults to seven independent samples of at least one thousand
measured transactions after warmup. A smoke mode may reduce this for tool
tests, but smoke results are not qualification evidence. Each sample verifies
the exact expected row count and payload checksum.

## Measurement and Output

The benchmark writes one raw record per measured transaction containing:

- revision, engine, filesystem, statement shape, writes per transaction,
  sample, and transaction index;
- elapsed monotonic nanoseconds;
- affected rows and cumulative committed rows;
- success or exact failure context.

For each engine, filesystem, statement shape, and transaction size, the
summary reports:

- transaction p50, p95, and p99 using sorted nearest-rank observations;
- minimum, maximum, median absolute deviation, and sample count;
- transactions per second and committed row writes per second;
- total and per-transaction `fsync`, `fdatasync`, `syncfs`, and `msync`
  counts;
- rollback-journal create, truncate, rename, and unlink activity for MyLite
  and SQLite;
- MySQL data, redo, and binary-log synchronization observations, including
  server-process syscall counts and before/after status counters.

Sync syscall evidence must trace the process that performs the I/O. Tracing
only a MySQL client is invalid; MySQL evidence traces the server process over
the exact workload window. Startup, shutdown, idle background work, setup, and
verification are excluded or reported separately.

Raw transactions, raw syscall traces, parsed syscall summaries, engine
configuration, environment metadata, and the generated summary are separate
artifacts. The summarizer fails on missing scenarios, invalid configuration,
too few qualification transactions, unsuccessful writes, row-count/checksum
mismatch, missing server-side MySQL sync evidence, or mixed filesystem
identity.

## Storage Environment

Qualified runs use a real block-backed filesystem. For each ext4 and XFS run,
the harness records:

- kernel, architecture, CPU model and count, memory, compiler, PHP/client, and
  engine versions;
- mount source, filesystem type, mount options, free space, and database path;
- block-device name, model, size, logical/physical sector sizes, rotational
  flag, scheduler, write-cache mode, discard properties, and device stack;
- container or virtualization identity and MySQL data-directory mount;
- git revision, dirty-worktree state, benchmark arguments, wall-clock start,
  locale, timezone, and relevant environment variables.

tmpfs, overlayfs, network filesystems, loop files backed by another filesystem,
and an XFS filesystem layered over an ext4 loop file may be useful smoke
environments, but they are not ext4/XFS qualification evidence. ext4 and XFS
results must come from block-backed mounts whose device stack is reported.

The engines for one filesystem run must place their durable data on that same
filesystem class. MySQL may run in a container only when its data directory is
a bind mount on the qualified filesystem and the container/storage mapping is
recorded.

## Diagnostics and Failure Handling

- A durability-setting mismatch fails before warmup.
- A transaction error aborts the scenario and records engine-native code,
  state, and message without continuing with a partial sample.
- A grouped transaction failure is rolled back before cleanup.
- Existing benchmark databases are never overwritten unless they are inside
  the explicitly supplied output directory for the current run.
- Temporary paths include the engine, scenario, process id, and a generated
  run identifier.
- Cleanup removes only the resolved paths created by the current invocation.
- Credentials are accepted through environment variables and are never
  written to metadata, raw samples, process arguments, or trace artifacts.

## Architecture and Ownership

- Product storage policy owns the MyLite `synchronous=EXTRA` setting.
- A focused benchmark client owns workload construction, timing, correctness
  checks, and engine configuration validation.
- A shell orchestrator owns scenario enumeration, process/server tracing, and
  environment capture.
- A deterministic summarizer owns schema validation, percentile and
  throughput calculation, and matrix-completeness checks.
- Generated benchmark data and raw evidence live below the selected build or
  artifact directory and are not public ABI or `.mylite` format inputs.

The implementation adds no shipping dependency. Optional benchmark tooling may
use PHP/PDO, `strace`, standard Linux storage utilities, and a pinned MySQL
8.4.9 runtime. The shipped MyLite library continues to depend only on its
bundled SQLite foundation and platform C runtime.

## Test and Qualification Plan

Before performance evidence is accepted:

1. Add native coverage that file-backed MyLite reports `DELETE` and `EXTRA`
   after open, while the test-only `TRUNCATE` path remains controlled.
2. Add smoke/tool tests for option validation, raw record shape, percentile
   calculation, completeness enforcement, configuration rejection, cleanup,
   and credential redaction.
3. Run the complete native suite, sanitizer coverage for the storage change,
   formatting, static analysis, ABI, install, compatibility-ledger, and
   production-size gates.
4. Run the complete matrix on block-backed ext4 and XFS with server-side MySQL
   syscall tracing and correctness verification.
5. Publish raw evidence and a concise qualification report. Explain residual
   differences in terms of observed transaction work and synchronization; do
   not optimize production behavior until evidence identifies a dominant
   avoidable MyLite cost.
