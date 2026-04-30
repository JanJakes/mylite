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

Direct utility targets are reported for statements such as `USE`, `TABLE`,
`TRUNCATE`, `HANDLER`, direct `DESCRIBE` / `EXPLAIN` table forms,
`LOAD ... INTO TABLE`, `LOCK TABLES`, and unambiguous `SHOW` table/schema
forms. Prepared statement handles are reported with their own object kind:

```text
ok statements=1 kinds=use[1:2,0:7]/database:app
```

```text
ok statements=1 kinds=prepare[1:4,0:22]/prepared_statement:stmt
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
target while ordinary variable assignments stay objectless:

```text
ok statements=1 kinds=set[1:3,0:10]/role:r
```

SHOW account-introspection forms preserve account spans:

```text
ok statements=1 kinds=show[1:6,0:24]/user:'u'@'h'
```

Transaction savepoint statements expose the savepoint handle:

```text
ok statements=1 kinds=savepoint[1:2,0:11]/savepoint:s
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
