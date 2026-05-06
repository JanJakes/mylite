# UPDATE IGNORE

## Scope

This feature specifies the first MyLite slice of MySQL-compatible
`UPDATE IGNORE` behavior for the single-table `UPDATE` surface that MyLite
already executes.

In scope:

- parsing and AST/runtime plan storage for `UPDATE IGNORE table_name SET ...`
- duplicate primary-key and unique-key row skipping with warning 1062
- child foreign-key update row skipping with warning 1452
- parent foreign-key `RESTRICT`, `NO ACTION`, and InnoDB-style `SET DEFAULT`
  row skipping with warning 1451
- affected rows, matched rows, warning counts, and storage side effects for
  mixed valid and ignored rows
- `foreign_key_checks = 0` behavior for the implemented foreign-key checks

Out of scope for this slice:

- joined or multi-table `UPDATE IGNORE`
- `LOW_PRIORITY`, CTEs, partition selection, and unsupported `LIMIT` forms
- data-conversion, range-clipping, truncation, and temporal coercion demotion
  caused specifically by `IGNORE`
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
updates. The first MyLite slice accepts only the no-priority single-table form:

```lemon
update_statement ::= UPDATE single_update_target SET update_assignment_list
    opt_where_clause opt_order_by_clause opt_update_limit_clause.

update_statement ::= UPDATE IGNORE single_update_target SET update_assignment_list
    opt_where_clause opt_order_by_clause opt_update_limit_clause.
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

For child foreign-key violations, MySQL skips each violating row, records
warning 1452, and continues. A row with a valid parent change is updated in the
same statement.

For parent foreign-key `RESTRICT`, `NO ACTION`, and observed InnoDB
`SET DEFAULT` behavior, MySQL skips the referenced parent row, records warning
1451, and continues updating other parent rows. If `foreign_key_checks = 0`,
the same update is allowed without warnings.

`UPDATE IGNORE` does not make parse errors, unknown target tables, unknown
assignment columns, unsupported expressions, allocation failures, or SQLite I/O
failures ignorable.

## MyLite Design

The parser records an `update_ignore` flag on single-table update AST nodes.
The DML copy layer carries that flag into `struct mylite_update_plan`.

Single-table update execution remains row-oriented:

1. Materialize the matched rowset using the existing `WHERE`, `ORDER BY`, and
   `LIMIT` logic.
2. Evaluate assignments into a candidate row.
3. Validate unique indexes, child foreign keys, and parent foreign keys.
4. If validation reports an ignorable conflict and the plan has `ignore = true`,
   append the MySQL warning, leave the stored row unchanged, and continue.
5. If the row changed and no ignorable conflict occurred, apply parent update
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

- parser acceptance and `update_ignore` AST flag for single-table updates
- parser rejection for `UPDATE LOW_PRIORITY ...` and current joined
  `UPDATE IGNORE ... JOIN ...` gap
- duplicate-key row skipping, warning code 1062, affected rows, and unchanged
  skipped rows
- duplicate-key mixed valid/skipped rows where non-key assignments on the
  skipped row do not apply
- child FK row skipping, warning code 1452, affected rows, and unchanged
  skipped rows
- parent FK row skipping, warning code 1451, affected rows, and updates to
  unreferenced rows
- `foreign_key_checks = 0` allowing the otherwise violating child update

Compatibility matrix and feedback task updates must keep conversion-error
demotion and joined `UPDATE IGNORE` listed as deferred until implemented.
