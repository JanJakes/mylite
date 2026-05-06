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

If a table has no `AUTO_INCREMENT` column, MySQL accepts the statement but
leaves the exposed `AUTO_INCREMENT` table metadata null and generated values are
unaffected.

Observed MySQL 8.4.9 results:

- `CREATE TABLE ai (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=10`
  followed by two generated inserts produced ids `10` and `11`.
- `ALTER TABLE ai AUTO_INCREMENT=50` affected zero rows and the next generated
  insert produced id `50`.
- `ALTER TABLE ai AUTO_INCREMENT=5` affected zero rows after id `50` existed and
  the next generated insert produced id `51`.
- `ALTER TABLE ai AUTO_INCREMENT 70` accepted the no-equals spelling and the next
  generated insert produced id `70`.
- `ALTER TABLE no_ai AUTO_INCREMENT=50` succeeded and left
  `information_schema.TABLES.AUTO_INCREMENT` null.

## MyLite Behavior

MyLite parses the option as a DDL table option, records the unsigned integer
literal in the prepared alter-table plan, and applies it after column and index
alter actions have produced the final in-memory table model.

When the final table model contains an `AUTO_INCREMENT` column, MyLite stores
the requested value in the table catalog. The insert path already computes the
runtime next value as the maximum of the catalog value and
`MAX(auto_column) + 1`, which preserves MySQL's no-lowering behavior.

When the final table model has no `AUTO_INCREMENT` column, MyLite treats the
option as an accepted no-op and clears any stale auto-increment catalog value.

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
- accepting the option as a no-op on a table without an auto-increment column.
