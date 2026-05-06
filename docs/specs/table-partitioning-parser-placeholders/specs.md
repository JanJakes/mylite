# Table Partitioning Parser Placeholders

## Scope

This slice recognizes MySQL table partitioning syntax as parser placeholders. It
does not implement partitioned storage, pruning, partition metadata,
partition-selection DML, per-partition maintenance, partitioned `CREATE TABLE`
execution, or `INFORMATION_SCHEMA.PARTITIONS`.

Covered syntax includes:

- `CREATE TABLE ... [table_options] PARTITION BY ...`
- `ALTER TABLE ... PARTITION BY ...`
- `ALTER TABLE ... ADD PARTITION ...`
- `ALTER TABLE ... DROP PARTITION ...`
- `ALTER TABLE ... DISCARD PARTITION ...`
- `ALTER TABLE ... IMPORT PARTITION ...`
- `ALTER TABLE ... TRUNCATE PARTITION ...`
- `ALTER TABLE ... COALESCE PARTITION ...`
- `ALTER TABLE ... REORGANIZE PARTITION ...`
- `ALTER TABLE ... EXCHANGE PARTITION ...`
- `ALTER TABLE ... ANALYZE PARTITION ...`
- `ALTER TABLE ... CHECK PARTITION ...`
- `ALTER TABLE ... OPTIMIZE PARTITION ...`
- `ALTER TABLE ... REBUILD PARTITION ...`
- `ALTER TABLE ... REPAIR PARTITION ...`
- `ALTER TABLE ... REMOVE PARTITIONING`

The existing nonpartitioned `CREATE TABLE` and `ALTER TABLE` implementations
remain unchanged.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `ALTER TABLE Partition Operations`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table-partition-operations.html
- MySQL 8.4 Reference Manual, `Partitioning Types`:
  https://dev.mysql.com/doc/refman/8.4/en/partitioning-types.html
- Runtime probes against local `mysql:8.4.9` container
  `mylite-mysql-849`.

## MySQL Runtime Observations

Representative MySQL 8.4.9 probes accepted:

- `CREATE TABLE p_hash(id INT) PARTITION BY HASH(id) PARTITIONS 2`
- `CREATE TABLE p_range(id INT) PARTITION BY RANGE (id) (PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN MAXVALUE)`
- `ALTER TABLE p_hash COALESCE PARTITION 1`
- `ALTER TABLE p_range DROP PARTITION p0`
- `ALTER TABLE p_range TRUNCATE PARTITION ALL`
- `ALTER TABLE p_range ANALYZE PARTITION p1`
- `ALTER TABLE p_range REMOVE PARTITIONING`

`ALTER TABLE p_range ADD PARTITION (PARTITION p2 VALUES LESS THAN MAXVALUE)`
was parsed by MySQL but failed with error `1481` because `MAXVALUE` can only be
used in the last partition definition. MyLite treats the same family as a no-op
parser placeholder.

## Syntax

The grammar uses exact partition-operation prefixes and a shared placeholder
tail for the full partition option surface:

```lemon
create_partitioned_table_statement ::= CREATE TABLE opt_if_not_exists table_name
        LPAREN table_element_list RPAREN table_option_list partition_options.

alter_table_partition_statement ::= ALTER TABLE table_name partition_options.
alter_table_partition_statement ::= ALTER TABLE table_name ADD PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name DROP PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name DISCARD PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name IMPORT PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name TRUNCATE PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name COALESCE PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name REORGANIZE PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name EXCHANGE PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name ANALYZE PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name CHECK PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name OPTIMIZE PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name REBUILD PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name REPAIR PARTITION parser_placeholder_tail.
alter_table_partition_statement ::= ALTER TABLE table_name REMOVE PARTITIONING.

partition_options ::= PARTITION BY parser_placeholder_tail.
```

The placeholder tail accepts the tokens needed by MySQL's `HASH`, `KEY`,
`RANGE`, `RANGE COLUMNS`, `LIST`, `LIST COLUMNS`, subpartitioning, partition
definitions, partition values, and per-partition options. It does not build a
partition AST or validate partition semantic constraints.

## Runtime Semantics

Partitioning statements prepare as `MYLITE_SQL_AST_PLACEHOLDER_STATEMENT` with
placeholder kind `MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING`. Executing
appends one warning with code `1235`, returns `MYLITE_DONE`, reports zero
affected rows, and returns no result columns.

Partitioned `CREATE TABLE` does not create an unpartitioned table. It is a
complete no-op placeholder so MyLite never silently drops partition semantics.

## Tests

Parser coverage must include every covered partition operation prefix and
representative partitioning variants:

- `PARTITION BY HASH`
- `PARTITION BY LINEAR KEY`
- `PARTITION BY RANGE`
- `PARTITION BY RANGE COLUMNS`
- `PARTITION BY LIST`
- `PARTITION BY LIST COLUMNS`
- subpartition syntax
- partition definitions with `VALUES LESS THAN`, `VALUES IN`, and `MAXVALUE`

Runtime coverage must assert warning `1235` and no result columns for
partitioned `CREATE TABLE` and representative `ALTER TABLE` partition
operations. A runtime test must also assert that the partitioned `CREATE TABLE`
placeholder does not create the table.
