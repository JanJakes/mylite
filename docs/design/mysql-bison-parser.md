# MySQL Bison Parser Prototype

## Goal

MyLite needs a parser that can accept the MySQL 8.4 SQL surface before the
analyzer and SQLite translation layers are complete. This prototype establishes
the Bison-based parser boundary, a MySQL-aware lexer, a public parse API, and a
corpus gate against the WordPress SQLite Database Integration MySQL query set.

## Sources

- MySQL 8.4.9 parser grammar: `sql/sql_yacc.yy`
- MySQL 8.4 statement labels:
  `https://dev.mysql.com/doc/refman/8.4/en/statement-labels.html`
- MySQL 8.4 local variable DECLARE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/declare-local-variable.html`
- MySQL 8.4 DECLARE ... CONDITION statement:
  `https://dev.mysql.com/doc/refman/8.4/en/declare-condition.html`
- MySQL 8.4 DECLARE ... HANDLER statement:
  `https://dev.mysql.com/doc/refman/8.4/en/declare-handler.html`
- MySQL 8.4 GET DIAGNOSTICS statement:
  `https://dev.mysql.com/doc/refman/8.4/en/get-diagnostics.html`
- MySQL 8.4 SELECT ... INTO statement:
  `https://dev.mysql.com/doc/refman/8.4/en/select-into.html`
- MySQL 8.4 SET variable assignment statement:
  `https://dev.mysql.com/doc/refman/8.4/en/set-variable.html`
- MySQL 8.4 connection character-set SET statements:
  `https://dev.mysql.com/doc/refman/8.4/en/set-character-set.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-names.html`
- MySQL 8.4 IMPORT TABLE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/import-table.html`
- MySQL 8.4 CALL statement:
  `https://dev.mysql.com/doc/refman/8.4/en/call.html`
- MySQL 8.4 condition signaling statements:
  `https://dev.mysql.com/doc/refman/8.4/en/signal.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/resignal.html`
- MySQL 8.4 account-management SET statements:
  `https://dev.mysql.com/doc/refman/8.4/en/set-role.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-default-role.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-password.html`
- MySQL 8.4 account-introspection SHOW statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-user.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-grants.html`
- MySQL 8.4 schema-scoped SHOW statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-databases.html`
- MySQL 8.4 SHOW character-set statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-collation.html`
- MySQL 8.4 SHOW PROFILE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-profile.html`
- MySQL 8.4 SHOW variable statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-variables.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-status.html`
- MySQL 8.4 SHOW diagnostics statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-errors.html`
- MySQL 8.4 EXPLAIN for named connections:
  `https://dev.mysql.com/doc/refman/8.4/en/explain-for-connection.html`
- MySQL 8.4 KILL statement:
  `https://dev.mysql.com/doc/refman/8.4/en/kill.html`
- MySQL 8.4 binary log statements:
  `https://dev.mysql.com/doc/refman/8.4/en/binlog.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-binlog-events.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/purge-binary-logs.html`
- MySQL 8.4 SHOW RELAYLOG EVENTS statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-relaylog-events.html`
- MySQL 8.4 SHOW REPLICA STATUS statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-replica-status.html`
- MySQL 8.4 FLUSH statement:
  `https://dev.mysql.com/doc/refman/8.4/en/flush.html`
- MySQL 8.4 table-maintenance statements:
  `https://dev.mysql.com/doc/refman/8.4/en/table-maintenance-statements.html`
- MySQL 8.4 RESET PERSIST statement:
  `https://dev.mysql.com/doc/refman/8.4/en/reset-persist.html`
- MySQL 8.4 CLONE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/clone.html`
- MySQL 8.4 STOP replication statements:
  `https://dev.mysql.com/doc/refman/8.4/en/stop-replica.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/stop-group-replication.html`
- MySQL 8.4 replication channel statements:
  `https://dev.mysql.com/doc/refman/8.4/en/start-replica.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/stop-replica.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/reset-replica.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/change-replication-source-to.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/change-replication-filter.html`
