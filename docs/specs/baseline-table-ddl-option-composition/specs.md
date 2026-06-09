# Baseline Table DDL Option Composition

## Scope

This slice broadens table DDL syntax that appears in real MySQL-oriented test
corpora and can be handled without new storage-engine behavior. It covers:

- `DROP TABLES` as a MySQL 8.4.9 runtime-accepted alias of `DROP TABLE`.
- `DROP [TEMPORARY] TABLE ... RESTRICT` and `DROP [TEMPORARY] TABLE ... CASCADE`
  as accepted no-op portability keywords.
- `SHOW [EXTENDED] [FULL] TABLES` with existing `FROM` / `IN`, `LIKE`, and
  `WHERE` filtering. `EXTENDED` is accepted and ignored because MyLite does not
  create hidden failed-ALTER scratch tables.
- `CHECK TABLES` as a runtime-accepted alias of `CHECK TABLE`.
- `ALTER TABLE` parenthesized `ADD [COLUMN] (col definition[, ...])`.
- `ALTER TABLE` multi-action lists with trailing or leading `ALGORITHM` /
  `LOCK` options where the underlying actions are already supported.
- `ALTER TABLE` table option actions for the existing descriptor-owned storage
  and statistics metadata subset: `ENGINE=InnoDB`, `ROW_FORMAT`,
  `KEY_BLOCK_SIZE`, `PACK_KEYS`, `CHECKSUM`, `STATS_PERSISTENT`,
  `STATS_AUTO_RECALC`, and `STATS_SAMPLE_PAGES`.
- Compatibility parsing and metadata persistence for legacy or storage-engine
  advisory table options that MySQL exposes through introspection: `MIN_ROWS`,
  `MAX_ROWS`, `AVG_ROW_LENGTH`, and `DELAY_KEY_WRITE`.

This slice does not implement partition clauses, physical engine conversion,
loaded storage statistics, MyISAM key disabling, physical effects for
`MIN_ROWS` / `MAX_ROWS` / `AVG_ROW_LENGTH` / `DELAY_KEY_WRITE`, or unsupported
multi-action operations such as table rename, column drop, or foreign-key
creation inside one ALTER statement.

## Compatibility Authorities

The intended behavior is based on the MySQL 8.4 Reference Manual pages for
`ALTER TABLE`, `DROP TABLE`, `SHOW TABLES`, and `CHECK TABLE`, plus MySQL
8.4.9 runtime probes captured in
`packages/libmylite/tests/mysql_baseline_table_ddl_option_composition_expectations.sh`.

Observed MySQL 8.4.9 behavior:

- `DROP TABLES t` is accepted and drops `t`.
- `DROP TABLE t RESTRICT` and `DROP TABLE t CASCADE` are accepted and the tail
  keywords do not change the drop.
- `SHOW EXTENDED FULL TABLES` returns the same user-visible table rows as
  `SHOW FULL TABLES` in the ordinary case.
- `CHECK TABLES t` returns the same shape as `CHECK TABLE t`.
- `ALTER TABLE t ADD COLUMN (a INT, b INT)` is accepted.
- `ALTER TABLE t ALGORITHM=INSTANT, ADD COLUMN c INT` is accepted.
- `ALTER TABLE t ADD COLUMN c INT, ADD COLUMN d INT, ALGORITHM=INSTANT,
  LOCK=DEFAULT` is accepted.
- `ALTER TABLE t ENGINE=InnoDB, ROW_FORMAT=DYNAMIC, PACK_KEYS=1, CHECKSUM=1`
  updates SHOW CREATE table-option text for supported metadata fields.
- `MIN_ROWS`, `MAX_ROWS`, `AVG_ROW_LENGTH`, and `DELAY_KEY_WRITE` are accepted
  by MySQL as table options and are visible in `SHOW CREATE TABLE` and
  `INFORMATION_SCHEMA.TABLES.CREATE_OPTIONS`.

## MyLite Design

The implementation stays in MyLite's parser, catalog, and runtime layers. No
SQLite fork hook is required because all supported behavior is parser
admission, descriptor metadata mutation, or an existing DDL path.

Supported descriptor-owned storage/statistics options are stored in the same
catalog fields already used by `CREATE TABLE` and `CREATE TABLE ... LIKE`.
`ALTER TABLE` applies them in the active catalog mutation so rollback and
failure handling remain consistent with existing atomic DDL behavior.

