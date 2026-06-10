# Parser Corpus View, Full-Text, and Utility Placeholders

This slice reduces remaining high-volume MySQL server-test parser-corpus
failures for syntax that MySQL 8.4.9 accepts but MyLite cannot execute yet. The
goal is explicit compatibility handling: valid-but-unsupported data-affecting
forms parse to MyLite's unsupported-utility diagnostic, while server-only
administration forms with no embedded side effect parse to warning no-ops.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-view.html
- https://dev.mysql.com/doc/refman/8.4/en/fulltext-search.html
- https://dev.mysql.com/doc/refman/8.4/en/select-into.html
- https://dev.mysql.com/doc/refman/8.4/en/load-xml.html
- https://dev.mysql.com/doc/refman/8.4/en/import-table.html
- https://dev.mysql.com/doc/refman/8.4/en/lock-instance-for-backup.html
- https://dev.mysql.com/doc/refman/8.4/en/change-replication-source-to.html
- https://dev.mysql.com/doc/refman/8.4/en/help.html
- https://dev.mysql.com/doc/refman/8.4/en/create-tablespace.html
- https://dev.mysql.com/doc/refman/8.4/en/drop-tablespace.html

## MySQL 8.4.9 Observations

Representative accepted forms from the current corpus and focused probes:

```sql
CREATE ALGORITHM=TEMPTABLE VIEW v AS
    SELECT t1.a FROM (t1 JOIN t2 ON t1.a = t2.a);
ALTER VIEW v AS SELECT LPAD('x', 1 NOT IN (0), 1) AS c;
SELECT MATCH a,b AGAINST ('+mysql*' IN BOOLEAN MODE) FROM ft;
SELECT * FROM ft WHERE MATCH a,b AGAINST ('mysql' IN BOOLEAN MODE);
SELECT a,b INTO OUTFILE '/tmp/result.txt' FROM t;
SELECT a INTO DUMPFILE '/tmp/blob.bin' FROM t;
LOAD XML INFILE 'rows.xml' INTO TABLE t ROWS IDENTIFIED BY '<row>';
IMPORT TABLE FROM '/tmp/mysql-files/*.sdi';
HELP 'contents';
LOCK INSTANCE FOR BACKUP;
UNLOCK INSTANCE;
CHANGE REPLICATION SOURCE TO SOURCE_HOST=host1, SOURCE_PORT=3002
    FOR CHANNEL 'channel2';
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
DROP UNDO TABLESPACE undo_003;
```

Observed runtime details used by the tests:

- MySQL accepts both `MATCH(a,b) AGAINST (...)` and shorthand
  `MATCH a,b AGAINST (...)`; with the same indexed row and search term they
  produce the same score.
- `SELECT ... INTO OUTFILE`, `SELECT ... INTO DUMPFILE`, `LOAD XML`, and
  `IMPORT TABLE` are syntax-valid. In the local MySQL 8.4.9 comparison
  container they fail after parsing because file operations are restricted by
  `secure_file_priv`.
- `HELP no_such_topic` is syntax-valid and returns MySQL's help surface for a
  missing topic.
- `LOCK INSTANCE FOR BACKUP` and `UNLOCK INSTANCE` succeed for the root
  comparison runtime.
- `CHANGE REPLICATION SOURCE TO` is a replica-server administration statement
  with replication-channel side effects and implicit-commit behavior on MySQL.
  MyLite deliberately does not run a runtime probe that mutates replica
  metadata; official MySQL 8.4 documentation is the source for parser
  acceptance.

## Scope

### Unsupported-utility placeholders

MyLite accepts the following statement families after the normal parser fails,
then returns `1064 / 42000` with the existing unsupported utility diagnostic at
runtime:

- `CREATE VIEW ... AS <query>` and `ALTER VIEW ... AS <query>` forms whose
  bodies are valid MySQL query expressions but outside the current baseline
  view grammar or execution subset;
- shorthand full-text `MATCH col[, ...] AGAINST (...)` in query or DML
  statements, including view bodies;
- `SELECT` / query-expression file output with `INTO OUTFILE` or
  `INTO DUMPFILE`;
- `LOAD XML [LOCAL] INFILE ...`;
- `IMPORT TABLE FROM ...`;
- `ALTER TABLE ... DISCARD TABLESPACE` and
  `ALTER TABLE ... IMPORT TABLESPACE`, including partition/subpartition
  tablespace file operations;
- `HELP ...`.

These are not warning no-ops because successful execution would imply user
catalog mutation, full-text evaluation, server-side file IO, import/export
behavior, physical tablespace file operations, or server help result rows that
MyLite does not yet implement.

### Embedded no-op placeholders

MyLite accepts the following statement families as embedded no-ops with warning
`1105`, preserving the existing utility/admin no-op behavior:

- `LOCK INSTANCE FOR BACKUP`;
- `UNLOCK INSTANCE`;
- `CHANGE REPLICATION SOURCE TO ...`;
- `CREATE UNDO TABLESPACE ...`;
- `ALTER UNDO TABLESPACE ...`;
- `DROP UNDO TABLESPACE ...`.

