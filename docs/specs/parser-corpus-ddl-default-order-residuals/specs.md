# Parser Corpus DDL Default And Order Residuals

This slice closes a small set of valid MySQL 8.4.9 DDL residuals still present
in the mysql-server-tests parser corpus:

- repeated column `DEFAULT` clauses;
- `FLOAT(10.3)` approximate-type syntax;
- comma-tail `ALTER TABLE ... ORDER BY ...` actions;
- bare `ALTER TABLE ... ADD PARTITION` and
  `ALTER TABLE ... REORGANIZE PARTITION`.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, partition-management operations:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table-partition-operations.html>
- Runtime evidence:
  `packages/libmylite/tests/mysql_parser_corpus_ddl_default_order_residuals_expectations.sh`
  against MySQL 8.4.9.

## Scope

Implemented with executable MyLite behavior:

- A column may contain more than one `DEFAULT` clause. The last default clause
  is the effective default, matching observed MySQL behavior and producing no
  warning for the covered literal forms.
- Repeated defaults are accepted as repeated column attributes in supported
  column-definition paths; column planning chooses the final default node.
- `FLOAT(decimal-token)` is accepted and normalized to the same descriptor as
  bare `FLOAT`. MySQL accepts `FLOAT(10.3)` and reports `float` metadata for
  the covered form, so MyLite does not preserve that decimal-looking token as
  precision metadata.

Implemented as unsupported utility placeholders:

- `ALTER TABLE table ADD COLUMN ..., ORDER BY column[, ...]`
- `ALTER TABLE table ADD PARTITION`
- `ALTER TABLE table REORGANIZE PARTITION`

The placeholder forms parse to `unsupported_utility_statement` after the normal
parser fails, execute with MyLite's deterministic unsupported utility
diagnostic, and do not mutate catalogs or rows.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned grammar shape.

```lemon
column_attribute_list ::= column_attribute_list column_attribute.
column_attribute ::= column_default.
column_default ::= DEFAULT NULL.
column_default ::= DEFAULT NULL DEFAULT NULL.
column_default ::= DEFAULT NULL DEFAULT column_default_value.
column_default ::= DEFAULT column_default_value.
column_default ::= DEFAULT column_default_value DEFAULT NULL.
column_default ::= DEFAULT column_default_value DEFAULT column_default_value.

approximate_precision_opt ::= LPAREN DECIMAL RPAREN.
```

The comma-tail reorder and bare partition operations stay in the post-parse
placeholder classifier rather than the main executable Lemon grammar.

## Runtime Semantics

Repeated defaults follow MySQL's last-default-wins behavior:

```sql
CREATE TABLE t (
  a INT DEFAULT 1 DEFAULT 2,
  b INT DEFAULT NULL DEFAULT 5,
  c INT DEFAULT 6 DEFAULT NULL
);
```

`INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT`, `SHOW COLUMNS`, and omitted-column
inserts observe `2`, `5`, and `NULL`, respectively. Other duplicate column
attributes remain unsupported as before.

`FLOAT(10.3)` is stored and reported as `float`. This slice does not broaden
approximate numeric precision semantics beyond already supported `FLOAT`,
`FLOAT(M,D)`, `DOUBLE(M,D)`, and related aliases.

Comma-tail reorder actions and partition-management operations are not no-ops.
They may reorder physical rows, repartition data, or redistribute rows in
MySQL. MyLite has no partitioned storage and does not currently execute
multi-action reorder clauses, so the safe behavior is a predictable unsupported
diagnostic before catalog mutation.

## Tests

- Parser tests cover repeated defaults, `FLOAT(10.3)`, comma-tail reorder
  placeholders, bare partition placeholders, and malformed tails.
- Runtime tests cover last-default metadata and default insertion behavior, plus
  no-mutation unsupported diagnostics for reorder and partition placeholders.
- The MySQL expectation script records the target-runtime observations for the
  covered forms.

## Compatibility Status

This improves valid DDL parser and default-value compatibility. It does not add
partitioned storage, physical row reordering for multi-action `ALTER TABLE`, or
new approximate numeric precision behavior.
