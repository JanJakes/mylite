# Baseline View DDL Options

## Status

This slice broadens MyLite's metadata-only view DDL support to cover the
common MySQL 8.4.9 view option surface:

```sql
CREATE [OR REPLACE]
    [ALGORITHM = {UNDEFINED | MERGE | TEMPTABLE}]
    [DEFINER = account]
    [SQL SECURITY {DEFINER | INVOKER}]
    VIEW view_name [(column_list)]
    AS select_statement
    [WITH [CASCADED | LOCAL] CHECK OPTION]

ALTER
    [ALGORITHM = {UNDEFINED | MERGE | TEMPTABLE}]
    [DEFINER = account]
    [SQL SECURITY {DEFINER | INVOKER}]
    VIEW view_name [(column_list)]
    AS select_statement
    [WITH [CASCADED | LOCAL] CHECK OPTION]

DROP VIEW [IF EXISTS] view_name [, view_name ...] [RESTRICT | CASCADE]
```

MyLite continues to store views as catalog metadata. It does not create native
SQLite views and does not execute queries through user-created views.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, `CREATE VIEW`:
  https://dev.mysql.com/doc/refman/8.4/en/create-view.html
- MySQL 8.4 Reference Manual, `ALTER VIEW`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-view.html
- MySQL 8.4 Reference Manual, `DROP VIEW`:
  https://dev.mysql.com/doc/refman/8.4/en/drop-view.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against local MySQL 8.4.9 in the `mylite-mysql-849` container:

- `CREATE ALGORITHM=MERGE SQL SECURITY INVOKER VIEW v (a, b) AS SELECT ... WITH
  LOCAL CHECK OPTION` records `CHECK_OPTION = LOCAL`,
  `SECURITY_TYPE = INVOKER`, the session client charset/collation, and a
  `SHOW CREATE VIEW` statement containing the algorithm, definer, security
  type, explicit column list, select definition, and check option.
- `WITH CHECK OPTION` without `LOCAL` or `CASCADED` records and renders
  `CASCADED`.
- `CREATE OR REPLACE ... VIEW existing_view ...` replaces the view definition.
  The replacement uses the clauses supplied by the replacement statement; when
  no `SQL SECURITY` clause is supplied, `DEFINER` is used.
- `ALTER ... VIEW existing_view ...` replaces the existing view definition and
  uses the same option syntax as `CREATE VIEW`.
- `DROP VIEW IF EXISTS missing_view RESTRICT` succeeds, emits a note for the
  missing view, and ignores `RESTRICT`; `CASCADE` is likewise parsed and
  ignored.
- Direct single-base-table column-projection views are `IS_UPDATABLE = YES` in
  `INFORMATION_SCHEMA.VIEWS`; `ALGORITHM=TEMPTABLE` views are
  `IS_UPDATABLE = NO`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_view_ddl_options_expectations.sh`.

## Scope

The implementation must add:

- parser support for `CREATE OR REPLACE VIEW`;
- parser support for pre-`VIEW` `ALGORITHM`, `DEFINER`, and `SQL SECURITY`
  clauses in documented order and in the common repeated-option order used by
  MySQL test corpora;
- parser support for explicit view column lists;
- parser support for `WITH CHECK OPTION`, `WITH CASCADED CHECK OPTION`, and
  `WITH LOCAL CHECK OPTION`;
- parser support for `ALTER VIEW` with the same options and select subset as
  `CREATE VIEW`;
- parser support for ignored `DROP VIEW ... RESTRICT` and
  `DROP VIEW ... CASCADE` tails;
- runtime storage of view metadata in existing catalog fields:
  `VIEW_DEFINITION`, `CHECK_OPTION`, `IS_UPDATABLE`, `DEFINER`,
  `SECURITY_TYPE`, `CHARACTER_SET_CLIENT`, `COLLATION_CONNECTION`, and
  `SHOW CREATE VIEW`;
- `CREATE OR REPLACE` replacement of existing view descriptors while rejecting
  replacement of base tables;
- `ALTER VIEW` replacement of existing view descriptors while rejecting missing
  or non-view targets;
- duplicate explicit view-column diagnostics; and
- MySQL-shaped metadata for the current direct single-base-table projection
  subset.

## Non-Goals

This slice intentionally does not add:

- query execution through user-created views;
- view DML, `WITH CHECK OPTION` enforcement, or privilege enforcement;
- full definer privilege behavior, account existence checks, role/security
  context changes, or binary logging;
- native SQLite views;
- arbitrary `SELECT` support inside view definitions beyond the existing direct
  projection subset;
- dependency extraction beyond the existing one-base-table
  `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` row;
- view-to-routine dependency extraction for user-created views;
- view algorithm planning effects; or
- full MySQL formatting of every valid view definition.

## MyLite Lemon Syntax

The intended grammar extension is:

```lemon
create_view_statement ::=
    CREATE view_or_replace_opt view_option_list_opt VIEW table_name
    view_column_list_opt AS select_statement view_check_option_opt.

