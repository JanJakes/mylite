# CREATE TABLE ... SELECT

## Scope

This slice adds executable support for `CREATE TABLE ... SELECT` without
predeclared column or index definitions:

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] tbl_name [table_options]
    [AS] SELECT ...
```

In scope:

- schema-qualified and default-schema target names
- scalar `SELECT` and supported table-backed `SELECT` statements
- optional `AS`
- `TEMPORARY` destinations
- `IF NOT EXISTS` target no-op after SELECT validation
- inferred target columns from SELECT result metadata
- row materialization into the new table
- MySQL-compatible implicit commit before non-temporary CTAS

Out of scope for this slice:

- predeclared columns, indexes, checks, and foreign keys before the `SELECT`
- `CREATE TABLE ... TABLE` and `CREATE TABLE ... VALUES`
- view sources
- generated-column assignment rules
- locking clauses such as `FOR UPDATE`
- `IGNORE` and `REPLACE` CTAS modifiers
- full expression type inference beyond MyLite's current SELECT metadata

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE ... SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-select.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Behavior Summary

`CREATE TABLE dst AS SELECT ...` creates a base table and inserts the rows
returned by the SELECT. `ROW_COUNT()` reports the number of inserted rows.

The destination table does not automatically receive indexes from the source
query. Source column attributes such as type, nullability, default, character
set, collation, and comment are preserved for direct selected columns, while
key membership and `AUTO_INCREMENT` are not preserved. A NOT NULL selected
column without an explicit default receives the type's implicit default, such
as `0` for integer columns. Expression columns use the expression label as the
column name and infer a compatible type.

The destination table uses the target schema's default table collation unless
an explicit table option overrides it. A temporary destination is created only
when the statement uses `CREATE TEMPORARY TABLE`.

Observed diagnostics and ordering:

- unqualified target names require a selected schema
- unqualified table references inside the SELECT require a selected schema
- missing SELECT source tables fail before target creation
- duplicate SELECT output column names fail with error 1060
- `CREATE TABLE IF NOT EXISTS existing AS SELECT ...` validates the SELECT
  before returning note 1050 and does not insert rows into the existing table
- non-temporary CTAS commits an active explicit transaction before execution

## MyLite Behavior

### Parser And AST

MyLite extends the existing `CREATE TABLE` statement node with a
`create_table_select` flag. In the supported no-predefinition form, the node
children are:

1. target table name
2. select statement
3. table options
4. optional `IF NOT EXISTS`

The parser also accepts the predefinition form for future implementation, but
runtime rejects it deterministically before mutation in this slice.

Supported Lemon-style grammar:

```lemon
create_table_statement ::= CREATE opt_temporary TABLE opt_if_not_exists
                           table_name table_option_list opt_as select_statement.
create_table_statement ::= CREATE opt_temporary TABLE opt_if_not_exists
                           table_name LPAREN table_element_list RPAREN
                           table_option_list opt_as select_statement.
```

### Runtime Execution

Execution order:

1. Non-temporary destinations commit an active explicit transaction before
   validation and execution.
2. The SELECT statement is prepared first so missing source tables, duplicate
   aliases, unsupported predicates, and unsupported projections fail before
   target no-op handling.
3. The SELECT result metadata is converted to target column definitions.
4. Duplicate output column names fail with error 1060.
5. The target schema and table name are validated with the same rules as
   ordinary `CREATE TABLE`.
6. `IF NOT EXISTS` skips an existing target with note 1050 after SELECT
   validation.
7. Physical table creation, SELECT row insertion, and catalog insertion happen
   inside one storage transaction.

The first implementation does not pre-create indexes or constraints from a
definition list. That broader path must merge explicit definitions with SELECT
outputs using MySQL's column matching rules before it is marked supported.

## MySQL-Runtime-Verified Expectations

Implementation tests should cover:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `CREATE TABLE c AS SELECT id, n, v FROM src ORDER BY n` | Creates `c`, inserts selected rows, and reports affected rows equal to inserted rows. |
| direct selected source columns | Preserve type, nullability, defaults, character set, collation, and comments, but omit keys and `AUTO_INCREMENT`. |
| scalar expressions | Create expression-named columns with inferred metadata and insert one row. |
| duplicate SELECT output names | Fail with error 1060 before target creation. |
| missing SELECT source with `IF NOT EXISTS existing` | Fail with missing-source error before target no-op handling. |
| existing target with valid SELECT and `IF NOT EXISTS` | Return note 1050 and leave the existing table unchanged. |
| `CREATE TEMPORARY TABLE tmp AS SELECT ...` | Create a temporary table hidden from `INFORMATION_SCHEMA.TABLES`. |
| CTAS inside an explicit transaction | Commit prior work before non-temporary CTAS and leave later autocommit work outside the rolled-back transaction. |

## Test Plan

- Parser tests for `AS`, omitted `AS`, `TEMPORARY`, `IF NOT EXISTS`, table
  options, and the accepted-but-deferred predefinition form.
- Runtime tests for source-column metadata, scalar expression metadata, row
  counts, warnings, duplicate names, missing source ordering, temporary
  destination visibility, and implicit commit behavior.
- Compatibility docs update in `COMPATIBILITY.md` and roadmap tasks.
