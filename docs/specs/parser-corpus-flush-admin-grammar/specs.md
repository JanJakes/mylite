# Parser Corpus FLUSH Admin Grammar

This slice moves common `FLUSH` administration statements from the
single-statement placeholder fallback into normal MyLite grammar. Runtime
behavior stays the existing embedded admin no-op: success, no result columns,
zero affected rows, and one warning that the server-only statement was accepted
as an embedded no-op.

The immediate corpus case is:

```sql
FLUSH TABLES;
UNLOCK TABLES;
```

`UNLOCK TABLES` already parses normally. `FLUSH TABLES` previously only parsed
through the fallback placeholder scanner, which intentionally rejects
non-trailing semicolons. Normal grammar coverage lets script parsing succeed
without changing MyLite's single-statement runtime execution rule.

## Compatibility Authority

MySQL 8.4 documents `FLUSH` as a server administration statement with table,
log, privilege, status, optimizer-cost, user-resource, binary-log, relay-log,
engine-log, error-log, general-log, and slow-log forms. MyLite does not emulate
server caches, log rotation, table-cache state, global read locks, grant
reloads, or binary/relay log state. These forms therefore remain embedded
no-ops.

Reference: <https://dev.mysql.com/doc/refman/8.4/en/flush.html>

## Grammar Scope

Normal grammar admits these no-op statement forms:

```lemon
flush_admin_noop_statement ::= FLUSH PRIVILEGES.
flush_admin_noop_statement ::= FLUSH STATUS.
flush_admin_noop_statement ::= FLUSH OPTIMIZER_COSTS.
flush_admin_noop_statement ::= FLUSH USER_RESOURCES.
flush_admin_noop_statement ::= FLUSH LOGS.
flush_admin_noop_statement ::= FLUSH BINARY LOGS.
flush_admin_noop_statement ::= FLUSH ENGINE LOGS.
flush_admin_noop_statement ::= FLUSH ERROR LOGS.
flush_admin_noop_statement ::= FLUSH GENERAL LOGS.
flush_admin_noop_statement ::= FLUSH SLOW LOGS.
flush_admin_noop_statement ::= FLUSH RELAY LOGS.
flush_admin_noop_statement ::= FLUSH TABLE.
flush_admin_noop_statement ::= FLUSH TABLES.
flush_admin_noop_statement ::= FLUSH TABLE WITH READ LOCK.
flush_admin_noop_statement ::= FLUSH TABLES WITH READ LOCK.
```

Each form maps to the existing `MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT` node. More
specific MySQL 8.4 `FLUSH` forms with table lists, relay-log channel clauses,
or the optional binary-log suppression modifier continue to rely on the fallback
scanner when they are issued as a single statement. Removed older forms such as
`FLUSH HOSTS` and `FLUSH QUERY CACHE` remain syntax errors.

## Runtime Behavior

Runtime execution is unchanged from the existing admin placeholder:

- no result columns and no rows;
- affected rows and `ROW_COUNT()` are `0`;
- one warning: `1105 / HY000`, `MyLite accepted this server-only statement as
  an embedded no-op`;
- no catalog, lock, transaction, cache, privilege, log, or connection state
  side effects.

Multiple statements in one SQL string still remain a runtime execution error
outside parser benchmarking and parser tests.

## Tests

Coverage includes:

- parser acceptance for the normal-grammar `FLUSH` forms listed above;
- parser fallback acceptance for documented table-list and relay-channel forms;
- parser rejection for removed host/query-cache forms and invalid mixed
  table-option lists;
- parser acceptance of `FLUSH TABLES; UNLOCK TABLES` as a two-statement script;
- runtime admin no-op warning behavior for `FLUSH TABLES`;
- corpus benchmark verification showing the `FLUSH TABLES; UNLOCK TABLES`
  row no longer contributes a default parse failure.
