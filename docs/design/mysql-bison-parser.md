# MySQL Bison Parser Prototype

## Goal

MyLite needs a parser that can accept the MySQL 8.4 SQL surface before the
analyzer and SQLite translation layers are complete. This prototype establishes
the Bison-based parser boundary, a MySQL-aware lexer, a public parse API, and a
corpus gate against the WordPress SQLite Database Integration MySQL query set.

## Sources

- MySQL 8.4.9 parser grammar: `sql/sql_yacc.yy`
- MySQL 8.4 keywords and reserved words:
  `https://dev.mysql.com/doc/refman/8.4/en/keywords.html`
- MySQL 8.4 statement labels:
  `https://dev.mysql.com/doc/refman/8.4/en/statement-labels.html`
- MySQL 8.4 compound BEGIN ... END statement:
  `https://dev.mysql.com/doc/refman/8.4/en/begin-end.html`
- MySQL 8.4 flow-control statements:
  `https://dev.mysql.com/doc/refman/8.4/en/case.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/if.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/loop.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/repeat.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/while.html`
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
- MySQL 8.4 structured system variables:
  `https://dev.mysql.com/doc/refman/8.4/en/structured-system-variables.html`
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
- MySQL 8.4 SHOW CREATE statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-database.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-event.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-function.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-procedure.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-trigger.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html`
- MySQL 8.4 routine SHOW statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-function-status.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-procedure-status.html`
- MySQL 8.4 schema-scoped SHOW statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-columns.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-databases.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-events.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-index.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-open-tables.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-tables.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-triggers.html`
- MySQL 8.4 SHOW character-set statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-collation.html`
- MySQL 8.4 SHOW PROFILE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-profile.html`
- MySQL 8.4 SHOW PARSE_TREE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-parse-tree.html`
- MySQL 8.4 SHOW variable statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-variables.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-status.html`
- MySQL 8.4 SHOW diagnostics statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-errors.html`
- MySQL 8.4 SHOW collection statements:
  `https://dev.mysql.com/doc/refman/8.4/en/show-engine.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-engines.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-plugins.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-privileges.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-processlist.html`
- MySQL 8.4 EXPLAIN for named connections:
  `https://dev.mysql.com/doc/refman/8.4/en/explain-for-connection.html`
- MySQL 8.4 KILL statement:
  `https://dev.mysql.com/doc/refman/8.4/en/kill.html`
- MySQL 8.4 binary log statements:
  `https://dev.mysql.com/doc/refman/8.4/en/binlog.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-binlog-events.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-binary-log-status.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/show-binary-logs.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/purge-binary-logs.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/reset-binary-logs-and-gtids.html`
- MySQL 8.4 SHOW RELAYLOG EVENTS statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-relaylog-events.html`
- MySQL 8.4 SHOW REPLICA STATUS statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-replica-status.html`
- MySQL 8.4 SHOW REPLICAS statement:
  `https://dev.mysql.com/doc/refman/8.4/en/show-replicas.html`
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
- MySQL 8.4 table-lock statements:
  `https://dev.mysql.com/doc/refman/8.4/en/lock-tables.html`
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
prefix and quoted body. Operators are split before an adjacent signed numeric
literal, so forms such as `@v=-1`, `a<=-1`, and `c<=>-1` keep the assignment or
comparison operator distinct from the unary sign. Digit-leading words that are
not valid numeric literals remain identifiers, matching MySQL object names such
as `15298_1`.

