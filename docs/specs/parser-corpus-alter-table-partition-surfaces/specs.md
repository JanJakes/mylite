# Parser Corpus ALTER TABLE partition surfaces

This slice admits high-volume MySQL 8.4 `ALTER TABLE` partition-operation
syntax from the parser corpus while preserving MyLite's current single-file,
nonpartitioned storage model.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- https://dev.mysql.com/doc/refman/8.4/en/alter-table-partition-operations.html
- https://dev.mysql.com/doc/refman/8.4/en/create-table.html

## Scope

MySQL partition-related `ALTER TABLE` clauses can repartition a table, add or
remove partitions, split or merge partitions, exchange rows with another table,
delete rows from partitions, and run partition maintenance.

MyLite now accepts these forms after the normal parser reports a syntax error:

- `ALTER TABLE table_name PARTITION BY ...`
- `ALTER TABLE table_name REMOVE PARTITIONING`
- `ALTER TABLE table_name ADD PARTITION ...`
- `ALTER TABLE table_name DROP PARTITION ...`
- `ALTER TABLE table_name REORGANIZE PARTITION ...`
- `ALTER TABLE table_name REBUILD PARTITION ...`
- `ALTER TABLE table_name COALESCE PARTITION ...`
- `ALTER TABLE table_name TRUNCATE PARTITION ...`
- `ALTER TABLE table_name EXCHANGE PARTITION ...`
- `ALTER TABLE table_name ANALYZE PARTITION ...`
- `ALTER TABLE table_name CHECK PARTITION ...`
- `ALTER TABLE table_name OPTIMIZE PARTITION ...`
- `ALTER TABLE table_name REPAIR PARTITION ...`

The recognizer also admits these partition operations when they appear after
top-level `ALGORITHM` or `LOCK` option clauses, or inside a comma-separated
`ALTER TABLE` action list that the current Lemon grammar cannot otherwise
accept.

## Runtime behavior

Every admitted statement becomes an `unsupported_utility_statement`. Runtime
execution returns `1064 / 42000` with a MyLite-owned unsupported diagnostic and
does not mutate data, schema descriptors, auto-increment state, partition
metadata placeholders, transactions, or SQLite physical schema.

This is stricter than the current `CREATE TABLE ... PARTITION BY` placeholder,
which creates a normal nonpartitioned base table. `ALTER TABLE` partition
operations are not treated as no-ops because several MySQL forms have visible
data or metadata effects:

- `TRUNCATE PARTITION` deletes rows in the named partitions;
- `EXCHANGE PARTITION` swaps rows with a nonpartitioned table;
- `DROP`, `COALESCE`, and `REORGANIZE PARTITION` change partition layout and
  redistribute rows;
- `PARTITION BY` and `REMOVE PARTITIONING` change the table definition.

Accepting those statements as successful no-ops would hide meaningful
application behavior. Parsing them and returning a deterministic unsupported
diagnostic gives applications a predictable failure instead of a generic parser
error.

## Parser approach

The normal Lemon grammar remains the authority for supported `ALTER TABLE`
syntax. The existing post-parse placeholder recognizer runs only after a syntax
error. For statements beginning with `ALTER TABLE`, it scans top-level tokens
outside parentheses and classifies the statement as an unsupported utility
placeholder when it finds a partition operation marker.

The fallback is intentionally broad about operation payloads but conservative
about the statement family:

- parenthesis nesting must remain balanced;
- partition markers must be top-level, not inside a partition definition or
  ordinary expression;
- nonpartition `ALTER TABLE` statements are left to the normal parser or the
  existing syntax-error path.

## MyLite grammar snippets

These snippets describe the intended MyLite-owned parser surface. The current
implementation uses the documented post-parse classifier instead of adding
these productions to Lemon.

```lemon
alter_table_statement ::= ALTER TABLE table_name alter_partition_operation.

alter_partition_operation ::= PARTITION BY partition_method partition_tail.
alter_partition_operation ::= REMOVE PARTITIONING.
alter_partition_operation ::= ADD PARTITION partition_tail.
alter_partition_operation ::= DROP PARTITION partition_tail.
alter_partition_operation ::= REORGANIZE PARTITION partition_tail.
alter_partition_operation ::= REBUILD PARTITION partition_tail.
alter_partition_operation ::= COALESCE PARTITION partition_tail.
alter_partition_operation ::= TRUNCATE PARTITION partition_tail.
alter_partition_operation ::= EXCHANGE PARTITION partition_tail.
alter_partition_operation ::= ANALYZE PARTITION partition_tail.
alter_partition_operation ::= CHECK PARTITION partition_tail.
alter_partition_operation ::= OPTIMIZE PARTITION partition_tail.
alter_partition_operation ::= REPAIR PARTITION partition_tail.
```

## Tests

MySQL 8.4.9 expectation probes record representative partition-operation forms
that parse and execute against suitable partitioned-table fixtures. MyLite
parser tests cover repartitioning, removal, add/drop, merge/split,
maintenance, truncation, exchange, and online-DDL option ordering. Runtime
tests verify that admitted statements return unsupported diagnostics and leave
ordinary MyLite table rows unchanged.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

Current benchmark result after this slice:

```text
parse.csv.mysql_server_tests,parse,1,69595,69595,65524,4071,0,5544381,2405.259,34.561,28934.519
# parse_status ok=65524 misuse=0 nomem=0 lexer_error=21 syntax_error=4049 stack_overflow=1
```

## Non-goals

- no physical partitioned storage;
- no partition descriptor persistence;
- no partition pruning;
- no partition-specific rows in `INFORMATION_SCHEMA.PARTITIONS`;
- no partition maintenance or repair implementation;
- no data movement, deletion, split, merge, or exchange;
- no MySQL partition-operation implicit-commit emulation before an unsupported
  runtime diagnostic.