- MySQL 8.4 XA transaction statements:
  `https://dev.mysql.com/doc/refman/8.4/en/xa-statements.html`
- MySQL 8.4 transaction-control statements:
  `https://dev.mysql.com/doc/refman/8.4/en/commit.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-transaction.html`
- MySQL 8.4 HELP statement:
  `https://dev.mysql.com/doc/refman/8.4/en/help.html`
- MySQL 8.4 component and plugin statements:
  `https://dev.mysql.com/doc/refman/8.4/en/install-component.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/uninstall-component.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/install-plugin.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/uninstall-plugin.html`
- MySQL 8.4 MyISAM key cache statements:
  `https://dev.mysql.com/doc/refman/8.4/en/cache-index.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/load-index.html`
- MySQL 8.4 resource group statements:
  `https://dev.mysql.com/doc/refman/8.4/en/create-resource-group.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-resource-group.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/drop-resource-group.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-resource-group.html`
- MySQL 8.4 server and logfile-group DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-server.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-server.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/drop-server.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/create-logfile-group.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-logfile-group.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/drop-logfile-group.html`
- MySQL 8.4 instance-level statements:
  `https://dev.mysql.com/doc/refman/8.4/en/alter-instance.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/lock-instance-for-backup.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/restart.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/shutdown.html`
- WordPress SQLite Database Integration query corpus:
  `packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv`

## Design

The current parser is a syntax-acceptance milestone, not the final semantic
grammar. Bison owns statement sequencing and balanced structure. The lexer
handles MySQL comments, executable version-comment bodies, quoted identifiers,
string literals, numeric literals, user/system variables, parameter markers,
statement-leading keywords, and stored program `END IF` / `END LOOP` style
compound endings.
MySQL prefixed literals such as `_utf8mb4'text'`, `N'text'`, `X'ff'`, and
`B'1010'` are emitted as single literal tokens with source spans covering the
prefix and quoted body.

The parser records the full token stream, a statement kind, an optional target
object kind for DDL/admin statements and DML table operations, an optional first
target-name span, and source spans for each parsed statement. Spans include
token ordinals, byte offsets into the original SQL buffer, and line/column
endpoints for diagnostics and future AST nodes. Object-name spans preserve
exact source text, including backtick quoting and schema qualification. Balanced
structural tokens also carry bidirectional match references for `(...)`,
`[...]`, `{...}`, `BEGIN ... END`, and `CASE ... END`. The lexer classifies
statement-leading words, major clause words, common DDL/admin/transaction/load
words, boolean/null operators, and join/set operators as keywords so the future
analyzer does not need to rediscover them from identifier text. Keyword-like
nonreserved words that MySQL commonly permits as identifiers remain usable in
target object-name spans. The grammar validates that
grouping delimiters, `BEGIN ... END`, and `CASE ... END` blocks are balanced.
Known statement heads that require a body now reject a bare keyword, while
transaction statements that MySQL accepts as single-keyword statements remain
valid.

