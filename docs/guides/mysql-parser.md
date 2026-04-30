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

Direct utility targets are reported for statements such as `USE`, `TABLE`,
`TRUNCATE`, `HANDLER`, `IMPORT TABLE`, `CALL`, direct `DESCRIBE` / `EXPLAIN`
table forms, `EXPLAIN ... FOR CONNECTION`, `LOAD ... INTO TABLE`, `CACHE INDEX`,
`LOAD INDEX INTO CACHE`, `LOCK TABLES`, and unambiguous `SHOW` table/schema
forms. Prepared statement handles and component/plugin administration targets
are reported with their own object kinds:

```text
ok statements=1 kinds=use[1:2,0:7]/database:app
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
`SET`, and statement-level `GET DIAGNOSTICS` forms:

```text
ok statements=1 kinds=select[1:6,0:23]/user_variable:@x
```

```text
ok statements=1 kinds=set[1:4,0:31]/system_variable:@@session.sql_mode
```

```text
ok statements=1 kinds=get[1:5,0:27]/user_variable:@n
```

SHOW account-introspection forms preserve account spans:

```text
ok statements=1 kinds=show[1:6,0:24]/user:'u'@'h'
```

Schema-scoped SHOW forms expose explicit `FROM` or `IN` database targets:

```text
ok statements=1 kinds=show[1:5,0:25]/database:db
```

Routine-code SHOW forms expose routine names:

```text
ok statements=1 kinds=show[1:4,0:20]/function:f
```

SHOW ENGINE diagnostics expose the engine name:

```text
ok statements=1 kinds=show[1:4,0:25]/engine:InnoDB
```

SHOW PROFILE exposes explicit query ids:

```text
ok statements=1 kinds=show[1:5,0:24]/query:1
```

KILL statements expose the processlist id as a connection target:

```text
ok statements=1 kinds=kill[1:3,0:14]/connection:123
```

Binary log statements expose explicit log-file targets:

```text
ok statements=1 kinds=show[1:7,0:41]/binary_log:'bin.000001'
```

Relay log event statements expose explicit relay-log files, or a channel when
no file is named:

```text
ok statements=1 kinds=show[1:7,0:43]/relay_log:'relay.000001'
```

SHOW REPLICA STATUS exposes explicit channel names:

```text
ok statements=1 kinds=show[1:6,0:36]/replication_channel:'ch'
```

BINLOG statements expose their event payload string:

```text
ok statements=1 kinds=binlog[1:2,0:12]/binary_log_event:'abc'
```

FLUSH TABLES exposes the first table target:

```text
ok statements=1 kinds=flush[1:3,0:14]/table:t
```

FLUSH RELAY LOGS exposes explicit channel names:

```text
ok statements=1 kinds=flush[1:6,0:33]/replication_channel:'ch'
```

RESET PERSIST exposes explicit persisted system-variable targets:

```text
ok statements=1 kinds=reset[1:3,0:29]/system_variable:max_connections
```

CLONE is classified as its own administrative statement kind:

```text
ok statements=1 kinds=clone[1:6,0:41]
```

STOP replication-control forms are classified as `stop`:

```text
ok statements=1 kinds=stop[1:2,0:12]
```

Server lifecycle statements expose the instance target:

```text
ok statements=1 kinds=restart[1:1,0:7]/instance
```

Replication-control statements expose explicit channel targets:

```text
ok statements=1 kinds=start[1:5,0:30]/replication_channel:'ch'
```

XA transaction statements expose XID targets:

```text
ok statements=1 kinds=xa[1:3,0:12]/xa_transaction:'x'
```

HELP exposes quoted help-topic searches:

```text
ok statements=1 kinds=help[1:2,0:15]/help_topic:'contents'
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

Cursor operations expose their cursor handle:

```text
ok statements=1 kinds=open[1:2,0:6]/cursor:c
```

`LEAVE` and `ITERATE` expose their target label:

```text
ok statements=1 kinds=leave[1:2,0:10]/label:done
```

Leading labels on `BEGIN`, `LOOP`, `REPEAT`, and `WHILE` are reported too:

```text
ok statements=1 kinds=loop[1:7,0:35]/label:done
```

Keyword lookup is table-driven and must remain sorted:

```sh
python3 tests/check_keywords.py
```

Some nonreserved MySQL words are emitted as keyword tokens for analyzer
fidelity but still remain usable in target-name spans:

```text
ok statements=1 kinds=create[1:7,0:26]/table:json
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
