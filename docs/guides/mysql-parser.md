# MySQL Parser

Build the prototype parser:

```sh
make
```

Parse one statement:

```sh
bin/mylite-parse "SELECT 1"
```

The CLI reports statement kind plus token and byte spans:

```text
ok statements=1 kinds=select[1:2,0:8]/query
```

DDL, table-maintenance, DML table statements, and direct utility targets include
a target object kind when the prototype can identify one. If a first target name
is found, the CLI prints the exact source slice after the object kind:

```text
ok statements=1 kinds=create[1:12,0:44]/table:`db`.`t`
```

For DML, the reported table is the first syntactic target table, including
`WITH`-prefixed `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` statements:

```text
ok statements=1 kinds=update[1:13,0:38]/table:wt
```

`INSERT` and `REPLACE` validate their target table, optional partition and
column lists, `VALUES` / `VALUE`, `SET`, and query-backed source forms.
`INSERT` also validates row aliases and `ON DUPLICATE KEY UPDATE` assignment
tails while leaving expression semantics and row changes to later phases.

Derived-table-leading `UPDATE` references skip the derived alias and report the
first concrete table before `SET`:

```text
ok statements=1 kinds=update[1:15,0:43]/table:t3
```

Executable MySQL comments are tokenized only when ungated or compatible with
the MySQL 8.4.9 target version:

```text
ok statements=1 kinds=set[1:4,9:18]/user_variable:@ok
```

Table-maintenance statements validate table lists, logging modifiers, supported
maintenance options, and `ANALYZE TABLE` histogram clause shape, then report
the first concrete table target:

```text
ok statements=1 kinds=analyze[1:3,0:15]/table:t
```

```text
ok statements=1 kinds=drop[1:3,0:14]/table:t1
```

`DROP TABLE` and `DROP VIEW` validate optional `IF EXISTS`, comma-separated
name lists, and `RESTRICT` / `CASCADE` tails. `DROP TABLE` also accepts
`TEMPORARY` and corpus-observed `TABLES` forms.
`CREATE DATABASE` and `CREATE SCHEMA` validate optional `IF NOT EXISTS` plus
character-set, collation, and encryption options. `ALTER DATABASE` and
`ALTER SCHEMA` validate explicit or nameless option forms, including
character-set, collation, encryption, and `READ ONLY`. `DROP DATABASE` and
`DROP SCHEMA` validate optional `IF EXISTS` with a single unqualified schema
target.
Unambiguous `CREATE FUNCTION` loadable-function forms validate documented
`RETURNS` and `SONAME` clauses separately from stored routine creation.
Stored `CREATE PROCEDURE` and `CREATE FUNCTION` forms validate optional definer
and `IF NOT EXISTS`, parameter-list shape, stored-function `RETURNS` types,
documented routine characteristics, and nonempty bodies without executing or
analyzing routine semantics yet.
`ALTER FUNCTION` and `ALTER PROCEDURE` validate documented routine
characteristics without changing parameters or bodies.
Compact `CREATE TABLE` validation covers `LIKE` and query-backed CTAS forms,
including temporary destinations, optional definition groups, table-option
prefixes, `IGNORE` / `REPLACE`, optional `AS`, and nested parenthesized query
expressions. Full column-definition grammar remains separate.
Base `CREATE TABLE` forms validate the statement head and nonempty top-level
definition-list structure, including quoted table identifiers, while keeping
individual column, index, constraint, option, and partition productions
permissive for the full table grammar milestone.
`ALTER TABLE` records table targets and validates top-level action-list
structure for common action heads, including column, index, constraint, rename,
ordering, algorithm, lock, validation, partition, and table-option forms.
Detailed table rebuild and metadata semantics are still outside this parser
prototype.
`CREATE INDEX` validates standalone index modifiers, key-part-list shape,
documented option clauses, and corpus-observed legacy `TYPE` / `RTREE` index
type spellings while table-level index definitions stay with `CREATE TABLE`
grammar work.
`CREATE VIEW` and `ALTER VIEW` validate view header clauses, optional
column-list groups, required `AS` query bodies, and trailing check-option
clauses while leaving view dependency analysis to later phases.
`CREATE EVENT` and `ALTER EVENT` validate optional definer clauses, event names,
schedule forms, completion behavior, status clauses, comments, renames, and
required or optional `DO` bodies while leaving scheduler metadata and execution
to later phases.
`CREATE TRIGGER` validates optional definer and `IF NOT EXISTS`, trigger timing
and event, table targets, `FOR EACH ROW`, optional ordering clauses, and
nonempty bodies while leaving trigger metadata and execution to later phases.
`DROP EVENT`, `DROP PROCEDURE`, `DROP FUNCTION`, and `DROP TRIGGER` validate
optional `IF EXISTS` with an identifier-like target.
`CREATE SERVER` validates `FOREIGN DATA WRAPPER` and documented server
`OPTIONS`, `ALTER SERVER` validates documented `OPTIONS`, `CREATE LOGFILE
GROUP` and `ALTER LOGFILE GROUP` validate `ADD UNDOFILE`, size/comment options,
`WAIT`, and required storage engines, `CREATE TABLESPACE`, `CREATE UNDO
TABLESPACE`, `ALTER TABLESPACE`, and `ALTER UNDO TABLESPACE` validate their
documented datafile, size, logfile-group, encryption, state, storage-engine, and
engine-attribute clauses, `CREATE SPATIAL REFERENCE SYSTEM` validates numeric
SRIDs and documented attributes, and `DROP SERVER`, `DROP SPATIAL REFERENCE
SYSTEM`, `DROP LOGFILE GROUP`, `DROP TABLESPACE`, and `DROP UNDO TABLESPACE`
validate their required object names and low-level tails, including optional
`IF EXISTS`, numeric SRIDs, and `ENGINE [=] name` where MySQL syntax or the corpus
requires it.
`DROP USER` and `DROP ROLE` validate optional `IF EXISTS` and account/role
lists. Resource-group statements validate `CREATE` type/options, `ALTER`
options including `DISABLE FORCE`, `DROP ... [FORCE]`, and `SET ... [FOR
thread_id [, ...]]` forms. `RENAME USER` validates comma-separated
`old_user TO new_user` account pairs.

