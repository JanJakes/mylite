# Parser Corpus DDL Option Surfaces

This slice reduces high-volume MySQL server-test parser failures around table
options and comma-separated `ALTER TABLE` actions. The goal is to admit MySQL
8.4-shaped DDL syntax that real test corpora use while keeping MyLite runtime
behavior explicit where embedded storage semantics are not implemented.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

## Scope

### CREATE TABLE table options

MySQL table options include storage-engine options such as `ENGINE`, row
storage hints, `TABLESPACE`, and MERGE-table options such as `UNION=(...)` and
`INSERT_METHOD=FIRST|LAST|NO`. MyLite already accepts a subset of table options
and records the supported metadata (`ENGINE`, character set, collation,
`AUTO_INCREMENT`, `COMMENT`, and selected storage/statistics hints).

This slice admits these additional parser surfaces:

- `TABLESPACE name`;
- `STORAGE DISK` and `STORAGE MEMORY`, including the common adjacent
  `TABLESPACE name STORAGE DISK|MEMORY` spelling as two placeholder options;
- `UNION=(table[, ...])` and `UNION (table[, ...])`;
- `INSERT_METHOD=NO|FIRST|LAST`;
- comma and whitespace separated ordering with the existing table options.

MyLite does not create external tablespaces, MERGE tables, or MyISAM table
unions. These options are represented as table-option placeholder nodes and are
ignored by the current base-table runtime in the same way as existing
storage/statistics hints, except where an existing supported option such as
`ENGINE`, `AUTO_INCREMENT`, or `COMMENT` has documented behavior.

### ALTER TABLE option/action sequencing

MySQL permits multiple `alter_option` clauses separated by commas. Existing
MyLite grammar accepts several multi-action combinations but rejects many valid
action kinds when they are placed inside a multi-action statement.

This slice admits additional comma-separated action forms in the multi-action
grammar:

- table rename: `RENAME [TO|AS] table_name`;
- column rename: `RENAME COLUMN old_name TO new_name`;
- index rename: `RENAME INDEX|KEY old_name TO new_name`;
- index visibility: `ALTER INDEX name VISIBLE|INVISIBLE`;
- column visibility: `ALTER [COLUMN] name SET VISIBLE|INVISIBLE`;
- table option actions after an earlier comma-separated action:
  `AUTO_INCREMENT`, table character set/collation, `CONVERT TO CHARACTER SET`,
  storage/statistics options, `COMMENT`;
- maintenance actions: `FORCE`, `DISABLE KEYS`, `ENABLE KEYS`;
- full-text, spatial, foreign-key, and check-constraint add/drop/alter forms
  that already have standalone statement nodes.

Runtime support remains conservative:

- multi-action forms composed only of currently supported action kinds keep
  executing through the existing multi-action executor;
- newly admitted but unsupported multi-action action kinds fail with the
  existing `multi-action ALTER TABLE does not support this action` diagnostic;
- standalone forms keep their current behavior;
- `ALTER TABLE ... ORDER BY ...` remains a standalone statement in this slice
  because comma-separated `ORDER BY` lists are ambiguous with following
  `ALTER TABLE` actions in MyLite's current Lemon grammar;
- no external tablespace lifecycle, MERGE table storage, MyISAM key enabling, or
  storage-engine conversion is added.

### Column attributes

MySQL column definitions can include visibility and, for spatial columns, an
`SRID` attribute. MyLite already has column-visibility metadata and spatial
column descriptors, and it already rejects spatial index creation when an SRID
attribute is missing. This slice admits:

- inline `VISIBLE` / `INVISIBLE` column attributes in `CREATE TABLE`,
  `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN`;
- `SRID integer` as a spatial column attribute placeholder.

Visibility metadata should be preserved for supported column-definition DDL
paths, and MyLite must keep MySQL's rule that a table has at least one visible
column. The SRID attribute is parse-time metadata only in this slice and is
accepted only on spatial columns: MyLite still does not implement geometry
values, spatial reference systems, or spatial index execution.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
table_option ::= TABLESPACE option_name.
table_option ::= STORAGE DISK.
table_option ::= STORAGE MEMORY.
table_option ::= UNION equal_opt LPAREN table_name_list RPAREN.
table_option ::= INSERT_METHOD equal_opt merge_insert_method.

merge_insert_method ::= NO.
merge_insert_method ::= FIRST.
merge_insert_method ::= LAST.
```

```lemon
alter_table_multi_action ::=
    RENAME table_rename_connector_opt table_name.
alter_table_multi_action ::=
    RENAME COLUMN identifier TO identifier.
alter_table_multi_action ::=
    RENAME INDEX identifier TO identifier.
alter_table_multi_action ::=
    RENAME KEY identifier TO identifier.
alter_table_multi_action ::=
    ALTER INDEX identifier VISIBLE.
alter_table_multi_action ::=
    ALTER INDEX identifier INVISIBLE.
alter_table_multi_action ::=
    ALTER column_keyword_opt identifier SET VISIBLE.
alter_table_multi_action ::=
    ALTER column_keyword_opt identifier SET INVISIBLE.
alter_table_multi_action ::= AUTO_INCREMENT equal_opt INTEGER.
alter_table_multi_action ::= table_options.
alter_table_multi_action ::= FORCE.
alter_table_multi_action ::= DISABLE KEYS.
alter_table_multi_action ::= ENABLE KEYS.
```

```lemon
column_attribute ::= VISIBLE.
column_attribute ::= INVISIBLE.
column_attribute ::= SRID INTEGER.
```

## Runtime Behavior

This is a parser-compatibility and placeholder slice. It does not require a
SQLite fork hook. The implementation is MyLite parser/AST work plus small
runtime dispatch updates so placeholder nodes are classified consistently.

Supported standalone and multi-action DDL keeps using MyLite's descriptor-owned
catalog mutation paths. Placeholder table options do not change physical
storage. Unsupported multi-action nodes intentionally fail through the current
multi-action unsupported-action path without durable catalog mutation.

## Tests

MySQL 8.4.9 expectations are verified with a focused shell script that runs
representative `CREATE TABLE` and `ALTER TABLE` forms against a real MySQL
runtime. MyLite tests cover:

- parser acceptance for table option placeholders;
- parser acceptance for newly admitted multi-action `ALTER TABLE` combinations;
- parser acceptance and metadata preservation for inline column visibility;
- runtime rejection for all-invisible table definitions and non-spatial SRID
  attributes;
- runtime success for supported multi-action combinations that previously failed
  at parse time;
- runtime unsupported diagnostics for newly admitted placeholder multi-action
  forms.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice moves selected DDL syntax from unsupported parser errors to parser
acceptance and explicit runtime placeholder handling. It does not mark
tablespace storage, MERGE tables, MyISAM maintenance actions, spatial SRID
semantics, or broad multi-action ALTER TABLE execution as fully supported.
