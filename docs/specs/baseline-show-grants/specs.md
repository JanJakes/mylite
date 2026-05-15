# Baseline SHOW GRANTS

## Status

This phase adds a narrow current-user `SHOW GRANTS` surface for MyLite's
embedded identity model:

- `SHOW GRANTS`
- `SHOW GRANTS FOR CURRENT_USER`
- `SHOW GRANTS FOR CURRENT_USER()`

MyLite has no account store, authentication, role graph, grant descriptors, or
privilege enforcement. The supported behavior is therefore a synthetic result
for the existing embedded `root@%` session identity, aligned with the privilege
metadata exposed by the baseline `INFORMATION_SCHEMA.USER_PRIVILEGES` phase.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing current-user identity surface:
  `docs/specs/baseline-current-user-identity/specs.md`
- Existing privilege metadata surface:
  `docs/specs/baseline-information-schema-privileges/specs.md`
- MySQL 8.4 Reference Manual, `SHOW GRANTS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-grants.html
- MySQL 8.4 Reference Manual, account-name syntax:
  https://dev.mysql.com/doc/refman/8.4/en/account-names.html
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_show_grants_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SHOW GRANTS`, `SHOW GRANTS FOR CURRENT_USER`, and
  `SHOW GRANTS FOR CURRENT_USER()` are accepted and display grants for the
  current user.
- The result has one text column. For the target MySQL runtime's `root@%`
  account, the column label is `Grants for root@%`.
- The target runtime returns two rows for `root@%`:
  one row listing all static global privileges with `WITH GRANT OPTION`, and
  one row listing all dynamic global privileges with `WITH GRANT OPTION`.
- The static global privilege row uses comma-space separators.
- The dynamic global privilege row uses comma separators without following
  spaces.
- Successful `SHOW GRANTS` returns warning count `0` and makes the following
  `ROW_COUNT()` return `-1`.
- `SHOW GRANTS FOR 'missing'@'%'` fails with MySQL error `1141`, SQLSTATE
  `42000`, and the diagnostic text
  `There is no such grant defined for user 'missing' on host '%'`.
- `SHOW GRANTS FOR user` defaults the omitted host to `%` in MySQL, but named
  account and role lookup is outside this MyLite phase because MyLite does not
  yet have account or role descriptors.
- `SHOW GRANTS ... USING role` is role-sensitive in MySQL and is outside this
  phase.

## Scope

The implementation must add:

- parser/AST support for the three current-user forms listed in Status;
- a runtime result set with one column named `Grants for root@%`;
- two deterministic grant rows for MyLite's embedded `root@%` identity:
  the static global privilege row and the dynamic global privilege row observed
  from MySQL 8.4.9 for the target root account;
- successful result-set behavior through existing public result conventions:
  affected rows `0`, warning count `0`, and subsequent `ROW_COUNT() = -1`;
- no catalog, schema, table, descriptor, storage, VFS, or SQLite fork changes;
- fast C parser/runtime tests plus a reproducible MySQL 8.4.9 expectation
  script.

## Non-Goals

This feature must not implement:

- named account parsing or lookup, including `SHOW GRANTS FOR user` and
  `SHOW GRANTS FOR 'user'@'host'`;
- role names, role state, `USING`, mandatory roles, default roles, active
  roles, proxy grants, partial revokes, or role-derived grants;
- account storage, authentication, passwords, grant descriptors, grant DDL,
  revocation DDL, `SHOW CREATE USER`, `mysql.*` privilege tables, privilege
  enforcement, definer privilege checks, or privilege-dependent filtering;
- `LIKE`, `WHERE`, ordering, limits, query modifiers, subqueries, CTEs, or
  arbitrary expressions on `SHOW GRANTS`;
- physical SQLite tables, storage-format changes, or SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: reused only for existing execution bookkeeping. No new
  session state is introduced.
- Parser/AST: owns recognition of the three admitted `SHOW GRANTS` spellings
  and emits a dedicated statement node.
- Analyzer/planner: no descriptor analysis is needed in this phase because the
  supported forms all target the current embedded identity.
- Catalog module: unchanged. The synthetic grants are not catalog descriptors
  and must not mutate catalog rows, descriptor versions, or schema generation.
- Result builder: emits one MySQL-shaped text column and two text rows through
  `mylite_result`.
- Storage/VFS: unchanged. `.mylite` preamble, shifted SQLite payload, and
  independent file handles remain unaffected.
- SQLite physical storage: not used. No SQLite SQL is generated for this
  surface.

## Grammar

The supported MyLite grammar subset is independently described as:

```lemon
statement(A) ::= show_grants_statement(B).

show_grants_statement(A) ::= SHOW(S) GRANTS(G).
show_grants_statement(A) ::= SHOW(S) GRANTS(G) FOR CURRENT_USER(C).
show_grants_statement(A) ::= SHOW(S) GRANTS(G) FOR CURRENT_USER(C) LPAREN RPAREN(R).
```