Standalone `DROP INDEX` records the index target and validates the required
`ON` table clause plus optional `ALGORITHM` and `LOCK` clauses:

```text
ok statements=1 kinds=drop[1:5,0:17]/index:i
```

`RENAME TABLE` records the first source table and validates comma-separated
`old TO new` rename pairs:

```text
ok statements=1 kinds=rename[1:5,0:23]/table:old
```

`SELECT` and `WITH` query statements expose the query object kind unless a more
specific `INTO` target is present:

```text
ok statements=1 kinds=select[1:2,0:8]/query
```

Standalone `VALUES` statements expose the query object kind and validate row
constructor-list, ordering, and limit shape:

```text
ok statements=1 kinds=values[1:10,0:21]/query
```

`DO` expression statements validate top-level expression-list separators and
expose the query object kind:

```text
ok statements=1 kinds=do[1:4,0:8]/query
```

Direct and parenthesized `TABLE` statements preserve their table target:

```text
ok statements=1 kinds=table[1:6,0:16]/table:`db`.`t`
```

Qualified names preserve reserved words after the qualifier dot:

```text
ok statements=1 kinds=create[1:9,0:31]/table:db.select
```

Digit-leading identifiers that are not purely numeric are preserved as object
names:

```text
ok statements=1 kinds=create[1:7,0:26]/table:1abc
```

Direct utility targets are reported for statements such as `USE`, `TABLE`,
`TRUNCATE`, `HANDLER`, `IMPORT TABLE`, `CALL`, direct `DESCRIBE` / `EXPLAIN`
table forms, `EXPLAIN ... FOR CONNECTION`, `LOAD ... INTO TABLE`, `CACHE INDEX`,
`LOAD INDEX INTO CACHE`, table-lock statements, and unambiguous `SHOW` table/schema
forms. Prepared statement handles and component/plugin administration targets
are reported with their own object kinds. Low-level storage DDL reports
tablespace and undo-tablespace targets separately:

