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
- MySQL 8.4 INSERT and REPLACE statements:
  `https://dev.mysql.com/doc/refman/8.4/en/insert.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/replace.html`
- MySQL 8.4 UPDATE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/update.html`
- MySQL 8.4 DELETE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/delete.html`
- MySQL 8.4 LOAD DATA and LOAD XML statements:
  `https://dev.mysql.com/doc/refman/8.4/en/load-data.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/load-xml.html`
- MySQL 8.4 condition signaling statements:
  `https://dev.mysql.com/doc/refman/8.4/en/signal.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/resignal.html`
- MySQL 8.4 account-management SET statements:
  `https://dev.mysql.com/doc/refman/8.4/en/set-role.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-default-role.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/set-password.html`
- MySQL 8.4 CREATE ROLE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/create-role.html`
- MySQL 8.4 CREATE USER statement:
  `https://dev.mysql.com/doc/refman/8.4/en/create-user.html`
- MySQL 8.4 ALTER USER statement:
  `https://dev.mysql.com/doc/refman/8.4/en/alter-user.html`
- MySQL 8.4 GRANT and REVOKE statements:
  `https://dev.mysql.com/doc/refman/8.4/en/grant.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/revoke.html`
- MySQL 8.4 routine alteration statements:
  `https://dev.mysql.com/doc/refman/8.4/en/alter-function.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-procedure.html`
- MySQL 8.4 routine creation statements:
  `https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html`
- MySQL 8.4 loadable-function DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-function-loadable.html`
- MySQL 8.4 standalone index DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-index.html`
- MySQL 8.4 view DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-view.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-view.html`
- MySQL 8.4 event DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-event.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-event.html`
- MySQL 8.4 trigger DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html`
- MySQL 8.4 CREATE TABLE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/create-table.html`
- MySQL 8.4 compact table creation forms:
  `https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/create-table-select.html`