Statement bodies remain token-preserving and permissive so the prototype can
accept the broad MySQL statement inventory while detailed productions are added
statement by statement. `WITH` statements are post-classified by skipping
matched CTE subqueries and inspecting the outer DML verb, so `WITH ... UPDATE`,
`WITH ... DELETE`, and `WITH ... INSERT` do not collapse to `select`. DML
target-name classification uses the same matched-token data to skip CTE bodies
before locating the first affected table for `INSERT`, `REPLACE`, `UPDATE`, and
`DELETE`, including common priority, delayed, quick, and ignore modifiers.
`SELECT ... INTO` assignment targets are recorded for user variables and local
variables, while `INTO OUTFILE` and `INTO DUMPFILE` record the explicit export
file target.
Direct target metadata is also recorded for simple utility and table statements
where the target is syntactically unambiguous: `USE`, `TABLE`, `TRUNCATE`,
`HANDLER`, `IMPORT TABLE FROM`, `CALL`, direct `DESCRIBE` / `EXPLAIN` table
forms, `EXPLAIN ... FOR CONNECTION`, `SIGNAL` / `RESIGNAL` condition values,
`LOAD ... INTO TABLE`,
`CACHE INDEX`, `LOAD INDEX INTO CACHE`, `LOCK TABLES`, `SHOW CREATE ...`,
`SHOW COLUMNS` / `FIELDS`,
`SHOW INDEX` / `KEYS`, `SHOW TABLES FROM ...`, schema-scoped `SHOW TABLE
STATUS`, `SHOW OPEN TABLES`, `SHOW EVENTS`, and `SHOW TRIGGERS`, account
targets in `SHOW CREATE USER` and `SHOW GRANTS FOR`, database targets in
`SHOW DATABASES` and `SHOW SCHEMAS`, routine targets in `SHOW FUNCTION CODE`
and `SHOW PROCEDURE CODE`, engine targets in
`SHOW ENGINE ... STATUS` and `SHOW ENGINE ... MUTEX`, character-set and
collation targets in `SHOW CHARACTER SET`, `SHOW CHARSET`, and
`SHOW COLLATION`, query targets in `SHOW PROFILE ... FOR QUERY`,
system-variable and status-variable targets in `SHOW VARIABLES`,
`SHOW STATUS`, and their scoped forms, diagnostics-area targets in
`SHOW WARNINGS`, `SHOW ERRORS`, and their `SHOW COUNT(*) ...` forms,
connection targets in `KILL`, binary log event
payloads in `BINLOG`, binary log targets in `SHOW BINLOG EVENTS IN ...` and
`PURGE BINARY LOGS TO ...`, relay log targets in
`SHOW RELAYLOG EVENTS IN ...`, replication channel targets in channel-only
`SHOW RELAYLOG EVENTS`, `SHOW REPLICA STATUS`, and `FLUSH RELAY LOGS`, table
targets in `FLUSH TABLES`, and
table-maintenance targets in `ANALYZE`, `CHECK`, `CHECKSUM`, `OPTIMIZE`, and
`REPAIR`, and prepared-statement names in
`PREPARE`, `EXECUTE`, `DEALLOCATE PREPARE`, and `DROP PREPARE`. Persisted
system-variable targets are recorded for `RESET PERSIST`. Replication channel
targets are recorded for `START`, `STOP`, `RESET`, and `CHANGE` forms that
include `FOR CHANNEL`. XA transaction XID targets are recorded for the
XID-bearing XA statements. Non-XA transaction-control statements record the
transaction object kind for `BEGIN`, `BEGIN WORK`, `START TRANSACTION`,
`COMMIT`, bare `ROLLBACK`, and `SET [GLOBAL | SESSION] TRANSACTION`, while
leaving compound `BEGIN ... END` blocks objectless. Quoted `HELP` search
topics are recorded as help-topic targets. Component and plugin targets are
recorded for `INSTALL` and `UNINSTALL` administrative statements.
`CLONE` has an explicit statement kind while its local-directory and remote
instance endpoints remain body tokens.
`STOP` has an explicit statement kind for replication-control statements.
Resource group targets are recorded for `CREATE`, `ALTER`, `DROP`, and
`SET RESOURCE GROUP`. Server, logfile-group, and tablespace DDL targets are
recorded for the low-level storage/metadata statements that expose a direct
name. Instance-level `ALTER`, `LOCK`, `UNLOCK`, `RESTART`, and `SHUTDOWN`
statements are recorded with an object kind but no object-name span. Principal
targets are recorded for
`GRANT ... TO` and `REVOKE ... FROM`, including the first `user@host` span when
present. Account and role DDL target spans also
preserve `user@host` / `role@host` syntax for `CREATE`, `ALTER`, `DROP`, and
`RENAME` forms. Account-management `SET` metadata is recorded for explicit
`SET ROLE`, `SET DEFAULT ROLE`, and `SET PASSWORD FOR` role or account targets,
and variable-assignment `SET` metadata is recorded for explicit user-variable
and system-variable targets. Unadorned `SET name = ...` assignments remain
objectless until semantic context can distinguish local variables from system
variables. Connection character-set `SET NAMES` and `SET CHARACTER SET` forms
record the target character set. Savepoint names are recorded for `SAVEPOINT`,
`RELEASE SAVEPOINT`, and `ROLLBACK TO SAVEPOINT`.
Statements that begin with parenthesized query expressions keep spans anchored
to the opening parenthesis and are classified as `SELECT`, `VALUES`, or `TABLE`
according to the innermost leading query token.
Stored-program statement heads such as `DECLARE`, cursor operations, `IF`,
`CASE`, loop forms, `LEAVE`, `ITERATE`, and `RETURN` have explicit statement
kinds. Compound-control tokens are structurally matched for `IF ... END IF`,
`LOOP ... END LOOP`, `REPEAT ... END REPEAT`, and `WHILE ... END WHILE` without
misclassifying `IF(...)` expressions or `IF [NOT] EXISTS` clauses as compound
block starts. Local variable names are recorded for ordinary `DECLARE`
statements. Cursor names are recorded for `DECLARE ... CURSOR`, `OPEN`,
`FETCH`, and `CLOSE`. Jump target labels are recorded for `LEAVE` and
`ITERATE`. Label declarations are recorded when they prefix the MySQL-labeled
constructs: `BEGIN`, `LOOP`, `REPEAT`, and `WHILE`. Named condition
declarations are recorded for `DECLARE ... CONDITION`, and the first handled
condition value is recorded for `DECLARE ... HANDLER`. `GET DIAGNOSTICS
... CONDITION` records the requested diagnostics condition area number.
Statement-level `GET DIAGNOSTICS` records the first explicit assignment target.

