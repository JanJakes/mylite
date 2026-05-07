# UPDATE IGNORE

## Scope

This feature specifies the first MyLite slice of MySQL-compatible
`UPDATE IGNORE` behavior for the single-table and joined `UPDATE` surfaces that
MyLite already executes.

In scope:

- parsing and AST/runtime plan storage for `UPDATE IGNORE table_name SET ...`
  and `UPDATE IGNORE ... JOIN ... SET ...`
- duplicate primary-key and unique-key row skipping with warning 1062
- data-conversion, temporal, string-truncation, and explicit `NULL`
  not-null assignment demotion for single-table and joined updates
- child foreign-key update row skipping with warning 1452
- parent foreign-key `RESTRICT`, `NO ACTION`, and InnoDB-style `SET DEFAULT`
  row skipping with warning 1451
- affected rows, matched rows, warning counts, and storage side effects for
  mixed valid and ignored rows
- `foreign_key_checks = 0` behavior for the implemented foreign-key checks

Out of scope for this slice:

- `LOW_PRIORITY`, CTEs, partition selection, and unsupported `LIMIT` forms
- range-clipping and conversion-demotion behavior beyond the covered numeric,
  temporal, string-length, and explicit `NULL` not-null assignment cases
- replication-safety warnings, protocol OK-packet text, triggers, privileges,
  views, and storage engines other than MyLite's InnoDB-compatible behavior

## Sources

- MySQL 8.4 Reference Manual, `UPDATE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/update.html
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html
- Existing MyLite specs:
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/insert-ignore/specs.md`
  - `docs/specs/foreign-key-support/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849` with `mysql:8.4.9`, using `mysql -uroot --table`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Behavior

`IGNORE` appears after the optional `LOW_PRIORITY` modifier and before the table
reference list. MySQL accepts it for both single-table and multiple-table
updates. MyLite accepts the no-priority single-table form and the supported
joined-update table-reference form:

```lemon
update_statement ::= UPDATE single_update_target SET update_assignment_list
    opt_where_clause opt_order_by_clause opt_update_limit_clause.

update_statement ::= UPDATE IGNORE single_update_target SET update_assignment_list
    opt_where_clause opt_order_by_clause opt_update_limit_clause.

update_statement ::= UPDATE IGNORE joined_update_table_references SET
    update_assignment_list opt_where_clause.
```

For duplicate-key conflicts, MySQL does not update the conflicting row, records
warning 1062, and continues with later candidate rows. If other assigned
columns would have changed, those changes are skipped along with the duplicate
key assignment. `ROW_COUNT()` counts only rows actually changed.

Verified duplicate-key probe:

```sql
CREATE TABLE dup_t(id INT PRIMARY KEY, u INT UNIQUE, v INT);
INSERT INTO dup_t VALUES (1,1,10),(2,2,20),(3,3,30);
UPDATE IGNORE dup_t
SET u = CASE id WHEN 2 THEN 1 ELSE 30 END
WHERE id IN (2,3) ORDER BY id;
```

MySQL changes row `id=3`, skips row `id=2`, reports affected rows `1`, and
records one warning 1062.

The same duplicate-key demotion applies to the supported joined-update surface.
Verified joined duplicate-key probe:

```sql
CREATE TABLE join_dup(id INT PRIMARY KEY, u INT UNIQUE, v INT);
CREATE TABLE join_src(id INT PRIMARY KEY, keep INT);
INSERT INTO join_dup VALUES (1,1,10),(2,2,20),(3,3,30);
INSERT INTO join_src VALUES (2,1),(3,1);
UPDATE IGNORE join_dup JOIN join_src ON join_dup.id = join_src.id
SET join_dup.u = CASE join_dup.id WHEN 2 THEN 1 ELSE 30 END,
    join_dup.v = join_dup.v + 1
WHERE join_src.keep = 1;
```

MySQL changes row `id=3`, skips row `id=2`, reports affected rows `1`, and
records one warning 1062.

For child foreign-key violations, MySQL skips each violating row, records
warning 1452, and continues. A row with a valid parent change is updated in the
same statement.

For parent foreign-key `RESTRICT`, `NO ACTION`, and observed InnoDB
`SET DEFAULT` behavior, MySQL skips the referenced parent row, records warning
1451, and continues updating other parent rows. If `foreign_key_checks = 0`,
the same update is allowed without warnings.

For invalid assignment values, `UPDATE IGNORE` applies the same practical
coercions as non-strict updates rather than skipping the row. Observed MySQL
8.4.9 behavior:

```sql
CREATE TABLE coerce_t(
  id INT PRIMARY KEY,
  i INT NOT NULL,
  d DATE NOT NULL,
  v VARCHAR(3) NOT NULL
);
INSERT INTO coerce_t VALUES (1,1,'2024-01-01','abc');
UPDATE IGNORE coerce_t
SET i = 'bad', d = '2022-31-01', v = 'abcdef'
WHERE id = 1;
```

MySQL changes the row to `i = 0`, `d = '0000-00-00'`, and `v = 'abc'`,
reports one affected row, and records warnings 1366, 1265, and 1265 in
assignment order. Explicit `NULL` assignments to `NOT NULL` columns coerce to
implicit defaults with warning 1048 per column.

The joined-update form uses the same assignment coercions. Verified joined
coercion probe:

```sql
CREATE TABLE join_coerce(
  id INT PRIMARY KEY,
  i INT NOT NULL,
  d DATE NOT NULL,
  v VARCHAR(3) NOT NULL
);
CREATE TABLE join_source(id INT PRIMARY KEY, marker INT);
INSERT INTO join_coerce VALUES
  (1,1,'2024-01-01','abc'),
  (2,2,'2024-01-02','def');
INSERT INTO join_source VALUES (1,1),(2,1);
UPDATE IGNORE join_coerce
JOIN join_source ON join_coerce.id = join_source.id
SET join_coerce.i = 'bad',
    join_coerce.d = '2022-31-01',
    join_coerce.v = 'abcdef'
WHERE join_coerce.id = 1;
```

MySQL changes row `id=1` to `i = 0`, `d = '0000-00-00'`, and `v = 'abc'`,
reports one affected row, and records warnings 1366, 1265, and 1265.
The joined explicit-`NULL` probe reports one affected row, stores implicit
defaults, and records three warning 1048 entries in assignment order.

`UPDATE IGNORE` does not make parse errors, unknown target tables, unknown
assignment columns, unsupported expressions, allocation failures, or SQLite I/O
failures ignorable.

## MyLite Design

The parser records an `update_ignore` flag on single-table and joined update
AST nodes.
The DML copy layer carries that flag into `struct mylite_update_plan`.

Update execution remains row-oriented:

1. Materialize the matched rowset using the existing `WHERE`, `ORDER BY`, and
   `LIMIT` logic for single-table updates, or the joined rowset for joined
   updates.
2. Evaluate assignments into a candidate row.
3. Validate unique indexes, child foreign keys, and parent foreign keys.
4. If assignment validation reports an ignorable conversion or `NOT NULL`
   condition and the plan has `ignore = true`, keep the coerced candidate value,
   append the MySQL warning, and continue validating the row.
5. If validation reports an ignorable conflict and the plan has `ignore = true`,
   append the MySQL warning, leave the stored row unchanged, and continue.
6. If the row changed and no ignorable conflict occurred, apply parent update
   referential actions and write the candidate row.

Warnings use the same diagnostics area as `INSERT IGNORE`. Error text is
MySQL-shaped and includes the table and constraint/key names available from
MyLite metadata. Foreign-key warning messages intentionally keep the current
MyLite constraint message shape until the broader SQLSTATE/error-message pass
adds referenced-column details consistently.

`UPDATE IGNORE` still participates in statement atomicity: nonignorable
failures roll back all rows changed by the statement, while ignorable rows are
skipped inside an otherwise successful statement.

## Tests

Runtime tests must cover:

- parser acceptance and `update_ignore` AST flag for single-table and joined
  updates
- parser rejection for `UPDATE LOW_PRIORITY ...`
- duplicate-key row skipping, warning code 1062, affected rows, and unchanged
  skipped rows
- duplicate-key mixed valid/skipped rows where non-key assignments on the
  skipped row do not apply
- invalid integer, invalid date, string truncation, and explicit `NULL`
  `NOT NULL` assignment coercion with MySQL warning codes, affected rows, and
  stored values
- joined `UPDATE IGNORE` duplicate-key skipping, invalid-assignment coercion,
  and explicit `NULL` `NOT NULL` assignment coercion
- child FK row skipping, warning code 1452, affected rows, and unchanged
  skipped rows
- parent FK row skipping, warning code 1451, affected rows, and updates to
  unreferenced rows
- `foreign_key_checks = 0` allowing the otherwise violating child update

Compatibility matrix and feedback task updates must keep remaining insert-path
conversion demotion and range-clipping edge cases listed as deferred until
implemented.