`ENGINE=InnoDB` follows the existing engine policy. Other engine names follow
the same SQL-mode sensitive validation used by `CREATE TABLE`: MyLite is an
embedded InnoDB-compatible surface and does not switch physical storage
engines.

Legacy/advisory options are descriptor metadata only. MyLite persists and
renders them for compatibility with schema-introspection code, but they do not
change physical storage layout or query planning.

## Syntax

Independently authored Lemon-style grammar sketch:

```lemon
drop_table_statement ::=
    DROP table_or_tables drop_if_exists_opt table_name_list drop_table_tail_opt.
drop_temporary_table_statement ::=
    DROP TEMPORARY table_or_tables drop_if_exists_opt table_name_list drop_table_tail_opt.
drop_table_tail_opt ::= .
drop_table_tail_opt ::= RESTRICT.
drop_table_tail_opt ::= CASCADE.

show_tables_statement ::=
    SHOW show_extended_opt show_full_opt TABLES show_tables_source_opt show_tables_filter_opt.
show_extended_opt ::= .
show_extended_opt ::= EXTENDED.

table_maintenance_statement ::=
    CHECK table_or_tables table_name_list check_table_option_list_opt.

alter_table_add_column_statement ::=
    ALTER TABLE table_name ADD column_keyword_opt LPAREN column_definition RPAREN
    column_position_opt alter_table_option_tail_opt.
alter_table_multi_action_statement ::=
    ALTER TABLE table_name alter_table_multi_action_list alter_table_option_tail_opt.
alter_table_multi_action_statement ::=
    ALTER TABLE table_name alter_table_algorithm_lock_option_list COMMA
    alter_table_multi_action_list alter_table_option_tail_opt.

alter_table_storage_option_statement ::=
    ALTER TABLE table_name alter_table_storage_option_list alter_table_option_tail_opt.
alter_table_storage_option_list ::= alter_table_storage_option.
alter_table_storage_option_list ::= alter_table_storage_option_list COMMA alter_table_storage_option.
alter_table_storage_option ::= ENGINE equal_opt option_name.
alter_table_storage_option ::= ROW_FORMAT equal_opt row_format_option_value.
alter_table_storage_option ::= KEY_BLOCK_SIZE equal_opt INTEGER.
alter_table_storage_option ::= PACK_KEYS equal_opt table_default_or_integer_option_value.
alter_table_storage_option ::= CHECKSUM equal_opt INTEGER.
alter_table_storage_option ::= STATS_PERSISTENT equal_opt table_default_or_integer_option_value.
alter_table_storage_option ::= STATS_AUTO_RECALC equal_opt table_default_or_integer_option_value.
alter_table_storage_option ::= STATS_SAMPLE_PAGES equal_opt table_default_or_integer_option_value.
alter_table_storage_option ::= MIN_ROWS equal_opt INTEGER.
alter_table_storage_option ::= MAX_ROWS equal_opt INTEGER.
alter_table_storage_option ::= AVG_ROW_LENGTH equal_opt INTEGER.
alter_table_storage_option ::= DELAY_KEY_WRITE equal_opt table_default_or_integer_option_value.
```

## Runtime Semantics

- `SHOW EXTENDED TABLES` reuses the existing `SHOW TABLES` executor. MyLite has
  no hidden failed-ALTER scratch table namespace to add.
- `CHECK TABLES` reuses the existing `CHECK TABLE` executor and result shape.
- `DROP TABLE` tail keywords are ignored.
- Parenthesized single-column `ADD COLUMN` uses the existing add-column path.
  Parenthesized multi-column additions lower to the existing multi-action
  executor and keep its atomic rollback behavior.
- Multi-action online-DDL options are validated once on the composed statement.
  Individual action bodies keep their current support checks.
- Supported table option metadata updates are catalog-only and report zero
  affected rows.
- Legacy/advisory table options report success without warnings. They are
  stored in the MyLite catalog, cloned by `CREATE TABLE ... LIKE`, and rendered
  in `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES.CREATE_OPTIONS`.

## Tests

The MySQL expectation script verifies the observed MySQL 8.4.9 behavior for the
syntax and metadata surface above. Parser tests cover AST admission and action
composition. Runtime tests cover descriptor metadata updates, SHOW CREATE /
INFORMATION_SCHEMA readback where relevant, and rollback-preserving multi-action
forms.