`CURRENT_USER` is accepted exactly as MyLite already tokenizes the current-user
function keyword. The two `FOR CURRENT_USER` forms produce the same runtime
result as bare `SHOW GRANTS`.

## Result Semantics

For every admitted form, MyLite returns:

| Column | Value |
| --- | --- |
| name | `Grants for root@%` |
| row count | `2` |
| warning count | `0` |
| affected rows | `0` through the existing row-result convention |
| following `ROW_COUNT()` | `-1` |

Rows:

```text
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, RELOAD, SHUTDOWN, PROCESS, FILE, REFERENCES, INDEX, ALTER, SHOW DATABASES, SUPER, CREATE TEMPORARY TABLES, LOCK TABLES, EXECUTE, REPLICATION SLAVE, REPLICATION CLIENT, CREATE VIEW, SHOW VIEW, CREATE ROUTINE, ALTER ROUTINE, CREATE USER, EVENT, TRIGGER, CREATE TABLESPACE, CREATE ROLE, DROP ROLE ON *.* TO `root`@`%` WITH GRANT OPTION
GRANT ALLOW_NONEXISTENT_DEFINER,APPLICATION_PASSWORD_ADMIN,AUDIT_ABORT_EXEMPT,AUDIT_ADMIN,AUTHENTICATION_POLICY_ADMIN,BACKUP_ADMIN,BINLOG_ADMIN,BINLOG_ENCRYPTION_ADMIN,CLONE_ADMIN,CONNECTION_ADMIN,ENCRYPTION_KEY_ADMIN,FIREWALL_EXEMPT,FLUSH_OPTIMIZER_COSTS,FLUSH_PRIVILEGES,FLUSH_STATUS,FLUSH_TABLES,FLUSH_USER_RESOURCES,GROUP_REPLICATION_ADMIN,GROUP_REPLICATION_STREAM,INNODB_REDO_LOG_ARCHIVE,INNODB_REDO_LOG_ENABLE,OPTIMIZE_LOCAL_TABLE,PASSWORDLESS_USER_ADMIN,PERSIST_RO_VARIABLES_ADMIN,REPLICATION_APPLIER,REPLICATION_SLAVE_ADMIN,RESOURCE_GROUP_ADMIN,RESOURCE_GROUP_USER,ROLE_ADMIN,SENSITIVE_VARIABLES_OBSERVER,SERVICE_CONNECTION_ADMIN,SESSION_VARIABLES_ADMIN,SET_ANY_DEFINER,SHOW_ROUTINE,SYSTEM_USER,SYSTEM_VARIABLES_ADMIN,TABLE_ENCRYPTION_ADMIN,TELEMETRY_LOG_ADMIN,TRANSACTION_GTID_TAG,XA_RECOVER_ADMIN ON *.* TO `root`@`%` WITH GRANT OPTION
```

The grant rows are intentionally synthetic compatibility text. They are not
permission checks, not reconstructed from mutable descriptors, and not a
promise that all listed MySQL administration operations are implemented.

## Diagnostics

Supported statements succeed with no warnings.

Unsupported `SHOW GRANTS` extensions are rejected deterministically by the
parser or by the existing unsupported-statement diagnostic path until the
corresponding account or role descriptors are designed. This includes:

- named accounts and roles;
- account names containing host parts;
- `USING` role clauses;
- `LIKE`, `WHERE`, `ORDER BY`, or `LIMIT`;
- query modifiers or trailing expression syntax.

Allocation failures follow existing `MYLITE_NOMEM` behavior and must leave no
leaked result objects. Public API misuse is unchanged.

## Performance

The runtime builds a constant two-row result. It does not scan catalog tables,
ask SQLite for metadata, execute SQLite SQL, or materialize user data. The
cost is bounded by allocating the public result object and copying two grant
strings.

## Tests

Fast C tests must cover:

- parser acceptance for `SHOW GRANTS`, `SHOW GRANTS FOR CURRENT_USER`, and
  `SHOW GRANTS FOR CURRENT_USER()`;
- parser rejection of named accounts, `USING`, filters, ordering, and limits;
- runtime result column name, row count, exact grant row text, warning count,
  affected rows, and subsequent `ROW_COUNT()`;
- the same runtime output with and without an explicitly selected schema;
- independent in-memory handles;
- file-backed close/reopen behavior and `.mylite` preamble preservation;
- zero-initialized cleanup for result objects by exercising early cleanup
  paths through existing test helpers.

The MySQL expectation script must verify the three admitted current-user forms
against MySQL 8.4.9 and must fail if the runtime is not MySQL 8.4.9.

## Compatibility Documentation

Update only the exact partial surface:

- `COMPATIBILITY.md` current-user identity notes;
- `docs/compatibility/sql-show-statements.md` `SHOW GRANTS`;
- `docs/compatibility/sql-users-privileges.md`.

Do not document general account, role, privilege, or grant-management support.
