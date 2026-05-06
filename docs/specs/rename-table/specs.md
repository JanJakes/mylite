# RENAME TABLE and ALTER TABLE Table Rename

## Scope

This slice implements MySQL-compatible table rename syntax and runtime behavior
for MyLite base tables:

- `RENAME TABLE old_name TO new_name [, old_name2 TO new_name2 ...]`
- `ALTER TABLE old_name RENAME new_name`
- `ALTER TABLE old_name RENAME TO new_name`
- `ALTER TABLE old_name RENAME AS new_name`

`ALTER TABLE old_name RENAME TABLE new_name` is not accepted by MySQL 8.4.9
and remains a syntax error.

The first executable slice covers parser and AST support, selected-schema and
schema-qualified name resolution, metadata table-name updates, physical SQLite
table rename, internal index/statistics metadata rewrite, foreign-key catalog
rewrite for child and parent tables, cross-schema moves, collision diagnostics,
missing-table and missing-schema diagnostics, and statement atomicity for
multi-table rename.

Deferred behavior is explicit: temporary tables, views, triggers, CHECK
constraints, privilege propagation, metadata locks, and implicit commit
boundaries are not modeled in this slice. MyLite does not yet implement
temporary tables; top-level `RENAME TABLE` therefore looks only for base tables.
`ALTER TABLE ... RENAME` over temporary tables is deferred with temporary-table
support.

## Sources

The intended behavior is independently specified from:

- MySQL 8.4 Reference Manual, `RENAME TABLE`
  (`https://dev.mysql.com/doc/refman/8.4/en/rename-table.html`)
- MySQL 8.4 Reference Manual, `ALTER TABLE`
  (`https://dev.mysql.com/doc/refman/8.4/en/alter-table.html`)
- MySQL 8.4.9 runtime probes against `mylite-mysql-849`

## MySQL 8.4.9 Observations

`RENAME TABLE` accepts one or more rename pairs. Operations are evaluated
left-to-right, and MySQL documents the temporary-intermediary pattern for
swaps. If any pair fails, the statement fails and no table-name changes remain.

`RENAME TABLE current_db.tbl TO other_db.tbl` moves a base table between
databases. An unqualified source or target name resolves against the selected
database. If no database is selected and either side is unqualified, MySQL
returns 1046 / `3D000` with `No database selected`. A qualified source does not
make an unqualified target resolvable without a selected database.

Accepted `ALTER TABLE` rename forms are:

- `ALTER TABLE t RENAME t2`
- `ALTER TABLE t RENAME TO t2`
- `ALTER TABLE t RENAME AS t2`

`ALTER TABLE t RENAME TABLE t2` returns 1064 / `42000` syntax error.

Observed diagnostics:

- Existing target, including `old_name == new_name`: 1050 / `42S01`.
- Missing source table: 1146 / `42S02`.
- Missing target schema: 1049 / `42000`.
- Temporary table with top-level `RENAME TABLE`: 1146 / `42S02`, because the
  statement searches base tables.

## MyLite Semantics

Name resolution:

- Qualified source names use their explicit schema.
- Unqualified source names require the selected schema.
- Qualified target names use their explicit schema.
- Unqualified target names require and use the selected schema.
- Source and target schemas must exist and must not be MyLite system schemas.

Validation runs before mutation and simulates the left-to-right rename sequence.
At each pair:

- the current source name must exist in the simulated base-table namespace;
- the current target name must not exist in that simulated namespace;
- a target vacated by an earlier pair may be reused;
- a source consumed by an earlier pair is no longer available under the old name;
- a name produced by an earlier pair may be used as a later source.

Runtime execution is wrapped in MyLite statement atomicity. Outside an explicit
transaction MyLite starts and commits a SQLite transaction; inside one it uses a
savepoint. Each pair renames the physical SQLite table and rewrites internal
metadata in the same atomic scope:

- `__mylite_table_catalog.table_schema/table_name`
- `__mylite_column_catalog.table_schema/table_name`
- `__mylite_index_catalog.table_schema/table_name/index_schema`
- `__mylite_foreign_key_catalog` child-side
  `constraint_schema/table_schema/table_name/constraint_name` and parent-side
  `unique_constraint_schema/referenced_table_schema/referenced_table_name`

`INFORMATION_SCHEMA.TABLES`, `INFORMATION_SCHEMA.COLUMNS`, and
`INFORMATION_SCHEMA.STATISTICS` therefore expose the new table name after a
successful rename. Foreign-key metadata views and `SHOW CREATE TABLE` expose
the renamed child and parent references. Data rows and index metadata are
preserved. A failed multi-table rename leaves physical tables and metadata
under their original names.

When a child table is renamed, MySQL-style generated constraint names matching
`old_table_ibfk_N` are rewritten to `new_table_ibfk_N`; explicit names that do
not match that generated-name pattern are preserved. Parent renames update the
referenced table schema/name even when `foreign_key_checks=0`.

`ALTER TABLE ... RENAME` is executed through the same single-pair runtime path
as `RENAME TABLE`. Mixing table rename with other `ALTER TABLE` actions is not
part of this slice and returns a deterministic unsupported diagnostic before
mutation if parsed.

## Grammar Sketch

The grammar is authored for MyLite Lemon syntax:

```lemon
statement(A) ::= rename_table_statement(B). {
    A = B;
}

rename_table_statement(A) ::= RENAME(T) TABLE rename_table_pair_list(P). {
    A = mylite_sql_parser_make_rename_table_statement(state, T, P);
}

rename_table_pair_list(A) ::= rename_table_pair(B). {
    A = mylite_sql_parser_make_rename_table_pair_list(state, B);
}
rename_table_pair_list(A) ::= rename_table_pair_list(B) COMMA rename_table_pair(C). {
    A = mylite_sql_parser_append_rename_table_pair(state, B, C);
}

rename_table_pair(A) ::= table_name(O) TO(T) table_name(N). {
    A = mylite_sql_parser_make_rename_table_pair(state, O, T, N);
}

alter_table_action(A) ::= RENAME(T) table_name(N). {
    A = mylite_sql_parser_make_alter_table_rename_table_action(state, T, N);
}
alter_table_action(A) ::= RENAME(T) TO table_name(N). {
    A = mylite_sql_parser_make_alter_table_rename_table_action(state, T, N);
}
alter_table_action(A) ::= RENAME(T) AS table_name(N). {
    A = mylite_sql_parser_make_alter_table_rename_table_action(state, T, N);
}
```

## Test Plan

Parser tests cover the accepted top-level and `ALTER TABLE` syntax forms,
schema-qualified targets, multi-pair lists, quoted identifiers, and rejected
forms including `ALTER TABLE ... RENAME TABLE ...`, missing targets, and trailing
commas.

Runtime tests cover:

- no selected database for unqualified names;
- data and metadata preservation across single-table rename;
- `INFORMATION_SCHEMA.STATISTICS` rows under the new table name;
- physical SQLite table rename;
- cross-schema `RENAME TABLE` moves;
- cross-schema `ALTER TABLE ... RENAME` moves;
- foreign-key metadata rewrites for parent renames, child renames, generated
  child constraint names, cross-schema child/parent moves, and
  `foreign_key_checks=0`;
- selected-schema target resolution when the source is qualified;
- swap via a temporary intermediary name;
- existing-target, same-name, missing-source, and missing-target-schema errors;
- rollback/no partial mutation for multi-pair failures;
- deferred temporary-table behavior documented as base-table lookup only.