The parser records the full token stream, a statement kind, an optional target
object kind for DDL/admin statements and DML table operations, an optional first
target-name span, and source spans for each parsed statement. Spans include
token ordinals, byte offsets into the original SQL buffer, and line/column
endpoints for diagnostics and future AST nodes. Object-name spans preserve
exact source text, including backtick quoting and schema qualification. Reserved
words are preserved after a qualifier dot, matching MySQL's identifier rule for
qualified names. Balanced structural tokens also carry bidirectional match
references for `(...)`,
`[...]`, `{...}`, `BEGIN ... END`, and `CASE ... END`. The lexer classifies
statement-leading words, major clause words, common DDL/admin/transaction/load
words, boolean/null operators, and join/set operators as keywords so the future
analyzer does not need to rediscover them from identifier text. Keyword-like
nonreserved words that MySQL commonly permits as identifiers remain usable in
target object-name spans, including nonreserved administrative keywords such as
`CACHE`, `CLONE`, and `COMMIT`. `BEGIN` and `END` remain compound-control
tokens unless they appear in an object-name position. The grammar validates that
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
`UPDATE` table-reference scanning skips derived-table subqueries and descends
into parenthesized joined table references before the `SET` clause.
`SELECT ... INTO` and `TABLE ... INTO` assignment targets are recorded for user
variables and local variables. `SET` system-variable targets preserve qualified
structured names such as `keycache1.key_buffer_size`. `INTO OUTFILE` and
`INTO DUMPFILE` record the explicit export file target.
Direct target metadata is also recorded for simple utility and table statements
where the target is syntactically unambiguous: `USE`, `TABLE`, `TRUNCATE`,
`HANDLER`, `IMPORT TABLE FROM`, `CALL`, direct `DESCRIBE` / `EXPLAIN` table
forms, `EXPLAIN ... INTO`, `EXPLAIN ... FOR CONNECTION`,
`SIGNAL` / `RESIGNAL` condition values,
`LOAD ... INTO TABLE`,
`CACHE INDEX`, `LOAD INDEX INTO CACHE`, `LOCK` / `UNLOCK TABLES`,
`SHOW CREATE ...`,
`SHOW COLUMNS` / `FIELDS`,
`SHOW INDEX` / `KEYS`, `SHOW TABLES`, schema-scoped `SHOW TABLE STATUS`,
`SHOW OPEN TABLES`, `SHOW EVENTS`, and `SHOW TRIGGERS`, account
targets in `SHOW CREATE USER`, bare `SHOW GRANTS`, and `SHOW GRANTS FOR`,
including `CURRENT_USER()` function-call spans where documented,
database targets in
`SHOW DATABASES` and `SHOW SCHEMAS`, routine targets in `SHOW FUNCTION CODE`
and `SHOW PROCEDURE CODE`, and routine collection/pattern targets in
`SHOW FUNCTION STATUS` and `SHOW PROCEDURE STATUS`, engine targets in
`SHOW ENGINE ... STATUS`, `SHOW ENGINE ... MUTEX`, `SHOW ENGINES`, and
`SHOW STORAGE ENGINES`,
plugin targets in `SHOW PLUGINS`, privilege targets in `SHOW PRIVILEGES`,
connection targets in `SHOW PROCESSLIST`,
character-set and collation targets in `SHOW CHARACTER SET`, `SHOW CHARSET`,
and `SHOW COLLATION`, query targets in `SHOW PROFILE`, `SHOW PROFILES`,
`SHOW PROFILE ... FOR QUERY`, and `SHOW PARSE_TREE SELECT ...`,
system-variable and status-variable targets in `SHOW VARIABLES`,
`SHOW STATUS`, and their scoped forms, diagnostics-area targets in
`SHOW WARNINGS`, `SHOW ERRORS`, and their `SHOW COUNT(*) ...` forms,
connection and query targets in `KILL`, binary log event
payloads in `BINLOG`, binary log targets in `SHOW BINARY LOGS`,
legacy `SHOW MASTER LOGS`,
`SHOW BINARY LOG STATUS`, `SHOW MASTER STATUS`, `SHOW BINLOG EVENTS`,
`PURGE BINARY LOGS TO ...`, `PURGE BINARY LOGS BEFORE ...`, and
`RESET BINARY LOGS AND GTIDS`, including legacy `RESET MASTER` routing, relay
log targets in `SHOW RELAYLOG EVENTS IN ...`, replication channel targets in
channel-only
`SHOW RELAYLOG EVENTS`, `SHOW REPLICAS`, `SHOW SLAVE HOSTS`,
`SHOW REPLICA STATUS`, `SHOW SLAVE STATUS`, and
`FLUSH RELAY LOGS`, documented global log/cache/privilege/status/user-resource
targets in `FLUSH`, table targets in `FLUSH TABLES`, and
table-maintenance targets in `ANALYZE`, `CHECK`, `CHECKSUM`, `OPTIMIZE`, and
`REPAIR`. The permissive parser also preserves first table targets for
corpus-observed plural `TABLES` spellings while semantic validation remains a
later grammar responsibility. `USE` has targeted syntax validation for the
single identifier-like schema-name form MySQL accepts, rejecting missing names,
quoted strings, variables, qualified names, and trailing tokens. `TRUNCATE`
validates a single identifier-like or qualified table target across optional
`TABLE` forms.
Prepared-statement names are recorded in
`PREPARE`, `EXECUTE`, `DEALLOCATE PREPARE`, and `DROP PREPARE`; those forms
validate handle names, `PREPARE ... FROM` sources, and `EXECUTE ... USING`
user-variable binding lists. Persisted
system-variable targets are recorded for `RESET PERSIST`. Local clone
directories and remote donor endpoints are recorded for `CLONE`. Replication
channel targets are recorded for explicit and default-channel `START`, `STOP`,
`RESET`, and `CHANGE` forms, including legacy `CHANGE MASTER TO` routing.
Group Replication start/stop statements record the group-replication subsystem
target. XA transaction XID targets are recorded for the
XID-bearing XA statements, while `XA RECOVER` records the XA transaction
collection. Non-XA transaction-control statements record the
transaction object kind for `BEGIN`, `BEGIN WORK`, `START TRANSACTION`,
`COMMIT`, bare `ROLLBACK`, and `SET [GLOBAL | LOCAL | SESSION] TRANSACTION`, while
leaving compound `BEGIN ... END` blocks objectless. `COMMIT` and non-savepoint
`ROLLBACK` validate MySQL completion-clause ordering for `AND [NO] CHAIN` and
`[NO] RELEASE`. Quoted `HELP` search
topics are recorded as help-topic targets. Component and plugin targets are
recorded for `INSTALL` and `UNINSTALL` administrative statements, with
component URI string lists, optional `INSTALL COMPONENT ... SET` assignments,
and plugin `SONAME` syntax validated.
Local `CLONE` directory targets and remote `CLONE INSTANCE` donor endpoints
are recorded as direct targets.
`STOP` has an explicit statement kind for replication-control statements.
Nameless `ALTER DATABASE` / `ALTER SCHEMA` option forms are recorded with the
database or schema object kind and no invented target name, matching MySQL's
default-database syntax.
Stored-object `DEFINER = user` clauses are skipped during object scanning so
routine, event, trigger, and view targets are not confused with definer account
tokens.
Resource group targets are recorded for `CREATE`, `ALTER`, `DROP`, and
`SET RESOURCE GROUP`. Server, logfile-group, tablespace, and undo-tablespace
DDL targets are recorded for the low-level storage/metadata statements that
expose a direct name. Instance-level `ALTER`, `LOCK`, `UNLOCK`, `RESTART`, and
`SHUTDOWN`
statements are recorded with an object kind but no object-name span. `RESTART`
and `SHUTDOWN` reject body tokens because MySQL accepts only the bare statement
forms. Principal
targets are recorded for
`GRANT ... TO` and `REVOKE ... FROM`, including the first `user@host` span when
present. Account and role DDL target spans also
preserve `user@host` / `role@host` syntax for `CREATE`, `ALTER`, `DROP`, and
`RENAME` forms. Account-management `SET` metadata is recorded for explicit
`SET ROLE` and `SET DEFAULT ROLE` role targets, collection-form `SET ROLE`
active-role targets, plus `SET PASSWORD FOR` and bare current-user
`SET PASSWORD` account targets,
and variable-assignment `SET` metadata is recorded for explicit user-variable
and system-variable targets, including direct unadorned `SET name = ...`
targets at the statement boundary. User-variable targets include MySQL's quoted
variable-name forms. Structured system-variable targets include the predefined
`default` instance used by key-cache variables. System-variable assignment
targets may use keyword-shaped names when followed by an assignment operator.
Connection character-set `SET NAMES` and `SET CHARACTER SET` forms record the
target character set. Savepoint names are recorded for `SAVEPOINT`,
`RELEASE SAVEPOINT`, and `ROLLBACK [WORK] TO [SAVEPOINT]`; those forms reject
missing names, non-identifier targets, and trailing tokens.
Statements that begin with parenthesized query expressions keep spans anchored
to the opening parenthesis and are classified as `SELECT`, `VALUES`, or `TABLE`
according to the innermost leading query token. Parenthesized `TABLE` forms
preserve the named table target.
Plain `SELECT` and `WITH` query statements are recorded as query targets unless
they expose a more specific `INTO` target.
Standalone `VALUES` statements are recorded as query targets because MySQL
treats them as DML statements that return row sets.
`DO` statements are recorded as query targets too because they execute
expressions without exposing a table, schema, or administrative target.
Stored-program statement heads such as `DECLARE`, cursor operations, `IF`,
`CASE`, loop forms, `LEAVE`, `ITERATE`, and `RETURN` have explicit statement
kinds. Compound-control tokens are structurally matched for `BEGIN ... END`,
`IF ... END IF`, `CASE ... END CASE`, `LOOP ... END LOOP`,
`REPEAT ... END REPEAT`, and `WHILE ... END WHILE` without misclassifying
`BEGIN WORK`, `IF(...)` expressions, or `IF [NOT] EXISTS` clauses as compound
block starts. Standalone matched compound and flow-control blocks keep
semicolon-delimited bodies in one statement span, with optional end labels
included in that span where MySQL allows them. Local variable names are
recorded for ordinary `DECLARE` statements. Cursor names are recorded for
`DECLARE ... CURSOR`, `OPEN`, `FETCH`, including optional `NEXT FROM` / `FROM`
forms, and `CLOSE`. Jump target labels are recorded for `LEAVE` and `ITERATE`
with the same lexical rules as label
declarations. Label declarations are recorded when they prefix
semicolon-delimited MySQL-labeled constructs: `BEGIN`, `LOOP`, `REPEAT`, and
`WHILE`. Label keyword handling is separate from local-variable keyword
handling so unrestricted nonreserved label keywords can be used without quotes
while MySQL's restricted label keywords are not treated as declarations or
targets.
Named condition
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
  not expand multi-target `SELECT ... INTO`, `TABLE ... INTO`, `SET`, or
  `GET DIAGNOSTICS` assignment lists. Direct unadorned `SET name = ...`
  assignments are reported as system-variable targets at the statement boundary;
  local-variable disambiguation remains semantic-analysis work.
