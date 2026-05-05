# Multi-table DELETE

## Scope

This feature extends executable `DELETE` beyond the existing single-table form
to MySQL multi-table delete syntax over MyLite-supported persistent base
tables:

- `DELETE target [, target] ... FROM table_references [WHERE expr]`
- `DELETE FROM target [, target] ... USING table_references [WHERE expr]`
- target names written as a visible table name or alias
- optional compatibility `.*` suffix on each delete target
- `INNER`, `CROSS`, comma, `LEFT`, and `RIGHT` joins already supported by the
  MyLite joined-`SELECT` row-source engine
- `ON` and `USING` join conditions over the supported scalar expression subset
- optional `WHERE` over the supported scalar expression subset
- deletion from one or more target tables, while other joined tables may be
  read-only search tables
- affected rows as the number of unique physical rows deleted across all
  target tables
- statement atomicity and rollback on binding, expression, or storage failure

Out of scope for this slice:

- `LOW_PRIORITY`, `QUICK`, and `IGNORE`
- common table expressions before `DELETE`
- partition selection
- `ORDER BY` and `LIMIT`, which MySQL does not accept for multi-table
  `DELETE`
- `NATURAL` joins, `STRAIGHT_JOIN`, parenthesized table references, derived
  tables, views, CTEs, subqueries, table functions, index hints, and
  optimizer hints
- information-schema or performance-schema delete targets
- privileges, triggers, foreign keys, cascading actions, binary logging,
  replication, table locks, and optimizer plan details

The implementation must reuse MyLite's independently authored joined-row
source and expression evaluator rather than delegating MySQL-visible join,
alias, warning, and expression semantics to SQLite.

## Sources

- MySQL 8.4 Reference Manual, `DELETE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/delete.html
- MySQL 8.4 Reference Manual, `JOIN` clause:
  https://dev.mysql.com/doc/refman/8.4/en/join.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- Existing MyLite specs:
  - `docs/specs/delete-single-table/specs.md`
  - `docs/specs/inner-joins/specs.md`
  - `docs/specs/outer-joins/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/expression-operator-foundation/specs.md`

Runtime observations were verified against Docker container
`mylite-mysql-849-regexp` running MySQL `8.4.9` with:

```sh
docker exec -i mylite-mysql-849-regexp mysql -h127.0.0.1 -uroot -pmylite \
  --batch --raw --show-warnings --force
```

The verified server reported `VERSION() = 8.4.9`,
`@@version_comment = MySQL Community Server - GPL`, and the default SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 Behavior Summary

Multi-table `DELETE` has two equivalent target-list placements. The first form
places targets before `FROM`; the second places targets between `FROM` and
`USING`. In both forms, `table_references` supplies the joined search row
source. Only rows from the target list are deleted.

Representative verified behavior:

| SQL | MySQL outcome |
| --- | --- |
| `DELETE a FROM a JOIN b ON a.id=b.a_id WHERE b.keep_flag=0` | Deletes unique matched `a` rows. Multiple matching `b` rows for one `a` row still delete that `a` row once. |
| `DELETE a, b FROM a JOIN b ON a.id=b.a_id JOIN c ON b.id=c.b_id WHERE c.marker=7` | Deletes matched rows from `a` and `b`; `c` is search-only. |
| `DELETE FROM aa, bb USING a AS aa JOIN b AS bb ON aa.id=bb.a_id WHERE bb.keep_flag=0` | Deletes through aliases declared in the `USING` table references. |
| `DELETE a.* FROM a LEFT JOIN b ON a.id=b.a_id WHERE b.id IS NULL` | Accepts `.*`; deletes unmatched `a` rows found by the outer join. |
| `DELETE b FROM a RIGHT JOIN b ON a.id=b.a_id WHERE a.id IS NULL` | Deletes unmatched right-side `b` rows. |

Affected rows are summed over unique deleted physical rows from all target
tables. In the verified alias `USING` fixture, deleting three `a` rows and
four `b` rows returned `ROW_COUNT() = 7`.

Duplicate target entries are rejected before mutation. The verified probe
`DELETE a, a FROM a JOIN b ON a.id=b.a_id WHERE a.id=1` failed with 1066
`Not unique table/alias: 'a'` and left rows unchanged.

When a table reference has an alias, the delete target must use that alias.
The verified probe `DELETE a FROM a AS aa JOIN b ON aa.id=b.a_id` failed with
1109 `Unknown table 'a' in MULTI DELETE`.

A target name not present in the visible table-reference namespace fails with
1109 `Unknown table '<name>' in MULTI DELETE`. Duplicate aliases in
`table_references` fail with 1066. Target alias declarations are not valid in
the delete-target list; `DELETE aa AS x FROM ...` is a syntax error.

`ORDER BY` and `LIMIT` after a multi-table `DELETE` are syntax errors in
MySQL. MyLite should keep them syntactically rejected for multi-table forms
while preserving the existing single-table `ORDER BY`/`LIMIT` support.

## MyLite Behavior

### Parser and AST

Add multi-table `DELETE` AST support without changing the single-table
statement shape:

- `MYLITE_SQL_AST_DELETE_TARGET_LIST`
- `MYLITE_SQL_AST_DELETE_TARGET_NAME`
- reuse `MYLITE_SQL_AST_FROM_TABLE_REFERENCES` for `table_references`
- reuse `MYLITE_SQL_AST_WHERE_CLAUSE`

The statement node records a form marker so runtime code can distinguish:

- single-table delete
- `DELETE targets FROM table_references`
- `DELETE FROM targets USING table_references`

### Lemon Grammar Snippets

These snippets describe MyLite's intended grammar shape and are independently
authored:

```lemon
delete_statement ::= DELETE FROM single_delete_target opt_where_clause
    opt_order_by_clause opt_delete_limit_clause.

