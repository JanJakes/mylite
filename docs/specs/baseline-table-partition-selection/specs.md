# Baseline table partition selection

This slice admits explicit `PARTITION (...)` table references while preserving
MyLite's current single-file, nonpartitioned storage model.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/partitioning-selection.html
- https://dev.mysql.com/doc/refman/8.4/en/partitioning-limitations.html
- https://dev.mysql.com/doc/refman/8.4/en/load-data.html

## Scope

MySQL supports explicit partition and subpartition selection for `SELECT`,
`DELETE`, `INSERT`, `REPLACE`, `UPDATE`, `LOAD DATA`, and `LOAD XML`.
The option appears immediately after the table name and before aliases, index
hints, column lists, or other table-reference options:

```sql
table_name PARTITION (partition_name[, ...])
```

MyLite now accepts this suffix in the supported table-reference envelopes for:

- `SELECT ... FROM table_name PARTITION (...)`, including currently supported
  joins and `INSERT ... SELECT` source `SELECT` statements;
- single-table `DELETE FROM table_name PARTITION (...)`;
- supported joined `DELETE` source tables in `FROM` / `USING` table references;
- single-table and currently supported joined `UPDATE` table sources;
- `INSERT ... VALUES`, `INSERT ... SET`, and `INSERT ... SELECT` targets;
- `REPLACE ... VALUES`, `REPLACE ... SET`, and `REPLACE ... SELECT` targets;
- supported `LOAD DATA [LOCAL] INFILE ... INTO TABLE table_name PARTITION (...)`
  syntax.

`LOAD XML` remains outside this slice because MyLite does not yet admit the
statement form.

## MySQL behavior

MySQL validates that every named partition or subpartition exists for the
target table. Reads, updates, and deletes inspect only the named partitions.
Inserts and replaces fail when a proposed row does not map to one of the named
partitions.

Observed against MySQL 8.4.9:

```sql
CREATE TABLE sales (
    id INT NOT NULL,
    region INT NOT NULL,
    label VARCHAR(16)
) PARTITION BY RANGE (region) (
    PARTITION p0 VALUES LESS THAN (10),
    PARTITION p1 VALUES LESS THAN MAXVALUE
);
INSERT INTO sales VALUES (1,3,'west'),(2,12,'east'),(3,8,'north');
SELECT GROUP_CONCAT(id ORDER BY id) FROM sales PARTITION (p0);
-- 1,3
SELECT GROUP_CONCAT(id ORDER BY id) FROM sales PARTITION (p1);
-- 2
UPDATE sales PARTITION (p0) SET label = 'hit' WHERE id = 2;
SELECT label FROM sales WHERE id = 2;
-- east
DELETE FROM sales PARTITION (p1) WHERE id = 1;
SELECT COUNT(*) FROM sales WHERE id = 1;
-- 1
INSERT INTO sales PARTITION (p0) VALUES (5,99,'bad');
-- ERROR 1748 (HY000): Found a row not matching the given partition set
SELECT COUNT(*) FROM sales PARTITION (missing_p);
-- ERROR 1735 (HY000): Unknown partition 'missing_p' in table 'sales'
```

## MyLite behavior

MyLite accepts the syntax and discards the partition list. All reads and writes
continue to use the ordinary base table. This matches the existing
`CREATE TABLE ... PARTITION BY ...` placeholder policy: partition clauses are
accepted so application SQL can run, but MyLite does not store partition
descriptors or split physical storage.

Consequences:

- no partition names are validated;
- no partition pruning is performed;
- updates and deletes can affect rows that MySQL would skip because they are
  outside the selected partition set;
- inserts and replaces are not rejected for rows that would map outside the
  selected partition set;
- no warnings are emitted in this baseline, matching the existing create-table
  partition placeholder;
- `INFORMATION_SCHEMA.PARTITIONS` continues to expose the single
  nonpartitioned placeholder row for the base table.

This is an explicit embedded-design compatibility placeholder, not full MySQL
partitioning support.

## Parser and runtime design

The Lemon grammar admits a `table_partition_selection_opt` suffix after a
`table_name` in the affected statement forms. The suffix parses the nonempty
identifier list and then returns the original `table_name` AST node to callers.
No new AST node is introduced because there is no later partition planning
surface in this baseline.

Runtime execution needs no table-planning change. Existing descriptor
resolution, table metadata, and SQLite execution paths see the same table name
they saw before this suffix was admitted.

The implementation is MyLite wrapper/parser behavior only. It does not use
SQLite public extension APIs and does not need a targeted SQLite fork hook.

## Errors and edge cases

- `PARTITION ()` remains a syntax error because MySQL requires at least one
  name.
- `PARTITION p0` remains a syntax error because the suffix requires
  parentheses.
- MyLite does not validate duplicate partition names, overlapping names,
  unknown names, partition/subpartition distinction, or generated partition
  names.
- MyLite does not implement partition-specific write errors such as MySQL's
  row-not-matching-partition-set diagnostic.
- Aliases and index hints remain parsed after the partition suffix for source
  table references.

## MyLite grammar snippets

These snippets describe the accepted MyLite grammar shape without copying MySQL
grammar.

```lemon
table_partition_selection ::= PARTITION LPAREN identifier_list RPAREN.

partitioned_table_name ::= table_name.
partitioned_table_name ::= table_name table_partition_selection.

table_source ::=
    table_name table_alias_opt table_index_hints_opt.
table_source ::=
    table_name table_partition_selection table_alias_opt table_index_hints_opt.

insert_values_statement ::=
    INSERT insert_modifier_opt INTO partitioned_table_name insert_column_list_opt
    insert_values_source on_duplicate_key_update_opt.

delete_statement ::=
    DELETE FROM table_name table_partition_selection table_alias_opt
    where_clause_opt order_clause_opt delete_limit_clause_opt.

update_table_source ::=
    table_name table_partition_selection table_index_hints_opt.

load_data_infile_statement ::=
    LOAD DATA INFILE STRING INTO TABLE partitioned_table_name load_data_tail_opt.
```

## Tests

MySQL expectation tests record MySQL 8.4.9 partition-pruning behavior, write
restriction behavior, unknown-partition errors, and `INSERT ... SELECT` source
partition selection.

Parser tests cover `SELECT`, joins, aliases, index hints, `INSERT`, `REPLACE`,
`UPDATE`, `DELETE`, and `LOAD DATA` table references, plus malformed empty and
unparenthesized partition lists.

Runtime tests verify MyLite's current placeholder behavior: reads, updates,
deletes, inserts, and replaces execute against the base table as if the
partition list were absent, with no partition-specific metadata created.
