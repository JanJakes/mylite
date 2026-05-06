# Table Maintenance Statements

## Scope

This feature implements first-slice compatibility for:

- `CHECK TABLE`
- `OPTIMIZE TABLE`
- `REPAIR TABLE`

MyLite accepts the common MySQL syntax, resolves table names, and returns the
MySQL-style result set columns:

- `Table`
- `Op`
- `Msg_type`
- `Msg_text`

The statements are embedded no-ops. They do not rebuild tables, repair data,
refresh optimizer statistics, or mutate storage.

## Sources

- MySQL 8.4 Reference Manual, `CHECK TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/check-table.html
- MySQL 8.4 Reference Manual, `OPTIMIZE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/optimize-table.html
- MySQL 8.4 Reference Manual, `REPAIR TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/repair-table.html
- Runtime probes against MySQL 8.4.9.

The specification is independently authored from official documentation and
observed runtime behavior.

## Syntax

MyLite Lemon grammar shape:

```lemon
table_maintenance_statement ::= CHECK TABLE maintenance_table_name_list
    check_table_option_list.
table_maintenance_statement ::= OPTIMIZE opt_write_to_binlog TABLE
    maintenance_table_name_list.
table_maintenance_statement ::= REPAIR opt_write_to_binlog TABLE
    maintenance_table_name_list repair_table_option_list.

maintenance_table_name_list ::= table_name.
maintenance_table_name_list ::= maintenance_table_name_list COMMA table_name.

opt_write_to_binlog ::= .
opt_write_to_binlog ::= NO_WRITE_TO_BINLOG.
opt_write_to_binlog ::= LOCAL.

check_table_option_list ::= .
check_table_option_list ::= check_table_option_list check_table_option.
check_table_option ::= QUICK.
check_table_option ::= FAST.
check_table_option ::= MEDIUM.
check_table_option ::= EXTENDED.
check_table_option ::= CHANGED.
check_table_option ::= FOR UPGRADE.

repair_table_option_list ::= .
repair_table_option_list ::= repair_table_option_list repair_table_option.
repair_table_option ::= QUICK.
repair_table_option ::= EXTENDED.
repair_table_option ::= USE_FRM.
```

Options are accepted and ignored.

## Runtime Semantics

Table names may be unqualified or schema-qualified. Unqualified names require a
selected schema. Temporary tables, persistent tables, and supported
`information_schema` views are treated as existing targets.

For an existing target:

- `CHECK TABLE` returns one `status` / `OK` row.
- `OPTIMIZE TABLE` returns the InnoDB-style no-op `note` row and then
  `status` / `OK`.
- `REPAIR TABLE` returns the InnoDB-style unsupported-engine `note` row.

For a missing table in a known schema, MyLite returns an `Error` row followed by
`status` / `Operation failed`.

For an unknown schema, MyLite returns an `Error` row followed by `error` /
`Corrupt`, matching observed MySQL 8.4.9 result-set shape for these
statements.

## Deferred

- Actual consistency checking for corrupted tables or indexes.
- Statistics refresh or table rebuild behavior for `OPTIMIZE TABLE`.
- Data repair behavior for storage engines that support repair.
- Table locks and binary logging interactions.
- Full diagnostics metadata beyond current row text.

## Tests

Coverage includes:

- parser acceptance for table lists and statement-specific options
- parser rejection for misplaced options
- runtime `No database selected` handling for unqualified names
- existing persistent and temporary tables
- missing tables and unknown schemas
- selected-schema and fully-qualified table names
- supported `information_schema` targets