```text
ok statements=1 kinds=use[1:2,0:7]/database:app
```

```text
ok statements=1 kinds=unlock[1:2,0:13]/table
```

`HANDLER` accepts the documented `OPEN`, `READ`, and `CLOSE` syntax, including
optional open aliases, indexed reads, key-comparison value lists, and optional
`WHERE` / `LIMIT` tails. The parser records the table or open-handler name;
storage-engine handler cursor state is not implemented.

`CACHE INDEX` and `LOAD INDEX INTO CACHE` accept MySQL table/index-list
syntax, including optional partition and index-name groups. `CACHE INDEX`
also validates the `IN key_cache_name` clause, while `LOAD INDEX` accepts
`IGNORE LEAVES`.
`LOAD DATA` and `LOAD XML` validate their import statement skeletons, including
string `INFILE` inputs, `INTO TABLE` targets, optional `LOCAL`, duplicate-key,
character-set, row-skip, column/user-variable list, and `SET` clauses. `LOAD
DATA` additionally validates partition and field/line clauses; `LOAD XML`
validates `ROWS IDENTIFIED BY`.

`LOCK TABLES` accepts MySQL 8.4 table-lock lists with optional aliases,
`READ [LOCAL]` or `WRITE` modes, and the corpus-observed legacy
`LOW_PRIORITY WRITE` form for later diagnostics. `UNLOCK TABLES` is
intentionally objectless because it releases the session's current table locks.

```text
ok statements=1 kinds=import[1:4,0:30]/sdi_file:'/tmp/a.sdi'
```

```text
ok statements=1 kinds=call[1:4,0:12]/procedure:p
```

```text
ok statements=1 kinds=explain[1:4,0:26]/connection:123
```

```text
ok statements=1 kinds=prepare[1:4,0:22]/prepared_statement:stmt
```

```text
ok statements=1 kinds=install[1:5,0:30]/plugin:p
```

```text
ok statements=1 kinds=create[1:7,0:50]/undo_tablespace:uts
```

Nameless default-database option forms keep the database object kind without
inventing a target name, and database option lists are syntax-validated:

```text
ok statements=1 kinds=alter[1:5,0:36]/database
```

Stored-object definers are skipped when selecting the DDL target:

```text
ok statements=1 kinds=create[1:11,0:54]/procedure:p
```

Grant and revoke principal targets preserve the first account span:

```text
ok statements=1 kinds=grant[1:10,0:31]/user:'u'@'h'
```

`GRANT` and `REVOKE` validate required `TO` / `FROM` target lists, support
`CURRENT_USER()` principals, and check documented grant/revoke tail shapes such
as `WITH GRANT OPTION`, `WITH ADMIN OPTION`, `AS ... WITH ROLE`, and `IGNORE
UNKNOWN USER`.

Account and role DDL preserve the first account-style target span too.
`CREATE USER` validates account lists, authentication clauses, default roles,
TLS, resource, password-management, account-locking, comment, and attribute
clauses. `ALTER USER` validates the same global account options plus
`IF EXISTS`, `USER()` password changes, secondary-password clauses, factor
operations, registration clauses, and default-role assignment. `CREATE ROLE`
validates optional `IF NOT EXISTS` and comma-separated role lists:

```text
ok statements=1 kinds=create[1:5,0:19]/user:'u'@'h'
```

Account-management `SET` statements expose the first explicit role or user
target. `SET ROLE` validates collection and role-list forms, and
`SET DEFAULT ROLE` validates role defaults with required `TO` account lists.
`SET PASSWORD` validates optional `FOR` account targets, literal assignment,
`TO RANDOM`, `REPLACE`, and `RETAIN CURRENT PASSWORD` tails. Bare
`SET PASSWORD` statements are reported as current-user targets without an
object-name span:

```text
ok statements=1 kinds=set[1:3,0:10]/role:r
ok statements=1 kinds=set[1:4,0:18]/user
ok statements=1 kinds=set[1:3,0:12]/role
```

Explicit variable-assignment targets are also reported for `SELECT ... INTO`,
`TABLE ... INTO`, `SET`, and statement-level `GET DIAGNOSTICS` forms:

```text
ok statements=1 kinds=select[1:6,0:23]/user_variable:@x
```

```text
ok statements=1 kinds=table[1:4,0:15]/user_variable:@x
```

```text
ok statements=1 kinds=set[1:4,0:31]/system_variable:@@session.sql_mode
```

```text
ok statements=1 kinds=set[1:9,0:49]/system_variable:keycache1.key_buffer_size
```

```text
ok statements=1 kinds=set[1:9,0:47]/system_variable:default.key_buffer_size
```

```text
ok statements=1 kinds=set[1:4,0:18]/system_variable:autocommit
```

```text
ok statements=1 kinds=set[1:4,0:17]/user_variable:@'my-var'
```

```text
ok statements=1 kinds=set[1:4,0:17]/user_variable:@iv
```

```text
ok statements=1 kinds=set[1:4,0:19]/system_variable:sql_log_bin
```

```text
ok statements=1 kinds=set[1:5,0:18]/system_variable:flush
```

```text
ok statements=1 kinds=get[1:5,0:27]/user_variable:@n
```

Connection character-set SET forms expose the requested character set and
validate shorthand statement shape, including following comma-separated `SET`
assignments:

```text
ok statements=1 kinds=set[1:5,0:44]/character_set:utf8mb4
```

SELECT and TABLE file-export forms expose the literal export target:

```text
ok statements=1 kinds=select[1:7,0:41]/outfile:'/tmp/x.csv'
```

```text
ok statements=1 kinds=table[1:5,0:33]/outfile:'/tmp/t.tsv'
```

SHOW account-introspection forms preserve account spans:

```text
ok statements=1 kinds=show[1:6,0:24]/user:'u'@'h'
```

Documented current-user function forms keep the full function-call span:

```text
ok statements=1 kinds=show[1:6,0:31]/user:CURRENT_USER()
```

```text
ok statements=1 kinds=show[1:2,0:11]/user
```

SHOW CREATE forms expose the object being recreated:

```text
ok statements=1 kinds=show[1:7,0:37]/database:db
```

```text
ok statements=1 kinds=show[1:4,0:20]/procedure:p
```

SHOW table-detail forms expose the inspected table:

```text
ok statements=1 kinds=show[1:4,0:19]/table:t
```

```text
ok statements=1 kinds=show[1:6,0:38]/table:`db`.`v`
```

Schema-scoped SHOW forms expose explicit `FROM` or `IN` database targets:

```text
ok statements=1 kinds=show[1:5,0:25]/database:db
```

Bare schema collection SHOW forms expose the listed object kind:

```text
ok statements=1 kinds=show[1:2,0:11]/table
```

`LIKE` filters expose the filtered object kind when the pattern maps to names:

```text
ok statements=1 kinds=show[1:4,0:24]/table:'wp_%'
```

```text
ok statements=1 kinds=show[1:4,0:27]/event:'e_%'
```

`SHOW TRIGGERS LIKE` filters by table name:

```text
ok statements=1 kinds=show[1:4,0:29]/table:'wp_%'
```

Routine-code SHOW forms expose routine names:

```text
ok statements=1 kinds=show[1:4,0:20]/function:f
```

Routine-status SHOW forms expose routine collections or patterns:

```text
ok statements=1 kinds=show[1:5,0:30]/function:'f%'
```

```text
ok statements=1 kinds=show[1:3,0:21]/procedure
```

SHOW collection forms expose collection object kinds:

```text
ok statements=1 kinds=show[1:2,0:15]/privilege
```

```text
ok statements=1 kinds=show[1:3,0:20]/engine
```

SHOW ENGINE diagnostics expose the engine name:

```text
ok statements=1 kinds=show[1:4,0:25]/engine:InnoDB
```

```text
ok statements=1 kinds=show[1:4,0:24]/engine:NDB
```

```text
ok statements=1 kinds=show[1:4,0:20]/engine:csv
```

SHOW PROFILE exposes explicit query ids:

```text
ok statements=1 kinds=show[1:5,0:24]/query:1
```

```text
ok statements=1 kinds=show[1:2,0:12]/query
```

