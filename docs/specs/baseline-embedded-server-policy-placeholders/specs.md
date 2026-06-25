# Baseline Embedded Server Policy Placeholders

This slice reconciles MyLite's detailed embedded-design matrix with the
placeholder surfaces already implemented for server-only MySQL features, and
adds the remaining `LOGFILE GROUP` syntax to the tablespace utility no-op
contract.

MyLite is an embedded single-file database. Server features that require a
shared mysqld process, server-global state, external network listeners, native
plugins, physical binary logs, NDB Disk Data files, or live Performance Schema
instrumentation must not silently pretend those effects exist. The compatible
embedded baseline is one of:

- deterministic no-op success with warning `1105`;
- deterministic unsupported diagnostics for file or instrumentation operations
  that would otherwise imply visible data access;
- metadata-only synthetic rows where applications commonly probe server
  capabilities.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-logfile-group.html
- https://dev.mysql.com/doc/refman/8.4/en/alter-logfile-group.html
- https://dev.mysql.com/doc/refman/8.4/en/drop-logfile-group.html
- https://dev.mysql.com/doc/refman/8.4/en/performance-schema.html
- https://dev.mysql.com/doc/refman/8.4/en/sys-schema.html
- https://dev.mysql.com/doc/refman/8.4/en/x-plugin-options-system-variables.html
- https://dev.mysql.com/doc/refman/8.4/en/document-store.html

Existing MyLite specs remain the detailed authority for each surface, including
admin placeholders, utility placeholders, replication controls, built-in schema
write protection, sys view baselines, Performance Schema helper/status
placeholders, `LOAD DATA INFILE`, file-output diagnostics, and MySQLX status
placeholders.

## MySQL 8.4.9 Observations

Runtime probes were executed against the local `mysql:8.4.9` container
`mylite-mysql-849`.

```sql
CREATE LOGFILE GROUP mylite_lg
  ADD UNDOFILE 'mylite_undo.dat'
  INITIAL_SIZE=1M;

CREATE LOGFILE GROUP mylite_lg
  ADD UNDOFILE 'mylite_undo.dat'
  ENGINE=NDB;

ALTER LOGFILE GROUP mylite_lg
  ADD UNDOFILE 'mylite_undo2.dat'
  ENGINE=NDB;

DROP LOGFILE GROUP mylite_lg ENGINE=NDB;
```

Observed behavior on this non-NDB target:

- `CREATE LOGFILE GROUP ...` without `ENGINE=NDB` is accepted by the grammar
  and fails with `3658 / HY000`, reporting unsupported logfile-group storage
  for the default InnoDB engine.
- `CREATE`, `ALTER`, and `DROP LOGFILE GROUP ... ENGINE=NDB` are accepted by
  the grammar and fail with `1286 / 42000` because the target runtime has no
  NDB engine.
- These statements are server/NDB storage operations, not portable single-file
  operations.

## MyLite Scope

This slice covers:

- `CREATE LOGFILE GROUP ...`;
- `ALTER LOGFILE GROUP ...`;
- `DROP LOGFILE GROUP ...`;
- detailed embedded-design status rows for already-covered server-only policy
  surfaces.

Runtime behavior for logfile groups:

- parse as `utility_noop_statement`;
- return success with no columns and no rows;
- affected rows and `ROW_COUNT()` are `0`;
- append warning `1105 / HY000` with the existing utility no-op text;
- preserve user transactions;
- do not create NDB objects, files, tablespace descriptors, information-schema
  rows, or storage-engine state.

The detailed embedded-design matrix moves out of red for surfaces that already
have a documented baseline:

- replication and binary logs: limited SHOW metadata, fixed variables, and
  replication-control no-ops;
- account management and privileges: synthetic embedded identity/grants plus
  account/role/privilege no-op statements;
- resource groups: information-schema metadata placeholders and no-op DDL;
- server lifecycle commands: explicit no-op `RESTART` / `SHUTDOWN`;
- tablespaces and logfile groups: metadata placeholders and no-op DDL or
  unsupported file-operation diagnostics;
- Performance Schema: schema/catalog exposure plus selected status/helper
  placeholders, without live event tables;
- sys schema: selected queryable views/functions/procedures plus placeholders;
- file import/export: limited server-side `LOAD DATA INFILE`, disabled
  `LOCAL`, and diagnostics for export/XML/import file surfaces;
- X Protocol and Document Store: SQL/status placeholders only, no X listener
  or X DevAPI document-store implementation.

## Out Of Scope

This slice does not implement:

- physical NDB logfile groups, undo files, data files, or Disk Data metadata;
- live binary logs, relay logs, replication appliers, or group membership;
- persisted account, role, privilege, resource-group, component, plugin, or
  server-global state;
- live Performance Schema instrumentation or queryable event tables;
- a physical sys schema backed by stored view definitions;
- `SELECT ... INTO OUTFILE`, `SELECT ... INTO DUMPFILE`, `LOAD XML`, or
  `IMPORT TABLE` file I/O;
- X Protocol packets, X Plugin listeners, X DevAPI, or document collections.

## MyLite Grammar Snippets

The ordinary parser remains the authority for supported SQL. These snippets
describe the placeholder classifier behavior for logfile groups:

```text
utility_noop_statement:
    CREATE LOGFILE GROUP ...
  | ALTER LOGFILE GROUP ...
  | DROP LOGFILE GROUP ...
```

## Runtime Architecture

No SQLite extension API or fork hook is needed. Logfile groups are server/NDB
storage management statements with no MyLite single-file equivalent, so they
reuse the existing raw utility no-op AST and runtime executor.

The embedded-design documentation changes do not add runtime paths. They align
the policy table with existing implementation and detailed compatibility pages.

## Tests

Coverage includes:

- MySQL 8.4.9 expectation probes for `CREATE`, `ALTER`, and `DROP LOGFILE
  GROUP` syntax on the target non-NDB runtime;
- parser classification tests for logfile-group no-op statements;
- runtime utility no-op tests covering result shape, warning count,
  `ROW_COUNT()`, and transaction preservation;
- compatibility documentation updates for tablespaces and embedded-design
  decisions.

## Compatibility Status

`LOGFILE GROUP` statements are covered embedded placeholders. The broader
embedded-design policy rows are no longer red when MyLite has a documented
policy, parser/runtime behavior, and compatibility references. Detailed
subfeature rows remain red where the actual table, protocol, variable, or
instrumentation surface is not implemented.