## Boundaries

- Produces a token stream and statement/object-kind/name-span AST shell only.
  Matching token pairs are structural metadata, not a full expression tree.
- Does not yet resolve identifiers, expression precedence, table references,
  metadata, warnings, or MySQL runtime errors.
- DML object metadata records the first syntactic target table span only. It
  does not yet resolve aliases, joined table references, partition clauses, or
  every affected table in multi-table statements.
- Assignment metadata records only the first direct assignment target. It does
  not expand multi-target `SELECT ... INTO`, `SET`, or `GET DIAGNOSTICS`
  assignment lists, and it does not classify ambiguous unadorned `SET name`
  assignments without semantic scope information.
- Character-set `SET` metadata records only the requested character set. It
  does not validate charset availability, collation compatibility, or the
  session variables affected by the statement.
- File-export metadata records only the first literal target for
  `SELECT ... INTO OUTFILE` and `SELECT ... INTO DUMPFILE`; field/line options
  and file I/O behavior remain unimplemented.
- Utility object metadata records the first direct target only and does not yet
  expand multi-table maintenance, cache-index lists, load-index lists, or lock
  lists. `DESCRIBE` and `EXPLAIN` target metadata is deliberately conservative:
  direct table forms and `EXPLAIN ... FOR CONNECTION` record targets, while
  query-plan forms such as `EXPLAIN SELECT` and `EXPLAIN FORMAT=... SELECT`
  remain objectless.
  `IMPORT TABLE` metadata records only the first string SDI file target.
  `CALL` metadata records only the first procedure name and does not classify
  parameters or OUT/INOUT binding semantics.
  `SIGNAL` and `RESIGNAL` metadata records only the explicit SQLSTATE literal
  or named condition value. Bare `RESIGNAL` and `RESIGNAL SET ...` forms remain
  objectless, and signal information item assignments are not yet classified.
  `SHOW` metadata is similarly limited to forms with a clear table, view, or
  schema/account target. Schema-scoped `SHOW` metadata is recorded only when an
  explicit `FROM` or `IN` schema name is present. Prepared-statement metadata
  records the statement handle name, not the SQL text referenced by `PREPARE`.
  Component/plugin
  metadata records only the first target in multi-target statements. Resource
  group metadata records only the named group, not VCPU, priority, or thread
  assignment lists. Server, logfile-group, and tablespace metadata records only
  the named object, not engine-specific options or rename destinations.
  Instance metadata records only the instance-level command surface, not the
  specific backup-lock, TLS, keyring, or redo-log operation.
  `KILL` metadata records only the processlist id token, not the distinction
  between query and connection termination.
  Binary log metadata records only the explicit log-file target and does not
  classify position or time expressions. `BINLOG` metadata records only the
  first string event payload.
  `SHOW RELAYLOG EVENTS` metadata records the explicit relay log name when
  present, otherwise an explicit channel name, and leaves bare forms objectless.
  `SHOW REPLICA STATUS` metadata records only explicit channel names and leaves
  bare status forms objectless.
  `FLUSH TABLES` metadata records only the first table target. `FLUSH RELAY
  LOGS` records only explicit channel names. Other global flush forms remain
  objectless.
  Table-maintenance metadata records only the first concrete table target and
  leaves malformed or name-less forms objectless.
  `RESET PERSIST` metadata records the first variable name only and leaves
  full persisted-variable reset forms objectless.
  Replication metadata records only explicit `FOR CHANNEL` names and leaves
  default-channel operations objectless.
  XA metadata records only the first XID token and leaves `XA RECOVER`
  objectless.
  `HELP` metadata records only quoted search strings and leaves keyword topics
  objectless.
  Instance lifecycle metadata records only the instance object kind, not
  privilege, connection-loss, shutdown, or restart semantics.
  `SHOW PROFILE` metadata records only numeric `FOR QUERY` ids and leaves bare
  `SHOW PROFILE` / `SHOW PROFILES` objectless.