SHOW PARSE_TREE exposes documented SELECT payloads as query targets:

```text
ok statements=1 kinds=show[1:4,0:24]/query
```

EXPLAIN and DESCRIBE expose explainable statements as query targets:

```text
ok statements=1 kinds=explain[1:3,0:16]/query
```

EXPLAIN INTO exposes the user-variable output target:

```text
ok statements=1 kinds=explain[1:8,0:39]/user_variable:@plan
```

KILL statements expose processlist-id expressions as connection or query
targets:

```text
ok statements=1 kinds=kill[1:2,0:8]/connection:123
ok statements=1 kinds=kill[1:3,0:14]/query:123
ok statements=1 kinds=kill[1:3,0:21]/query:@thread_id
ok statements=1 kinds=kill[1:4,0:20]/connection:CONNECTION_ID()
```

Binary log statements expose explicit log-file targets:

```text
ok statements=1 kinds=show[1:7,0:41]/binary_log:'bin.000001'
```

Binary log collection statements expose the binary-log object kind:

```text
ok statements=1 kinds=show[1:4,0:22]/binary_log
```

```text
ok statements=1 kinds=show[1:3,0:18]/binary_log
```

Relay log event statements expose explicit relay-log files, bare relay-log
targets, or a channel when no file is named:

```text
ok statements=1 kinds=show[1:7,0:43]/relay_log:'relay.000001'
```

```text
ok statements=1 kinds=show[1:3,0:20]/relay_log
```

SHOW REPLICA STATUS exposes explicit channel names or the replica-channel
collection, including the deprecated `SHOW SLAVE STATUS` alias:

```text
ok statements=1 kinds=show[1:6,0:36]/replication_channel:'ch'
```

```text
ok statements=1 kinds=show[1:3,0:19]/replication_channel
```

SHOW REPLICAS exposes the replica-channel collection, including the deprecated
`SHOW SLAVE HOSTS` alias:

```text
ok statements=1 kinds=show[1:2,0:13]/replication_channel
```

```text
ok statements=1 kinds=show[1:3,0:16]/replication_channel
```

Replication control statements expose explicit channel names or the default
replication-channel target, including legacy `CHANGE MASTER TO` routing:

```text
ok statements=1 kinds=change[1:6,0:32]/replication_channel
```

BINLOG statements expose their event payload string:

```text
ok statements=1 kinds=binlog[1:2,0:12]/binary_log_event:'abc'
```

SHOW BINARY LOGS exposes the binary-log collection, including the legacy
`SHOW MASTER LOGS` spelling:

```text
ok statements=1 kinds=show[1:3,0:16]/binary_log
```

PURGE BINARY LOGS validates `TO` string log-file targets and `BEFORE`
expression forms, including the legacy `MASTER` spelling, and exposes named or
collection binary-log targets:

```text
ok statements=1 kinds=purge[1:7,0:30]/binary_log
```

FLUSH TABLES validates table-name lists, `WITH READ LOCK`, and named-table
`FOR EXPORT` tails, then exposes the first table target or the table
collection:

```text
ok statements=1 kinds=flush[1:3,0:14]/table:t
```

```text
ok statements=1 kinds=flush[1:2,0:12]/table
```

FLUSH RELAY LOGS validates string channel names and exposes explicit channel
names or the default channel collection for bare forms:

```text
ok statements=1 kinds=flush[1:6,0:33]/replication_channel:'ch'
```

```text
ok statements=1 kinds=flush[1:3,0:16]/replication_channel
```

FLUSH collection forms validate comma-separated options and expose clear global
targets:

```text
ok statements=3 kinds=flush[1:3,0:17]/binary_log,flush[5:6,19:35]/privilege,flush[8:9,37:49]/status_variable
```

Other documented FLUSH options, plus legacy `FLUSH HOSTS` routing for
compatibility, expose their global targets:

```text
ok statements=3 kinds=flush[1:3,0:16]/error_log,flush[5:6,18:29]/host_cache,flush[8:9,31:51]/user_resource
```

