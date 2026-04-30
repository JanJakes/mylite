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
`LOAD ... INTO TABLE`, and `LOCK TABLES`:

```text
ok statements=1 kinds=use[1:2,0:7]/database:app
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

Keyword lookup is table-driven and must remain sorted:

```sh
python3 tests/check_keywords.py
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
