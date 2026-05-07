# ALTER TABLE AUTO_INCREMENT

## Sources

- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` table options:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4.9 runtime probes run against local test server on 2026-05-06.

## Scope

This slice implements the table option form:

```lemon
ddl_table_option ::= AUTO_INCREMENT opt_equal INTEGER.
alter_table_item ::= ddl_table_option.
```

The option is accepted anywhere MyLite currently accepts comma-separated
`ALTER TABLE` items. Repeated `AUTO_INCREMENT` options are parsed and the last
one in statement order determines the requested value.

## MySQL Behavior

MySQL accepts `ALTER TABLE tbl AUTO_INCREMENT = N` and
`ALTER TABLE tbl AUTO_INCREMENT N`. For InnoDB tables with an `AUTO_INCREMENT`
column, a requested value greater than the current generated sequence is used
for the next generated value. A requested value lower than the current maximum
stored auto value does not lower future generated values; the next generated
value remains at least `MAX(auto_column) + 1`.

If a table has no `AUTO_INCREMENT` column, MySQL accepts the statement but does
not create or change generated-value state. A table option recorded by
`CREATE TABLE ... AUTO_INCREMENT=N` remains visible; a table created without
that option continues to expose `AUTO_INCREMENT` as `NULL`.

Observed MySQL 8.4.9 results:

- `CREATE TABLE ai (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=10`
  followed by two generated inserts produced ids `10` and `11`.
- `ALTER TABLE ai AUTO_INCREMENT=50` affected zero rows and the next generated
  insert produced id `50`.
- `ALTER TABLE ai AUTO_INCREMENT=5` affected zero rows after id `50` existed,
  kept the exposed next value at `51`, and the next generated insert produced
  id `51`.
- `ALTER TABLE ai AUTO_INCREMENT 70` accepted the no-equals spelling and the next
  generated insert produced id `70`.
- `CREATE TABLE no_ai (id INT PRIMARY KEY) AUTO_INCREMENT=20` exposed
  `AUTO_INCREMENT=20`; `ALTER TABLE no_ai AUTO_INCREMENT=50` succeeded and left
  that exposed value unchanged.
- `CREATE TABLE no_ai (id INT PRIMARY KEY)` followed by
  `ALTER TABLE no_ai AUTO_INCREMENT=50` succeeded and left
  `information_schema.TABLES.AUTO_INCREMENT` null.

## MyLite Behavior

MyLite parses the option as a DDL table option, records the unsigned integer
literal in the prepared alter-table plan, and applies it after column and index
alter actions have produced the final in-memory table model.

When the final table model contains an `AUTO_INCREMENT` column, MyLite stores
the larger of the requested value and `MAX(auto_column) + 1` in the table
catalog. This keeps `INFORMATION_SCHEMA.TABLES` and `SHOW TABLE STATUS`
aligned with the next generated value before a later insert runs.

When the final table model has no `AUTO_INCREMENT` column, MyLite treats the
table option as an accepted no-op. It does not create a new catalog value and
does not replace a value previously recorded by `CREATE TABLE ... AUTO_INCREMENT`.
Independent column actions that remove an auto-increment column still clear the
catalog value.

MyLite keeps `affected_rows` at `0` for supported persistent alter-table option
updates, matching the observed MySQL persistent-table behavior for this slice.
Full temporary-table affected-row fidelity remains part of the broader
temporary `ALTER TABLE` work.

## Tests

Parser tests cover both `AUTO_INCREMENT=50` and `AUTO_INCREMENT 70` spellings as
`ALTER TABLE` items and assert the recorded integer literal.

Runtime tests compare the MySQL-observed generated-value behavior:

- creating an initial table with `AUTO_INCREMENT=10`;
- raising the next value to `50`;
- trying to lower the value below existing rows;
- using the no-equals spelling;
- accepting the option as a no-op on tables without an auto-increment column,
  both with and without an existing create-time table option value.