RESET PERSIST validates bare full-reset operations, explicit persisted
system-variable targets, and `IF EXISTS` only when a variable name follows:

```text
ok statements=1 kinds=reset[1:3,0:29]/system_variable:max_connections
ok statements=1 kinds=reset[1:2,0:13]/system_variable
```

RESET accepts comma-separated reset options. RESET BINARY LOGS AND GTIDS
validates optional numeric file indexes and exposes the binary-log collection.
The legacy `RESET MASTER` spelling is also classified as binary-log metadata so
runtime diagnostics can route the unsupported form deliberately:

```text
ok statements=1 kinds=reset[1:5,0:27]/binary_log
```

```text
ok statements=1 kinds=reset[1:2,0:12]/binary_log
```

CLONE validates local and remote clone clause shape, then exposes local
directory and remote donor targets:

```text
ok statements=2 kinds=clone[1:6,0:41]/directory:'/tmp/clone',clone[8:17,43:95]/server:user@host:3306
```

STOP replication-control forms are classified as `stop`:

```text
ok statements=1 kinds=stop[1:2,0:12]
```

Server lifecycle statements expose the instance target. `ALTER INSTANCE`
also validates documented redo-log, key-rotation, TLS reload, and keyring
reload action shapes:

```text
ok statements=1 kinds=restart[1:1,0:7]/instance
```

Replication-control statements expose explicit and default channel targets.
`START REPLICA` validates thread-type lists, supported `UNTIL` forms,
connection options, and final channel clauses:

```text
ok statements=1 kinds=start[1:5,0:30]/replication_channel:'ch'
```

```text
ok statements=4 kinds=start[1:2,0:13]/replication_channel,stop[4:5,15:27]/replication_channel,reset[7:8,29:42]/replication_channel,change[10:16,44:88]/replication_channel
```

Group Replication start and stop statements expose the group-replication target;
`START GROUP_REPLICATION` validates credential-option shape, and
`STOP GROUP_REPLICATION` is validated as a bare statement:

```text
ok statements=2 kinds=start[1:2,0:23]/group_replication,stop[4:5,25:47]/group_replication
```

Transaction-control statements validate supported transaction modifiers and
expose the transaction object kind:

```text
ok statements=1 kinds=start[1:4,0:28]/transaction
```

Scoped `SET TRANSACTION` forms do the same:

```text
ok statements=1 kinds=set[1:5,0:31]/transaction
```

XA transaction statements validate XID and action-option shape, then expose XID
targets:

```text
ok statements=1 kinds=xa[1:3,0:12]/xa_transaction:'x'
```

```text
ok statements=1 kinds=xa[1:2,0:10]/xa_transaction
```

HELP validates and exposes string-literal help-topic searches:

```text
ok statements=1 kinds=help[1:2,0:15]/help_topic:'contents'
```

Corpus-observed unquoted identifier and keyword topics are accepted too:

```text
ok statements=1 kinds=help[1:3,0:17]/help_topic:CREATE TABLE
```

SHOW variable statements expose system-variable or status-variable targets:

```text
ok statements=1 kinds=show[1:4,0:32]/system_variable:'autocommit'
```

```text
ok statements=1 kinds=show[1:4,0:29]/status_variable:'Com_select'
```

SHOW character-set statements expose character-set or collation targets:

```text
ok statements=1 kinds=show[1:5,0:31]/character_set:'utf8%'
```

```text
ok statements=1 kinds=show[1:4,0:40]/collation:'utf8mb4_0900_ai_ci'
```

SHOW diagnostics statements expose the diagnostics-area object kind:

```text
ok statements=1 kinds=show[1:4,0:21]/diagnostics_area
```

```text
ok statements=1 kinds=show[1:6,0:20]/diagnostics_area
```

SHOW database statements expose the database collection or pattern:

```text
ok statements=1 kinds=show[1:4,0:25]/database:'wp%'
```

SHOW collection statements expose the collection object kind:

```text
ok statements=1 kinds=show[1:2,0:12]/engine
```

```text
ok statements=1 kinds=show[1:2,0:12]/plugin
```

```text
ok statements=1 kinds=show[1:3,0:21]/connection
```

