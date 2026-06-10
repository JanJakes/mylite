# Parser Corpus Admin Transaction Surfaces

This slice admits a small set of MySQL 8.4.9 parser-corpus statements that are
common in dump, administration, and transaction-control SQL without expanding
MyLite into a server administration engine.

## Sources

- MySQL 8.4 Reference Manual, `START TRANSACTION`, `COMMIT`, and `ROLLBACK`:
  <https://dev.mysql.com/doc/refman/8.4/en/commit.html>
- MySQL 8.4 Reference Manual, `RENAME TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/rename-table.html>
- MySQL 8.4 Reference Manual, `CREATE SERVER`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-server.html>
- MySQL 8.4 Reference Manual, `ALTER SERVER`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-server.html>
- MySQL 8.4 Reference Manual, `DROP SERVER`:
  <https://dev.mysql.com/doc/refman/8.4/en/drop-server.html>
- MySQL 8.4 Reference Manual, `ALTER DATABASE` / `ALTER SCHEMA`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-database.html>
- Runtime evidence:
  `packages/libmylite/tests/mysql_parser_corpus_admin_transaction_surfaces_expectations.sh`
  against MySQL 8.4.9.

## Scope

Implemented in this slice:

- `COMMIT` and `ROLLBACK` accept optional `WORK`, optional
  `AND [NO] CHAIN`, and optional `[NO] RELEASE` completion modifiers.
- `AND CHAIN` starts a new explicit MyLite user transaction after the current
  `COMMIT` or `ROLLBACK` finishes. The new transaction preserves the just-ended
  transaction isolation and access mode when an explicit transaction was active.
  When no explicit transaction was active, MyLite starts a new transaction using
  the current connection default or pending next-transaction characteristics.
- `RELEASE` and `NO RELEASE` are accepted. Because MyLite is embedded and does
  not own a client/server session to disconnect, `RELEASE` has no connection
  close side effect.
- `RENAME TABLES` is accepted as an alias for the existing limited
  multi-pair `RENAME TABLE` implementation.
- `CREATE SERVER`, `ALTER SERVER`, and `DROP SERVER` are admitted as
  server-only administrative no-ops with warning `1105`. MyLite keeps
  `mysql.servers` read-only and empty.
- `ALTER DATABASE` / `ALTER SCHEMA` forms containing `ENCRYPTION` or
  `READ ONLY [=] {DEFAULT|0|1}` are parsed and rejected as unsupported utility
  statements before catalog mutation.

Out of scope:

- `completion_type` system-variable defaults, protocol flags, and physical
  disconnection for `RELEASE`.
- Persisted FEDERATED server definitions, privilege checks, binary logging,
  implicit-commit emulation for server no-ops, or writable `mysql.servers`.
- Schema encryption state, schema read-only state, table-encryption inheritance,
  or `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` read-only mutation.
- New `RENAME TABLE` support for temporary tables, triggers, views beyond the
  current table-rename subset, metadata locks, privileges, or foreign-key
  rename side effects.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies:

- `COMMIT AND CHAIN` commits preceding work and makes following DML rollbackable
  by the next `ROLLBACK`.
- `ROLLBACK AND CHAIN` rolls back preceding work and makes following DML
  commit-capable in the next transaction.
- `COMMIT AND CHAIN` with no active explicit transaction still starts a new
  transaction.
- `COMMIT WORK RELEASE`, `ROLLBACK NO RELEASE`, `COMMIT AND NO CHAIN
  NO RELEASE`, and `ROLLBACK WORK AND NO CHAIN RELEASE` are accepted.
- `RENAME TABLES a TO b, c TO d` is accepted.
- `CREATE SERVER`, `ALTER SERVER`, and `DROP SERVER` mutate `mysql.servers` in
  MySQL but are represented in MyLite as empty-table administrative no-ops.
- `ALTER SCHEMA ... ENCRYPTION = 'N'` and
  `ALTER SCHEMA ... READ ONLY DEFAULT` are valid MySQL syntax.

## Grammar

The MyLite grammar surface for this slice is:

```lemon
transaction_control_statement:
    COMMIT transaction_work_opt transaction_completion_opt
  | ROLLBACK transaction_work_opt transaction_completion_opt

transaction_work_opt:
    /* empty */
  | WORK

transaction_completion_opt:
    /* empty */
  | transaction_chain_completion
  | transaction_release_completion
  | transaction_chain_completion transaction_release_completion

transaction_chain_completion:
    AND CHAIN
  | AND NO CHAIN

transaction_release_completion:
    RELEASE
  | NO RELEASE

rename_table_statement:
    RENAME TABLE rename_table_pair_list
  | RENAME TABLES rename_table_pair_list
```

Server DDL and unsupported schema options are intentionally classified through
the parser placeholder scanner rather than a full grammar because this slice
does not implement their semantic bodies. The scanner only admits recognizable
complete forms:

- `CREATE SERVER ... FOREIGN DATA WRAPPER ... OPTIONS (...)`
- `ALTER SERVER ... OPTIONS (...)`
- `DROP SERVER name`
- `DROP SERVER IF EXISTS name`
- `ALTER {DATABASE|SCHEMA} ... ENCRYPTION ...`
- `ALTER {DATABASE|SCHEMA} ... READ ONLY [=] {DEFAULT|0|1}`

## Runtime Semantics

`AND CHAIN` reuses the existing MyLite explicit-user-transaction path. A
successful commit first reconciles persistent auto-increment high-water state,
commits the SQLite transaction, clears user-visible savepoints, then starts a
new `BEGIN IMMEDIATE` transaction. A successful rollback rolls back the SQLite
transaction, reconciles temporary physical tables and persistent
auto-increment state, clears savepoints, then starts the next transaction.

The chained transaction stores active isolation and read-only access state in
connection-local session state. This is wrapper/runtime state, not a SQLite
fork hook. No SQLite fork patch is needed because SQLite already exposes the
required transaction primitives.

Foreign-server no-ops use the existing administrative no-op warning path. They
do not mutate `mysql.servers`, do not create physical storage, and do not
implement MySQL's implicit commit for these server-only statements.

Schema encryption/read-only forms use the existing unsupported-utility
diagnostic path. They are parsed so applications see a deterministic embedded
compatibility error rather than a generic syntax gap.

## Tests

- `parser_corpus_admin_transaction_surfaces_test.c` covers parser shape,
  chain-marker AST nodes, no-op/unsupported placeholder classification, plural
  `RENAME TABLES`, and malformed tails.
- `runtime_parser_corpus_admin_transaction_surfaces_test.c` covers transaction
  chaining side effects, `RENAME TABLES` execution, foreign-server no-op
  warnings and empty `mysql.servers`, and schema unsupported diagnostics.
- `mysql_parser_corpus_admin_transaction_surfaces_expectations.sh` records the
  MySQL 8.4.9 evidence used by these expectations.

## Performance And Storage

The parser additions are fixed grammar alternatives plus small placeholder
classification checks. Runtime chaining adds at most one extra `BEGIN
IMMEDIATE` after a successful `COMMIT` or `ROLLBACK AND CHAIN`. No additional
catalog tables, file-format changes, dependencies, or SQLite fork hooks are
introduced.