delete_statement ::= DELETE delete_target_list FROM table_references
    opt_where_clause.

delete_statement ::= DELETE FROM delete_target_list USING table_references
    opt_where_clause.

delete_target_list ::= delete_target_name.
delete_target_list ::= delete_target_list COMMA delete_target_name.

delete_target_name ::= identifier opt_delete_target_star.
delete_target_name ::= identifier DOT identifier opt_delete_target_star.

opt_delete_target_star ::= .
opt_delete_target_star ::= DOT STAR.
```

`FROM`, `USING`, `WHERE`, `ORDER`, and `LIMIT` remain clause boundaries.
Target-list aliases are not accepted.

### Binding

Runtime binding should:

1. Bind `table_references` with the same resolver used by joined `SELECT`.
2. Reject system-schema target tables.
3. Bind join predicates and `WHERE` through the joined-row expression binder.
4. Resolve every delete target against the visible table-reference namespace:
   alias when present, otherwise the base table name; schema-qualified base
   table names only when the visible table is not aliased.
5. Reject targets not found in the row source with the MySQL 1109-style
   multi-delete diagnostic.
6. Reject duplicate target entries with the MySQL 1066-style duplicate
   table/alias diagnostic.

### Execution

The executor materializes matching joined rows over pre-delete values. Each
joined row carries stable physical row identities for base tables. For every
matched joined row, the executor records the row identity for each delete
target that is non-null in the joined row. Null-extended outer-join targets do
not produce row identities and are therefore not deleted.

Before mutation, row identities are deduplicated per target table. The physical
deletes run inside one statement atomicity boundary. Any failure rolls back all
target tables. On success, affected rows equals the total number of unique
deleted rows across all targets.

The executor must preserve existing `AUTO_INCREMENT` sequence behavior: delete
does not rewrite the next generated value and does not change
`LAST_INSERT_ID()`.

## Test Expectations

Implementation tests should cover:

- parser acceptance for both multi-table forms and `.*` targets
- parser rejection for multi-table `ORDER BY` and `LIMIT`
- one-target inner-join delete with duplicate join matches deleting the target
  row once
- two-target delete where a third joined table is search-only
- `USING` form with aliases
- left-join unmatched-row delete
- right-join unmatched-row delete
- duplicate delete target rejection before mutation
- base table target rejected when the row source aliases that table
- missing target rejected before mutation
- affected rows across multiple target tables
- rollback when a strict-mode warning in `WHERE` is promoted to an error

## Compatibility Status

After implementation and tests, `DELETE` (multi-table) should move from
unsupported to implemented with documented gaps. The status remains limited to
MyLite-supported persistent base tables and the existing joined-row source
surface.