SIGNAL and RESIGNAL expose explicit SQLSTATE values or named conditions:

```text
ok statements=1 kinds=signal[1:3,0:23]/sqlstate:'45000'
```

```text
ok statements=1 kinds=resignal[1:2,0:21]/condition:my_condition
```

DECLARE CONDITION exposes the declared condition name:

```text
ok statements=1 kinds=declare[1:6,0:43]/condition:cond
```

DECLARE HANDLER exposes the first handled condition value:

```text
ok statements=1 kinds=declare[1:9,0:48]/condition:SQLEXCEPTION
```

GET DIAGNOSTICS CONDITION exposes the requested condition-area index:

```text
ok statements=1 kinds=get[1:8,0:62]/diagnostics_condition:1
```

Resource group administration exposes the group name:

```text
ok statements=1 kinds=create[1:7,0:36]/resource_group:rg
```

Low-level DDL targets such as server and logfile-group names are reported too,
with `CREATE SERVER`, `ALTER SERVER`, `CREATE LOGFILE GROUP`, and
`ALTER LOGFILE GROUP` validating their documented option lists. Tablespace and
undo-tablespace DDL targets are also reported and syntax-validated, as are
spatial reference system SRID targets:

```text
ok statements=1 kinds=create[1:12,0:61]/server:s
```

Instance-level commands report an object kind without a name span:

```text
ok statements=1 kinds=lock[1:4,0:24]/instance
```

Transaction savepoint statements expose the savepoint handle:

```text
ok statements=1 kinds=savepoint[1:2,0:11]/savepoint:s
```

```text
ok statements=1 kinds=rollback[1:3,0:13]/savepoint:s
```

Stored-program local variable declarations expose the first declared variable:

```text
ok statements=1 kinds=declare[1:3,0:13]/local_variable:x
```

Parenthesized query expressions keep their opening-parenthesis span and are
classified by the leading query token:

```text
ok statements=1 kinds=select[1:7,0:25]
```

Dump tokens:

```sh
bin/mylite-parse --tokens "SELECT @a, ?"
```

Balanced groups and compound blocks print `match` lines in token mode:

```text
match 2 4
match 4 2
```

Stored-program control blocks are matched as structural tokens as well:

```text
match 7 21
match 21 7
```

Semicolon-delimited compound and flow-control bodies stay in one statement span:

```text
ok statements=1 kinds=if[1:7,0:26]
```

```text
ok statements=1 kinds=begin[1:6,0:23]
```

Cursor operations expose their cursor handle and validate handle-only
`OPEN` / `CLOSE` forms plus `FETCH` target-list shape:

```text
ok statements=1 kinds=open[1:2,0:6]/cursor:c
```

```text
ok statements=1 kinds=fetch[1:6,0:24]/cursor:c
```

`LEAVE` and `ITERATE` expose their target label:

```text
ok statements=1 kinds=leave[1:2,0:10]/label:done
```

Label targets use the same keyword rules as label declarations.

Leading labels on `BEGIN`, `LOOP`, `REPEAT`, and `WHILE` are reported too:

```text
ok statements=1 kinds=loop[1:8,0:36]/label:open
```

Unrestricted nonreserved label keywords are accepted without quotes; reserved
or label-restricted keywords require quoted identifiers.

Keyword lookup is table-driven and must remain sorted:

```sh
python3 tests/check_keywords.py
```

Some nonreserved MySQL words are emitted as keyword tokens for analyzer
fidelity but still remain usable in target-name spans:

```text
ok statements=1 kinds=create[1:7,0:26]/table:json
```

```text
ok statements=1 kinds=create[1:7,0:27]/table:clone
```

`BEGIN` and `END` are still recognized as control tokens except in object-name
positions:

```text
ok statements=1 kinds=create[1:7,0:27]/table:begin
```

Run smoke tests:

```sh
make smoke
```

Run the WordPress MySQL query corpus:

```sh
make corpus
```

The corpus target downloads the CSV fixture into `build/corpus/`, which is
ignored by Git. The current corpus reader handles the fixture's MySQL-style
backslash-escaped double quotes and verifies 69,577 records.
