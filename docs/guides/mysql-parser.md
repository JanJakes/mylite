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
ok statements=1 kinds=select[1:2,0:8]
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

Table-maintenance statements also report the first concrete table target:

```text
ok statements=1 kinds=analyze[1:3,0:15]/table:t
```

Standalone `VALUES` statements expose the query object kind:

```text
ok statements=1 kinds=values[1:10,0:21]/query
```

Direct and parenthesized `TABLE` statements preserve their table target:

```text
ok statements=1 kinds=table[1:6,0:16]/table:`db`.`t`
```

Direct utility targets are reported for statements such as `USE`, `TABLE`,
`TRUNCATE`, `HANDLER`, `IMPORT TABLE`, `CALL`, direct `DESCRIBE` / `EXPLAIN`
table forms, `EXPLAIN ... FOR CONNECTION`, `LOAD ... INTO TABLE`, `CACHE INDEX`,
`LOAD INDEX INTO CACHE`, table-lock statements, and unambiguous `SHOW` table/schema
forms. Prepared statement handles and component/plugin administration targets
are reported with their own object kinds:

```text
ok statements=1 kinds=use[1:2,0:7]/database:app
```

```text
ok statements=1 kinds=unlock[1:2,0:13]/table
```

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

Grant and revoke principal targets preserve the first account span:

```text
ok statements=1 kinds=grant[1:10,0:31]/user:'u'@'h'
```

Account and role DDL preserve the first account-style target span too:

```text
ok statements=1 kinds=create[1:5,0:19]/user:'u'@'h'
```

Account-management `SET` statements expose the first explicit role or user
target:

```text
ok statements=1 kinds=set[1:3,0:10]/role:r
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
ok statements=1 kinds=set[1:4,0:19]/system_variable:sql_log_bin
```

```text
ok statements=1 kinds=get[1:5,0:27]/user_variable:@n
```

Connection character-set SET forms expose the requested character set:

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

KILL statements expose a numeric processlist id as a connection target:

```text
ok statements=1 kinds=kill[1:3,0:14]/connection:123
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
collection:

```text
ok statements=1 kinds=show[1:6,0:36]/replication_channel:'ch'
```

```text
ok statements=1 kinds=show[1:3,0:19]/replication_channel
```

SHOW REPLICAS exposes the replica-channel collection:

```text
ok statements=1 kinds=show[1:2,0:13]/replication_channel
```

BINLOG statements expose their event payload string:

```text
ok statements=1 kinds=binlog[1:2,0:12]/binary_log_event:'abc'
```

PURGE BINARY LOGS exposes named or collection binary-log targets:

```text
ok statements=1 kinds=purge[1:7,0:30]/binary_log
```

FLUSH TABLES exposes the first table target or the table collection:

```text
ok statements=1 kinds=flush[1:3,0:14]/table:t
```

```text
ok statements=1 kinds=flush[1:2,0:12]/table
```

FLUSH RELAY LOGS exposes explicit channel names:

```text
ok statements=1 kinds=flush[1:6,0:33]/replication_channel:'ch'
```

FLUSH collection forms expose clear global targets:

```text
ok statements=3 kinds=flush[1:3,0:17]/binary_log,flush[5:6,19:35]/privilege,flush[8:9,37:49]/status_variable
```

Other documented FLUSH options expose their global targets:

```text
ok statements=3 kinds=flush[1:3,0:16]/error_log,flush[5:6,18:29]/host_cache,flush[8:9,31:51]/user_resource
```

RESET PERSIST exposes explicit persisted system-variable targets:

```text
ok statements=1 kinds=reset[1:3,0:29]/system_variable:max_connections
```

RESET BINARY LOGS AND GTIDS exposes the binary-log collection. The legacy
`RESET MASTER` spelling is also classified as binary-log metadata so runtime
diagnostics can route the unsupported form deliberately:

```text
ok statements=1 kinds=reset[1:5,0:27]/binary_log
```

```text
ok statements=1 kinds=reset[1:2,0:12]/binary_log
```

CLONE exposes local directory and remote donor targets:

```text
ok statements=2 kinds=clone[1:6,0:41]/directory:'/tmp/clone',clone[8:17,43:95]/server:user@host:3306
```

STOP replication-control forms are classified as `stop`:

```text
ok statements=1 kinds=stop[1:2,0:12]
```

Server lifecycle statements expose the instance target:

```text
ok statements=1 kinds=restart[1:1,0:7]/instance
```

Replication-control statements expose explicit and default channel targets:

```text
ok statements=1 kinds=start[1:5,0:30]/replication_channel:'ch'
```

```text
ok statements=4 kinds=start[1:2,0:13]/replication_channel,stop[4:5,15:27]/replication_channel,reset[7:8,29:42]/replication_channel,change[10:16,44:88]/replication_channel
```

Group Replication start and stop statements expose the group-replication target:

```text
ok statements=2 kinds=start[1:2,0:23]/group_replication,stop[4:5,25:47]/group_replication
```

Transaction-control statements expose the transaction object kind:

```text
ok statements=1 kinds=start[1:4,0:28]/transaction
```

XA transaction statements expose XID targets:

```text
ok statements=1 kinds=xa[1:3,0:12]/xa_transaction:'x'
```

HELP exposes quoted help-topic searches:

```text
ok statements=1 kinds=help[1:2,0:15]/help_topic:'contents'
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

Low-level DDL targets such as server and logfile-group names are reported too:

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

Cursor operations expose their cursor handle:

```text
ok statements=1 kinds=open[1:2,0:6]/cursor:c
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
