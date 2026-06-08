# Baseline CREATE TABLE partition options

This slice admits MySQL partition options on `CREATE TABLE` statements while
preserving MyLite's single-file, nonpartitioned storage model.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/alter-table-partition-operations.html
- https://dev.mysql.com/doc/refman/8.4/en/partitioning-selection.html
- https://dev.mysql.com/doc/refman/8.4/en/partitioning-range.html
- https://dev.mysql.com/doc/refman/8.4/en/partitioning-key.html

## Scope

MySQL allows `CREATE TABLE` definitions to end with table options followed by
partition options. MyLite now accepts partition options after an otherwise
supported `CREATE TABLE` or `CREATE TEMPORARY TABLE` definition:

- `PARTITION BY RANGE (expr) (...)`
- `PARTITION BY RANGE COLUMNS (column_list) (...)`
- `PARTITION BY LIST (expr) (...)`
- `PARTITION BY LIST COLUMNS (column_list) (...)`
- `PARTITION BY HASH (expr) PARTITIONS n`
- `PARTITION BY LINEAR HASH (expr) PARTITIONS n`
- `PARTITION BY KEY [ALGORITHM=1|2] (column_list) PARTITIONS n`
- `PARTITION BY LINEAR KEY [ALGORITHM=1|2] (column_list) PARTITIONS n`
- optional `SUBPARTITION BY HASH|KEY ... SUBPARTITIONS n`
- optional partition definitions containing partition names, values,
  subpartition definitions, comments, engines, data directories, index
  directories, max/min rows, and tablespaces.

The partition clause is an admitted storage-layout suffix only. MyLite creates
the base table descriptor and physical table exactly as if the partition clause
were absent. It does not persist partition descriptors, subpartition
descriptors, partition value descriptors, partition comments, partition storage
options, generated partition names, or partition statistics.

`INFORMATION_SCHEMA.PARTITIONS` continues to expose the existing single
nonpartitioned placeholder row for the created base table, with `PARTITION_NAME`
and partition method fields `NULL`.

## Parser approach

The normal Lemon grammar handles MyLite-supported `CREATE TABLE`. If parsing
the full statement fails, the parser retokenizes the statement, recognizes a
top-level `PARTITION BY` suffix on `CREATE [TEMPORARY] TABLE`, validates that
the suffix has a known partitioning method and balanced parentheses, and then
re-parses the `CREATE TABLE` prefix. This keeps partition options out of the
main grammar until MyLite stores real partition metadata.

The fallback does not accept `CREATE TABLE ... PARTITION BY ... AS SELECT ...`
because dropping the partition suffix while preserving the query expression
needs a dedicated `CREATE TABLE ... SELECT` partition slice.

## Runtime behavior

Runtime execution creates the ordinary base table:

- row storage, indexes, constraints, and defaults follow the supported
  nonpartitioned `CREATE TABLE` path;
- no rows are assigned to partitions;
- no partition pruning is performed;
- no partition metadata appears in `SHOW CREATE TABLE`;
- no warning is emitted in this baseline.

This is intentionally not full MySQL partitioning. It is an embedded
compatibility decision for applications that include storage-layout clauses in
portable DDL but do not rely on partition pruning, partition maintenance, or
partition introspection.

## Non-goals

- no partitioned physical storage;
- no `CREATE TABLE ... SELECT` partition suffix;
- no `ALTER TABLE` partition maintenance;
- no explicit partition selection in `SELECT`, `INSERT`, `REPLACE`, `UPDATE`,
  `DELETE`, `LOAD DATA`, or `LOAD XML`;
- no partition pruning or partition-aware optimizer behavior;
- no partition descriptor persistence or partition rows in metadata;
- no partition-specific error enforcement for unique keys, primary keys,
  foreign keys, storage engines, partition expressions, or partition names.

## MyLite grammar snippets

These snippets describe the accepted suffix shape without copying MySQL
grammar.

```lemon
create_table_statement ::=
    CREATE TABLE create_if_not_exists_opt table_name LPAREN create_table_item_list RPAREN
    table_option_list_opt partition_options_opt.

partition_options_opt ::= .
partition_options_opt ::= PARTITION BY partition_method partition_count_opt
    subpartition_options_opt partition_definition_list_opt.

partition_method ::= HASH LPAREN expression RPAREN.
partition_method ::= LINEAR HASH LPAREN expression RPAREN.
partition_method ::= KEY key_algorithm_opt LPAREN identifier_list_opt RPAREN.
partition_method ::= LINEAR KEY key_algorithm_opt LPAREN identifier_list_opt RPAREN.
partition_method ::= RANGE LPAREN expression RPAREN.
partition_method ::= RANGE COLUMNS LPAREN identifier_list RPAREN.
partition_method ::= LIST LPAREN expression RPAREN.
partition_method ::= LIST COLUMNS LPAREN identifier_list RPAREN.
```

The current implementation uses the documented post-parse fallback instead of
these Lemon productions.

## Tests

Focused parser tests cover `RANGE`, `LIST`, `HASH`, `LINEAR KEY`, temporary
tables, subpartition syntax, incomplete partition suffix rejection,
`CREATE TABLE ... SELECT` partition-suffix rejection, and continued rejection
of query partition selection.

Focused runtime tests verify that a partitioned `CREATE TABLE` statement
creates a normal base table, stores and reads rows through the ordinary table
path, and exposes only the existing nonpartitioned
`INFORMATION_SCHEMA.PARTITIONS` placeholder row.