- Account and principal metadata records the first syntactic account or role
  target only. It does not yet resolve roles, dynamic privileges, multiple
  accounts, proxy grants, account-name normalization, rename destinations, or
  implicit current-user/current-role targets in `SET` statements.
- Savepoint metadata records the named savepoint only. Bare `ROLLBACK` records
  the transaction object kind, and non-savepoint `RELEASE` forms remain
  objectless.
- Parenthesized query-expression classification only identifies the leading
  query statement kind; it does not build the query-expression tree.
- Stored-program control matching records token pairs only; it does not yet
  validate label binding, declaration ordering, cursor scope, or control-flow
  semantics.
- Cursor metadata records the first cursor handle only. It does not yet validate
  declaration scope, result shape, fetch target lists, or cursor lifecycle.
- Local variable metadata records only the first declared variable name. It does
  not expand comma-separated declaration lists, validate type/default clauses,
  or model block scope.
- Condition declaration metadata records only the declared condition name. It
  does not yet validate declaration ordering, SQLSTATE/error-code validity,
  handler references, or condition scope.
- Handler declaration metadata records only the first condition value after
  `FOR`. It does not expand multi-condition lists, validate handler action,
  bind named conditions, or execute handler bodies.
- `GET DIAGNOSTICS` metadata records only the requested condition-area index in
  `... CONDITION n` forms. Statement-level diagnostics item lists and individual
  assignment targets remain body tokens only.
- Label metadata records direct `LEAVE` / `ITERATE` targets and leading label
  declarations. It does not yet validate end labels, duplicate labels, the
  16-character label limit, or label binding.
- Skips ordinary comments. MySQL executable `/*! ... */` comments are tokenized
  as SQL because they can carry required syntax.
- Accepts unknown statement starts as `unknown`; later grammar work should
  reduce that surface as concrete productions land.
- Rejects bare known statement keywords such as `SELECT`, `CREATE`, and `SET`,
  but detailed clause-level syntax errors still require future productions.

## Verification

- `make smoke` builds the parser and checks representative MySQL syntax plus
  negative tests for unmatched delimiters and unterminated strings.
- `make corpus` downloads the WordPress corpus into `build/corpus/` and parses
  every query through the same CLI in NUL-delimited batch mode. The current
  corpus gate covers 69,577 records.