- Character-set `SET` metadata records only the requested character set. It
  does not validate charset availability, collation compatibility, or the
  session variables affected by the statement.
- File-export metadata records only the first literal target for
  `SELECT ... INTO OUTFILE`, `SELECT ... INTO DUMPFILE`,
  `TABLE ... INTO OUTFILE`, and `TABLE ... INTO DUMPFILE`; field/line options
  and file I/O behavior remain unimplemented.
- Utility object metadata records the first direct target only and does not yet
  expand multi-table maintenance, cache-index lists, load-index lists, or lock
  lists. `DESCRIBE` and `EXPLAIN` target metadata records direct table forms,
  `EXPLAIN ... INTO` user-variable targets, `EXPLAIN ... FOR CONNECTION`
  targets, and documented explainable statement forms as query targets,
  without modeling optimizer plans or result rows.
  `UNLOCK TABLES` metadata records the table object kind without a name because
  the statement releases the session's table locks rather than naming tables.
  `IMPORT TABLE` metadata records only the first string SDI file target and
  validates the `IMPORT TABLE FROM` string-file list shape.
  `CALL` metadata records the procedure name and validates optional
  parenthesized argument-list shape, but does not classify parameters or
  OUT/INOUT binding semantics.
  `SIGNAL` and `RESIGNAL` metadata records only the explicit SQLSTATE literal
  or named condition value. Bare `RESIGNAL` and `RESIGNAL SET ...` forms remain
  objectless, and signal information item assignments are not yet classified.
  `SHOW` metadata is similarly limited to forms with a clear table, view, event,
  trigger, schema, or account target. `SHOW COLUMNS` / `FIELDS` and
  `SHOW INDEX` / `KEYS` metadata records the inspected table target.
  Schema-scoped `SHOW` metadata records explicit `FROM` or `IN` schema names,
  otherwise collection targets or `LIKE` patterns where the pattern maps
  unambiguously to table or event names. `WHERE` filters remain analysis-layer
  work. Prepared-statement metadata
  records the statement handle name, not the SQL text referenced by `PREPARE`.
  Component/plugin
  metadata records only the first target in multi-target statements. Resource
  group metadata records only the named group, not VCPU, priority, or thread
  assignment lists. Server, logfile-group, tablespace, and undo-tablespace
  metadata records only the named object, not engine-specific options or
  rename destinations.
  Instance metadata records only the instance-level command surface, not the
  specific backup-lock, TLS, keyring, or redo-log operation.
  `KILL` metadata records single processlist-id expressions and distinguishes
  `KILL QUERY` from connection termination. `KILL` also has targeted syntax
  validation for missing targets and comma-separated target lists. `BINLOG`
  validates and records the single string event payload only.
  `CLONE` metadata records local clone directories or remote donor endpoints
  only; plugin availability, privilege checks, copy behavior, SSL requirements,
  and restart side effects remain runtime work.
  Binary log metadata records explicit log-file targets or binary-log collection
  targets for `BEFORE` purge and reset forms, including legacy
  `SHOW MASTER LOGS` and `RESET MASTER`, but does not classify position or time
  expressions. `BINLOG` metadata records only the first string event payload.
  `SHOW RELAYLOG EVENTS` metadata records the explicit relay log name when
  present, bare relay-log targets, or an explicit channel name when no log file
  is named.
  `SHOW REPLICA STATUS` and deprecated `SHOW SLAVE STATUS` metadata record
  explicit channel names or the replication-channel collection for bare status
  forms. `SHOW REPLICAS` and deprecated `SHOW SLAVE HOSTS` metadata record the
  replication-channel collection.
  `FLUSH TABLES` metadata records the first table target when present and the
  table collection for unnamed table-cache forms. `FLUSH RELAY LOGS` records
  explicit channel names or the default replication-channel collection when no
  channel is named. Other documented global `FLUSH` options record their object
  kinds after optional `LOCAL` /
  `NO_WRITE_TO_BINLOG` modifiers, but comma-separated option lists still expose
  only the first option target.
  Table-maintenance metadata records only the first concrete table target and
  leaves malformed or name-less forms objectless.
  `RESET PERSIST` metadata records the first variable name when present and
  records full persisted-variable reset forms as unnamed system-variable
  collection targets.
  `RESET BINARY LOGS AND GTIDS` metadata records the binary-log collection and
  does not classify the optional file-index number.
  Replication metadata records explicit `FOR CHANNEL` names and default-channel
  operations, but does not classify thread lists, source options, filters, or
  replica runtime state.
  Group Replication metadata records only the subsystem object kind, not user
  credentials, distributed recovery, group membership, or timeout behavior.
  XA metadata records only the first XID token for XID-bearing statements and
  the XA transaction collection for `XA RECOVER`; it does not model XA state.
  `HELP` metadata records quoted and unquoted search-topic text, including
  multi-token topics.
  Instance lifecycle metadata records only the instance object kind, not
  privilege, connection-loss, shutdown, or restart semantics.
  `SHOW PROFILE` metadata records query targets for bare forms and numeric
  `FOR QUERY` ids, while malformed `FOR QUERY` forms stay objectless.
  `SHOW PARSE_TREE` metadata records documented `SELECT` payloads as query
  targets, while non-`SELECT` payloads and runtime JSON generation remain
  analysis/runtime work.
  `SHOW ENGINE` metadata records the named engine for `STATUS`, `MUTEX`, and
  runtime-accepted `LOGS` forms.
- Account and principal metadata records the first syntactic account or role
  target only. It does not yet resolve roles, dynamic privileges, multiple
  accounts, proxy grants, account-name normalization, rename destinations, or
  implicit current-user/current-role targets in `SET` statements.
- Savepoint metadata records the named savepoint only. Bare `ROLLBACK` records
  the transaction object kind, `ROLLBACK [WORK] TO [SAVEPOINT]` records the
  savepoint name, and non-savepoint `RELEASE` forms are rejected.
- Parenthesized query-expression classification only identifies the leading
  query statement kind; it does not build the query-expression tree.
- Stored-program control matching records token pairs only; it does not yet
  validate label binding, declaration ordering, cursor scope, or control-flow
  semantics.
- Cursor metadata records the first cursor handle only, skipping documented
  `FETCH` direction/from prefixes. It does not yet validate declaration scope,
  result shape, fetch target lists, or cursor lifecycle.
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
  as SQL when ungated, or when their five- or six-digit version gate is less
  than or equal to the MySQL 8.4.9 target version.
- Treats quoted user-variable names such as `@'my-var'`, `@"my-var"`, and
  `` @`my-var` `` as single user-variable tokens.
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