These statements have server-instance, replication-channel, or physical
tablespace effects that do not map to MyLite's embedded single-file runtime.
They leave MyLite transactions, user data, metadata catalogs, replication
placeholders, warning state except for the one no-op warning, and row counts
unchanged.

Existing supported behavior is unchanged:

- baseline `CREATE VIEW` / `ALTER VIEW` forms that parse normally continue to
  use view descriptor validation and metadata storage;
- parenthesized `MATCH(...) AGAINST(...)` expressions keep their existing
  parser placeholder path;
- `SHOW REPLICA STATUS`, `SHOW REPLICAS`, `SHOW BINARY LOG STATUS`, and
  `SHOW BINARY LOGS` keep their existing result-shape implementations;
- `LOAD DATA` keeps the existing implemented subset and unsupported-utility
  placeholder path for unsupported options.

## Parser Approach

The normal Lemon grammar remains the authority for supported SQL. This slice
extends the existing post-parse placeholder classifier, which runs only after a
syntax error from the normal parser.

Classifier requirements:

- reject lexically malformed SQL and obvious incomplete statements;
- reject multi-statement input for no-op placeholders;
- accept only balanced-parentheses surfaces for query, view, full-text, and
  file-operation placeholders;
- avoid converting invalid top-level fragments such as `MATCH a AGAINST ('x')`
  into placeholders;
- classify only statement families with explicit runtime behavior.

No SQLite fork hook is needed. The work stays in MyLite's parser/runtime
compatibility layer.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned grammar surface. This slice
uses the post-failure classifier for unsupported surfaces rather than broad
Lemon productions, because the normal grammar already owns supported subsets.

```lemon
create_view_statement ::=
    CREATE view_options_opt VIEW table_name view_column_list_opt
    AS query_expression view_check_option_opt.

alter_view_statement ::=
    ALTER view_options_opt VIEW table_name view_column_list_opt
    AS query_expression view_check_option_opt.

expression ::= fulltext_match_against.
fulltext_match_against ::=
    MATCH LPAREN fulltext_column_list RPAREN AGAINST LPAREN expression fulltext_mode_opt RPAREN.
fulltext_match_against ::=
    MATCH fulltext_column_list AGAINST LPAREN expression fulltext_mode_opt RPAREN.
fulltext_column_list ::= column_reference.
fulltext_column_list ::= fulltext_column_list COMMA column_reference.

select_file_output ::= query_expression INTO OUTFILE STRING export_options_opt.
select_file_output ::= query_expression INTO DUMPFILE STRING.

utility_statement ::= HELP expression.
utility_statement ::= LOAD XML load_xml_tail.
utility_statement ::= IMPORT TABLE FROM string_list.
utility_statement ::= ALTER TABLE table_name DISCARD tablespace_file_target TABLESPACE.
utility_statement ::= ALTER TABLE table_name IMPORT tablespace_file_target TABLESPACE.
tablespace_file_target ::= .
tablespace_file_target ::= PARTITION identifier_list.
tablespace_file_target ::= SUBPARTITION identifier_list.
admin_noop_statement ::= LOCK INSTANCE FOR BACKUP.
admin_noop_statement ::= UNLOCK INSTANCE.
admin_noop_statement ::= CHANGE REPLICATION SOURCE TO replication_option_list channel_opt.
utility_noop_statement ::= CREATE UNDO TABLESPACE tablespace_name tablespace_options_opt.
utility_noop_statement ::= ALTER UNDO TABLESPACE tablespace_name tablespace_options_opt.
utility_noop_statement ::= DROP UNDO TABLESPACE tablespace_name tablespace_options_opt.
```

## Runtime Behavior

Unsupported placeholders:

- parse to `MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT`;
- execution returns `MYLITE_ERROR`;
- diagnostics are `1064 / 42000` and contain
  `utility statement is not supported`;
- no result rows, warnings, catalogs, user data, files, variables, or
  transactions are modified.

No-op placeholders:

- parse to `MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT` or
  `MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT`;
- execution succeeds with zero columns, zero rows, and affected rows `0`;
- one warning is appended: `1105 / HY000`,
  `MyLite accepted this utility statement as an embedded no-op`;
- `ROW_COUNT()` remains `0`;
- no implicit commit is performed for these placeholders.

## Tests

Focused coverage:

- MySQL 8.4.9 expectation script for representative valid syntax and runtime
  post-parse failures;
- parser classification for complex view bodies, shorthand `MATCH ... AGAINST`,
  file import/export, help, replication-source, backup-lock, and undo
  tablespace surfaces;
- parser syntax-error preservation for malformed fragments and incomplete file
  output;
- runtime unsupported diagnostics for the unsupported-utility surfaces;
- runtime no-op warning/result behavior for the embedded admin and utility
  no-op surfaces;
- corpus benchmark movement for the server-test query CSV.

## Compatibility Status

This slice improves parser compatibility and runtime diagnostics. It does not
add full-text search execution, broader view execution, writable views,
server-side file IO, XML import, SDI import, backup locks, replication channel
state, implicit commit behavior for replica statements, server help result
sets, or physical undo tablespaces.