alter_view_statement ::=
    ALTER view_option_list_opt VIEW table_name view_column_list_opt
    AS select_statement view_check_option_opt.

view_option_list_opt ::= .
view_option_list_opt ::= view_option_list.
view_option_list ::= view_option.
view_option_list ::= view_option_list view_option.

view_option ::= ALGORITHM equal_opt view_algorithm_value.
view_option ::= DEFINER equal_opt view_definer_account.
view_option ::= SQL SECURITY view_security_value.

view_check_option_opt ::= .
view_check_option_opt ::= WITH CHECK OPTION.
view_check_option_opt ::= WITH LOCAL CHECK OPTION.
view_check_option_opt ::= WITH CASCADED CHECK OPTION.

drop_view_statement ::=
    DROP VIEW drop_if_exists_opt table_name_list view_drop_tail_opt.
```

The parser owns only the compatibility syntax shape. The runtime continues to
validate whether the parsed select definition fits the current metadata-only
view subset.

## Runtime Semantics

`CREATE VIEW` and `ALTER VIEW` both lower into a planned metadata descriptor:

- view name and schema are resolved through existing writable object rules;
- the select statement is planned through the current direct projection
  `SELECT` subset;
- explicit view column lists replace inferred descriptor column names but do
  not widen the executable select subset;
- `ALGORITHM` is stored only in `SHOW CREATE VIEW`;
- `DEFINER` is stored as normalized account text;
- omitted definer defaults to the current embedded identity `root@%`;
- omitted security type defaults to `DEFINER`;
- omitted check option defaults to `NONE`;
- bare `WITH CHECK OPTION` normalizes to `CASCADED`;
- direct `MERGE` or `UNDEFINED` single-base-table projection views report
  `IS_UPDATABLE = YES`, while `TEMPTABLE` reports `NO`.

`CREATE OR REPLACE VIEW` atomically removes an existing view descriptor and
inserts the replacement descriptor in one catalog mutation. If the name resolves
to an existing base table, MyLite returns the existing table-exists diagnostic.

`ALTER VIEW` requires an existing view descriptor and atomically replaces it.
Missing targets use the existing missing-table diagnostic shape; non-view
targets use the existing not-view diagnostic.

`DROP VIEW ... RESTRICT` and `DROP VIEW ... CASCADE` ignore the tail option,
matching MySQL's parsed-but-ignored behavior.

## Storage, SQLite, And Performance

This is a MyLite catalog/parser feature. It uses existing catalog storage and
public SQLite APIs. It requires no SQLite fork patch, file-format change, or
new dependency.

Replacement creates one catalog mutation and does not scan user data. Planning
cost is the same as the existing metadata-only `CREATE VIEW` path plus small
option parsing and duplicate-name checks.

## Tests

Coverage must include:

- parser acceptance for `CREATE OR REPLACE`, `ALGORITHM`, `SQL SECURITY`,
  explicit column lists, check options, `ALTER VIEW`, and ignored drop tails;
- MySQL expectation coverage for `SHOW CREATE VIEW`,
  `INFORMATION_SCHEMA.VIEWS`, replacement, alteration, and ignored drop tails;
- runtime metadata rows for algorithm/security/check option/column list;
- runtime `CREATE OR REPLACE` replacement of an existing view;
- runtime `ALTER VIEW` replacement of an existing view; and
- regression coverage that base-table replacement and non-view alteration
  remain rejected.