- MySQL 8.4 ALTER TABLE statement:
  `https://dev.mysql.com/doc/refman/8.4/en/alter-table.html`
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
- MySQL 8.4 database DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-database.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-database.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/drop-database.html`
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
- MySQL 8.4 tablespace DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-tablespace.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/alter-tablespace.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/drop-tablespace.html`
- MySQL 8.4 spatial reference system DDL:
  `https://dev.mysql.com/doc/refman/8.4/en/create-spatial-reference-system.html`,
  `https://dev.mysql.com/doc/refman/8.4/en/drop-spatial-reference-system.html`
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
prefix and quoted body. Operators are split before adjacent unary signs, so
forms such as `@v=-1`, `a<=-1`, `c<=>-1`, and `SET a=-a` keep the assignment or
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
`INSERT` and `REPLACE` validation checks the documented target/source skeleton:
optional `INTO`, partition lists, optional column lists, `VALUES` / `VALUE`,
`SET`, query-backed sources, row aliases, and `ON DUPLICATE KEY UPDATE`
assignment-list shape where MySQL allows it. Expression and source-query
semantics remain later analyzer work.
`UPDATE` table-reference scanning skips derived-table subqueries and descends
into parenthesized joined table references before the `SET` clause. `UPDATE`
validation checks optional modifiers, a nonempty table-reference span, required
assignment-list shape, and optional `WHERE`, `ORDER BY`, and `LIMIT` tails while
leaving expression, join, ordering, and affected-row semantics to later phases.
`DELETE` validation distinguishes single-table `FROM` deletes from both
multi-table `FROM` and `USING` forms, checks documented modifiers, delete-target
lists including `.*`, table-reference spans, optional single-table partition
lists, and the clause rule that `ORDER BY` and `LIMIT` apply only to
single-table deletes.
`VALUES` validation checks nonempty `ROW(...)` constructors, consistent row
arity, unsupported standalone `DEFAULT` values, ordering, limit, and direct
set-operator tails.
`TABLE` validation checks direct and parenthesized table-value statements,
documented `ORDER BY`, `LIMIT`, `OFFSET`, set-operator tails, and final `INTO`
variable/export targets.
`SELECT ... INTO` and `TABLE ... INTO` assignment targets are recorded for user
variables and local variables. `SET` system-variable targets preserve qualified
structured names such as `keycache1.key_buffer_size`. `INTO OUTFILE` and
`INTO DUMPFILE` record the explicit export file target.
`SELECT ... INTO` validation checks the single top-level `INTO` rule, variable
target-list separators, OUTFILE character-set and field/line options, and
DUMPFILE target shape while leaving full SELECT query-expression analysis to a
later phase.
Direct target metadata is also recorded for simple utility and table statements
where the target is syntactically unambiguous: `USE`, `TABLE`, `TRUNCATE`,
`HANDLER`, `IMPORT TABLE FROM`, `CALL`, direct `DESCRIBE` / `EXPLAIN` table
forms, `EXPLAIN ... INTO`, `EXPLAIN ... FOR CONNECTION`,
`SIGNAL` / `RESIGNAL` condition values and signal-information item lists,
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
validated `PURGE BINARY LOGS TO ...` string targets and
`PURGE BINARY LOGS BEFORE ...` expression forms, and
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
`HANDLER` validates direct table-access forms: `OPEN` with an optional
`[AS] alias`, `READ` using documented no-index directions, indexed directions,
key-comparison value lists, and optional `WHERE` / `LIMIT` tails, and `CLOSE`.
`LOAD DATA` and `LOAD XML` validate string `INFILE` inputs, optional priority,
`LOCAL`, duplicate-key handling, `INTO TABLE` targets, character-set clauses,
row-skip clauses, field/user-variable lists, and `SET` assignments. `LOAD DATA`
also validates partition and field/line clauses, while `LOAD XML` validates
`ROWS IDENTIFIED BY`; file access and import execution remain later runtime
work.
`RENAME TABLE` validates comma-separated `old TO new` rename pairs, while the
corpus-observed `RENAME TABLES` spelling remains accepted for parser coverage.
`DROP TABLE` and `DROP VIEW` validate optional `IF EXISTS`, comma-separated
name lists, and `RESTRICT` / `CASCADE` tails; `DROP TABLE` also accepts
`TEMPORARY` and corpus-observed `TABLES` forms. `DROP` now rejects unknown
object families instead of falling back to permissive acceptance.
`CREATE DATABASE` and `CREATE SCHEMA` validate optional `IF NOT EXISTS` and
documented character-set, collation, and encryption options. `ALTER DATABASE`
and `ALTER SCHEMA` validate explicit or nameless default-database option forms,
including character-set, collation, encryption, and `READ ONLY` options.
`DROP DATABASE` and `DROP SCHEMA` validate optional `IF EXISTS` with a single
unqualified schema target.
Unambiguous `CREATE FUNCTION` loadable-function forms validate documented
`RETURNS` and `SONAME` clauses without tightening stored-function bodies yet.
`ALTER FUNCTION` and `ALTER PROCEDURE` validate documented routine
characteristics without changing parameters or bodies.
`CREATE INDEX` validates standalone index modifiers, key-part-list shape,
documented option clauses, and corpus-observed legacy `TYPE` / `RTREE` index
type spellings while table-level index definitions stay with `CREATE TABLE`
grammar work.
`CREATE VIEW` and `ALTER VIEW` validate view header clauses, optional
column-list groups, required `AS` query bodies, and trailing check-option
clauses while leaving view dependency analysis to later phases.
`DROP EVENT`, `DROP PROCEDURE`, `DROP FUNCTION`, and `DROP TRIGGER` validate
optional `IF EXISTS` with an identifier-like target; the trigger and routine
forms accept schema-qualified targets used by the corpus.
Low-level DDL forms validate their distinctive tails: `CREATE SERVER` requires
`FOREIGN DATA WRAPPER` and a nonempty documented `OPTIONS` list, `ALTER SERVER`
requires a nonempty documented `OPTIONS` list, `CREATE LOGFILE GROUP` and
`ALTER LOGFILE GROUP` require `ADD UNDOFILE` and a final `ENGINE [=] name`
clause, `CREATE TABLESPACE`, `CREATE UNDO TABLESPACE`, `ALTER TABLESPACE`, and
`ALTER UNDO TABLESPACE` validate documented datafile, size, encryption,
logfile-group, state, storage-engine, and engine-attribute clause shapes,
`CREATE SPATIAL REFERENCE SYSTEM` validates numeric SRIDs and documented SRS
attributes, `DROP SERVER` accepts optional `IF EXISTS`, `DROP SPATIAL REFERENCE
SYSTEM` requires a numeric SRID, `DROP LOGFILE GROUP` requires an engine clause,
and `DROP TABLESPACE` / `DROP UNDO TABLESPACE` accept the
corpus-observed optional engine tail.
Account and resource DDL validation covers `DROP USER` and `DROP ROLE` with
optional `IF EXISTS` plus comma-separated account/role lists. Resource-group
validation covers `CREATE RESOURCE GROUP` required `TYPE` plus optional `VCPU`,
`THREAD_PRIORITY`, and `ENABLE`/`DISABLE` clauses, `ALTER RESOURCE GROUP`
modifiable `VCPU`, `THREAD_PRIORITY`, `ENABLE`, and `DISABLE [FORCE]` clauses,
`DROP RESOURCE GROUP group_name [FORCE]`, and `SET RESOURCE GROUP` optional
thread-id lists. `RENAME USER` validates comma-separated `old_user TO new_user`
account pairs.
Prepared-statement names are recorded in
`PREPARE`, `EXECUTE`, `DEALLOCATE PREPARE`, and `DROP PREPARE`; those forms
validate handle names, `PREPARE ... FROM` sources, and `EXECUTE ... USING`
user-variable binding lists. Persisted
system-variable targets are recorded for `RESET PERSIST`. Local clone
directories and remote donor endpoints are recorded for `CLONE`. Replication
channel targets are recorded for explicit and default-channel `START`, `STOP`,
`RESET`, and `CHANGE` forms, including legacy `CHANGE MASTER TO` routing.
`CHANGE REPLICATION SOURCE TO` validates nonempty comma-separated option
assignments and final `FOR CHANNEL` clauses. `CHANGE REPLICATION FILTER`
validates documented filter names, parenthesized nonempty filter lists, and
final channel clauses.
Group Replication start/stop statements record the group-replication subsystem
target. XA transaction XID targets are recorded for the
XID-bearing XA statements, while `XA RECOVER` records the XA transaction
collection. Non-XA transaction-control statements record the
transaction object kind for validated `BEGIN`, `BEGIN WORK`,
`START TRANSACTION`, `COMMIT`, bare `ROLLBACK`, and
`SET [GLOBAL | LOCAL | SESSION] TRANSACTION`, while leaving compound
`BEGIN ... END` blocks objectless. `BEGIN` validation rejects extra
transaction tails and MySQL-unsupported compound `BEGIN [NOT] ATOMIC` clauses.
`COMMIT` and non-savepoint
`ROLLBACK` validate MySQL completion-clause ordering for `AND [NO] CHAIN` and
`[NO] RELEASE`, and `START TRANSACTION` validates comma-separated transaction
characteristics. Quoted `HELP` search
topics are recorded as help-topic targets. Component and plugin targets are
recorded for `INSTALL` and `UNINSTALL` administrative statements, with
component URI string lists, optional `INSTALL COMPONENT ... SET` assignments,
and plugin `SONAME` syntax validated.
Local `CLONE` directory targets and remote `CLONE INSTANCE` donor endpoints
are recorded as direct targets.
`STOP` has an explicit statement kind for replication-control statements.
Nameless `ALTER DATABASE` / `ALTER SCHEMA` option forms are recorded with the
database or schema object kind and no invented target name, and their option
lists are syntax-validated, matching MySQL's default-database syntax.
Stored-object `DEFINER = user` clauses are skipped during object scanning so
routine, event, trigger, and view targets are not confused with definer account
tokens.
`CREATE EVENT` and `ALTER EVENT` validate optional definer clauses, event names,
schedule forms, completion behavior, status clauses, comments, renames, and
required or optional `DO` bodies while leaving event-scheduler metadata and
execution semantics to later phases.
`CREATE TRIGGER` validates optional definer and `IF NOT EXISTS`, trigger timing
and event, table target, `FOR EACH ROW`, optional ordering clauses, and nonempty
bodies while leaving trigger metadata and execution to later phases.
Stored `CREATE PROCEDURE` and `CREATE FUNCTION` validate optional definer and
`IF NOT EXISTS`, parameter-list shape, stored-function `RETURNS` types,
documented routine characteristics, and nonempty simple or compound bodies while
leaving routine metadata and execution semantics to later phases.
Compact table creation validation covers `CREATE [TEMPORARY] TABLE ... LIKE`
and query-backed CTAS forms, including optional definition groups, table-option
prefixes, `IGNORE` / `REPLACE`, optional `AS`, and nested parenthesized query
expressions. Full column-definition grammar remains a separate table DDL
milestone.
Base `CREATE TABLE` validation records table targets and rejects missing, empty,
or comma-dangling definition lists while keeping individual column, index,
constraint, option, and partition productions permissive for the later full
table grammar milestone.
`ALTER TABLE` validation records table targets and checks top-level action-list
shape for common action heads, including `ADD`, `DROP`, `CHANGE`, `MODIFY`,
`ALTER`, `RENAME`, `ORDER BY`, online-DDL options, validation options,
partition actions, and table options. Column, index, constraint, partition,
table rebuild, online-DDL, and diagnostic semantics remain later milestones.
Resource group targets are recorded and validated for `CREATE`, `ALTER`,
`DROP`, and `SET RESOURCE GROUP`. Server DDL targets are recorded and validated
for `CREATE SERVER`, `ALTER SERVER`, and `DROP SERVER`. Logfile-group DDL
targets are recorded and validated for create, alter, and drop logfile-group
forms. Tablespace and undo-tablespace DDL targets are recorded and validated for
create, alter, and drop tablespace forms. Spatial reference system DDL records
and validates numeric SRID targets for create and drop forms. Standalone
`DROP INDEX` records the index target and validates the required `ON` table
clause plus optional `ALGORITHM` and `LOCK` clauses. Instance-level `ALTER`,
`LOCK`, `UNLOCK`, `RESTART`, and `SHUTDOWN`
statements are recorded with an object kind but no object-name span. `RESTART`
and `SHUTDOWN` reject body tokens because MySQL accepts only the bare statement
forms. Principal
targets are recorded for
`GRANT ... TO` and `REVOKE ... FROM`, including the first `user@host` span when
present. Those statements validate required target-list markers and documented
grant/revoke tails while leaving privilege graph semantics to later runtime
work. Account and role DDL target spans also preserve `user@host` /
`role@host` syntax for `CREATE`, `ALTER`, `DROP`, and `RENAME` forms, including
corpus-observed trailing-`@` accounts before option clauses. `CREATE USER`
validates optional `IF NOT EXISTS`, account lists with authentication clauses,
default roles, TLS requirements, resource limits, password-management options,
account locks, comments, and attributes. `ALTER USER` validates optional
`IF EXISTS`, account lists with authentication changes, `USER()` password
changes, secondary-password clauses, factor operations, registration clauses,
default roles, TLS requirements, resource limits, password-management options,
account locks, comments, and attributes. `CREATE ROLE` validates optional
`IF NOT EXISTS` plus comma-separated role lists. Account-management `SET`
metadata is recorded for explicit `SET ROLE` and `SET DEFAULT ROLE` role
targets, collection-form `SET ROLE` active-role targets, plus `SET PASSWORD FOR`
and bare current-user `SET PASSWORD` account targets. `SET PASSWORD` validates
optional `FOR` account targets, literal assignment, `TO RANDOM`, `REPLACE`, and
`RETAIN CURRENT PASSWORD` tails. `SET ROLE` validates `DEFAULT`, `NONE`, `ALL`,
`ALL EXCEPT` role lists, and explicit role lists, while `SET DEFAULT ROLE`
validates `NONE`, `ALL`, and role-list defaults with required `TO` account lists,
and variable-assignment `SET` metadata is recorded for explicit user-variable
and system-variable targets, including direct unadorned `SET name = ...`
targets at the statement boundary. User-variable targets include MySQL's quoted
variable-name forms. Structured system-variable targets include the predefined
`default` instance used by key-cache variables. System-variable assignment
targets may use keyword-shaped names when followed by an assignment operator.
Connection character-set `SET NAMES` and `SET CHARACTER SET` forms record the
target character set and validate the shorthand and optional `COLLATE` clause
shape, including following comma-separated `SET` assignments. Savepoint names
are recorded for `SAVEPOINT`,
`RELEASE SAVEPOINT`, and `ROLLBACK [WORK] TO [SAVEPOINT]`; those forms reject
missing names, non-identifier targets, and trailing tokens.
Statements that begin with parenthesized query expressions keep spans anchored
to the opening parenthesis and are classified as `SELECT`, `VALUES`, or `TABLE`
according to the innermost leading query token. Parenthesized `TABLE` forms
preserve the named table target.
Plain `SELECT` and `WITH` query statements are recorded as query targets unless
they expose a more specific `INTO` target.
Standalone `VALUES` statements are recorded as query targets because MySQL
treats them as DML statements that return row sets; parser validation checks
`ROW(...)` constructor-list shape plus optional `ORDER BY`, `LIMIT`, and
set-operation tails.
`DO` statements are recorded as query targets too because they execute
expressions without exposing a table, schema, or administrative target, and
they validate top-level expression-list separator shape.
Stored-program statement heads such as `DECLARE`, cursor operations, `IF`,
`CASE`, loop forms, `LEAVE`, `ITERATE`, and `RETURN` have explicit statement
kinds. `LEAVE` and `ITERATE` validate single label targets, and `RETURN`
validates a nonempty expression tail. Compound-control tokens are structurally
matched for `BEGIN ... END`,
`IF ... END IF`, `CASE ... END CASE`, `LOOP ... END LOOP`,
`REPEAT ... END REPEAT`, and `WHILE ... END WHILE` without misclassifying
`BEGIN WORK`, `IF(...)` expressions, or `IF [NOT] EXISTS` clauses as compound
block starts. Already-matched nested `BEGIN ... END` and `CASE ... END`
expression tokens are ignored by the control matcher so they do not steal the
outer stored-program block ending. Standalone matched compound and flow-control
blocks keep semicolon-delimited bodies in one statement span, with optional end
labels included in that span where MySQL allows them. `IF`, `CASE`, `LOOP`,
`REPEAT`, and `WHILE` validate their top-level clause markers, nonempty
conditions where required, and nonempty branch or loop bodies. Local variable
names are
recorded for ordinary `DECLARE` statements, which validate comma-separated
names, a nonempty type span, and optional nonempty `DEFAULT` expression. Cursor
names are recorded for `DECLARE ... CURSOR`, `OPEN`, `FETCH`, including
optional `NEXT FROM` / `FROM` forms, and `CLOSE`; cursor declarations validate
`CURSOR FOR` plus a nonempty `SELECT` or `WITH` query body. Jump target labels
are recorded for `LEAVE` and `ITERATE` with the same lexical rules as label
declarations. Label declarations are recorded when they prefix
semicolon-delimited MySQL-labeled constructs: `BEGIN`, `LOOP`, `REPEAT`, and
`WHILE`. Label keyword handling is separate from local-variable keyword
handling so unrestricted nonreserved label keywords can be used without quotes
while MySQL's restricted label keywords are not treated as declarations or
targets.
Named condition declarations are recorded for `DECLARE ... CONDITION`, which
validates `CONDITION FOR` condition values. The first handled condition value is
recorded for `DECLARE ... HANDLER`, which validates handler actions, required
`FOR`, condition lists, and a nonempty handler body. `GET DIAGNOSTICS`
validates optional `CURRENT` / `STACKED` area selectors, statement-area and
condition-area assignment lists, variable targets, documented item names, and
literal or variable condition numbers. `... CONDITION` forms also record the
requested diagnostics condition area number. Statement-level `GET DIAGNOSTICS`
records the first explicit assignment target.

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
  lists. `DESCRIBE` and `EXPLAIN` validate direct table/column forms,
  `FORMAT`, `INTO`, `FOR SCHEMA` / `FOR DATABASE`, `FOR CONNECTION`,
  `ANALYZE`, existing `VALUES`, and nested parenthesized query-expression
  shells. Target metadata records direct table forms, `EXPLAIN ... INTO`
  user-variable targets, `EXPLAIN ... FOR CONNECTION` targets, and explainable
  statement forms as query targets, without modeling optimizer plans or result
  rows.
  `CACHE INDEX` and `LOAD INDEX INTO CACHE` validate table/index lists,
  optional `PARTITION (...)` and `INDEX` / `KEY` groups, key-cache targets,
  and `LOAD INDEX ... IGNORE LEAVES` modifiers.
  `LOCK TABLES` validates comma-separated table entries with optional aliases
  and MySQL 8.4 `READ [LOCAL]` / `WRITE` modes, while preserving the
  corpus-observed legacy `LOW_PRIORITY WRITE` form for later runtime
  diagnostics.
  `UNLOCK TABLES` metadata records the table object kind without a name because
  the statement releases the session's table locks rather than naming tables;
  parser validation rejects named unlock targets.
  `IMPORT TABLE` metadata records only the first string SDI file target and
  validates the `IMPORT TABLE FROM` string-file list shape.
  `CALL` metadata records the procedure name and validates optional
  parenthesized argument-list shape, but does not classify parameters or
  OUT/INOUT binding semantics.
  `SIGNAL` and `RESIGNAL` metadata records only the explicit SQLSTATE literal
  or named condition value. Bare `RESIGNAL` and `RESIGNAL SET ...` forms remain
  objectless. Parser validation checks the documented condition-value shape and
  signal-information item assignment lists while leaving handler context and
  diagnostics-area mutation to runtime.
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
  Instance metadata records only the instance-level command surface, while
  `ALTER INSTANCE` validates documented redo-log, key-rotation, TLS reload,
  and keyring reload action shapes.
  `KILL` metadata records single processlist-id expressions and distinguishes
  `KILL QUERY` from connection termination. `KILL` also has targeted syntax
  validation for missing targets and comma-separated target lists. `BINLOG`
  validates and records the single string event payload only.
  `CLONE` metadata records local clone directories or remote donor endpoints
  and validates local directory, remote endpoint, password, optional
  data-directory, and optional SSL-requirement clause shape; plugin
  availability, privilege checks, copy behavior, SSL enforcement, and restart
  side effects remain runtime work.
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
  table collection for unnamed table-cache forms. Parser validation covers
  table-name lists, `WITH READ LOCK`, and named-table `FOR EXPORT` tails.
  `FLUSH RELAY LOGS` records explicit channel names or the default
  replication-channel collection when no channel is named and validates string
  channel names. Other documented global `FLUSH` options record their object
  kinds after optional `LOCAL` / `NO_WRITE_TO_BINLOG` modifiers, validate
  comma-separated option lists, and preserve legacy `FLUSH HOSTS` routing.
  Table-maintenance metadata records only the first concrete table target and
  validates table lists, optional logging modifiers, `CHECK TABLE` options,
  `CHECKSUM TABLE` options, `REPAIR TABLE` options, and `ANALYZE TABLE`
  histogram update/drop clause shape.
  `RESET PERSIST` metadata records the first variable name when present,
  validates `IF EXISTS` only with a variable name, and records bare full
  persisted-variable reset forms as unnamed system-variable collection targets.
  `RESET BINARY LOGS AND GTIDS` metadata records the binary-log collection,
  validates optional numeric file-index numbers, and accepts comma-separated
  reset options.
  Replication metadata records explicit `FOR CHANNEL` names and default-channel
  operations; `START REPLICA` validates optional thread-type lists, `UNTIL`
  shapes, connection options, and final channel clauses, and `STOP REPLICA`
  validates optional thread-type lists and channel clause shape, but neither
  form classifies thread lists, source options, filters, or replica runtime
  state.
  Group Replication metadata records the subsystem object kind, validates
  `START GROUP_REPLICATION` credential option shape, and validates bare
  `STOP GROUP_REPLICATION`, but does not model credential storage, distributed
  recovery, group membership, or timeout behavior.
  XA metadata records only the first XID token for XID-bearing statements and
  the XA transaction collection for `XA RECOVER`; it validates XID shape,
  `XA START` / `XA BEGIN` options, `XA END` suspend options, `XA COMMIT ONE
  PHASE`, and `XA RECOVER CONVERT XID`, but does not model XA state.
  `HELP` metadata records and validates single string-literal search topics
  plus corpus-observed unquoted identifier/keyword topics.
  Instance lifecycle metadata records only the instance object kind, not
  privilege, connection-loss, shutdown, restart, keyring, TLS, or redo-log
  semantics.
  `SHOW` statement validation consumes the documented parser forms for create,
  grant, table/index/schema collection, character-set/collation, routine,
  engine, variable/status, diagnostics, profile, binary-log, relay-log, and
  replica-status statements. It rejects missing SHOW targets, dangling
  `LIKE`, incomplete `FOR CHANNEL`, missing engine diagnostic types,
  nonnumeric `SHOW PROFILE ... FOR QUERY` ids, non-`SELECT`
  `SHOW PARSE_TREE` payloads, and unsupported forms such as
  `SHOW CHARACTERISTICS`.
  `SHOW GRANTS` accepts documented `USING` role tails and corpus-observed
  trailing-`@` account names. `SHOW PROFILE` metadata records query targets for
  bare forms, numeric `FOR QUERY` ids, and accepts `LIMIT` / `OFFSET` profile
  tails in the corpus-observed order variants.
  `SHOW PARSE_TREE` metadata records documented `SELECT` payloads as query
  targets, while runtime JSON generation remains analysis/runtime work.
  `SHOW ENGINE` metadata records the named engine for `STATUS`, `MUTEX`, and
  runtime-accepted `LOGS` forms and rejects missing diagnostic-type tails.
- Account and principal metadata records the first syntactic account or role
  target only. It does not yet resolve roles, dynamic privileges, multiple
  accounts, proxy grants, account-name normalization, rename destinations, or
  implicit current-user/current-role targets in `SET` statements.
- Savepoint metadata records the named savepoint only. Bare `ROLLBACK` records
  the transaction object kind, `ROLLBACK [WORK] TO [SAVEPOINT]` records the
  savepoint name, and non-savepoint `RELEASE` forms are rejected.
- Parenthesized query-expression classification only identifies the leading
  query statement kind; it does not build the query-expression tree.
- Stored-program control matching and validators record token pairs and clause
  shape only; they do not yet validate label binding, declaration ordering,
  cursor scope, or control-flow semantics.
- Cursor metadata records the first cursor handle, validates `OPEN` and `CLOSE`
  handle-only forms, and validates `FETCH` direction/from prefixes plus
  nonempty target lists. It does not yet validate declaration scope, result
  shape, target assignment, or cursor lifecycle.
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
  `... CONDITION n` forms or the first statement-level assignment target.
  Remaining item-list assignments stay body tokens after shape validation.
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
