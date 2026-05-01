# Primary keys and AUTO_INCREMENT

## Scope

This feature extends MyLite's parse-only `CREATE TABLE` surface for primary-key
and `AUTO_INCREMENT` declarations over the column types and column attributes
implemented by earlier roadmap tasks:

- inline `PRIMARY KEY`
- inline `KEY` as MySQL's inline primary-key shorthand
- inline `AUTO_INCREMENT`
- table-level `PRIMARY KEY (...)`
- `CONSTRAINT PRIMARY KEY (...)` and `CONSTRAINT name PRIMARY KEY (...)`
- optional primary-key index name after `PRIMARY KEY`
- key-part lists over column identifiers, including optional prefix length and
  optional `ASC` or `DESC`
- primary-key index type syntax `USING BTREE` and `USING HASH`, before or after
  the key-part list
- primary-key index options `KEY_BLOCK_SIZE [=] integer`, `COMMENT 'string'`,
  `VISIBLE`, `INVISIBLE`, `ENGINE_ATTRIBUTE [=] 'string'`, and
  `SECONDARY_ENGINE_ATTRIBUTE [=] 'string'`

The task remains parse-only. Valid `CREATE TABLE` statements covered by this
feature prepare as `MYLITE_UNSUPPORTED`; no SQLite table is created and no
MyLite catalog, column, or index rows are written. Allocation, uniqueness
enforcement, executable table DDL, implicit primary-key `NOT NULL` metadata,
`SHOW CREATE TABLE`, warnings, duplicate-key diagnostics, and information-schema
primary-key/index metadata are deferred.

Unique and secondary indexes are intentionally out of scope for this task,
except where the grammar must distinguish them from primary-key syntax.

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, Using `AUTO_INCREMENT`:
  https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html
- MySQL 8.4 Reference Manual, InnoDB `AUTO_INCREMENT` handling:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html
- MySQL 8.4 Reference Manual, Generated Invisible Primary Keys:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-gipks.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

### Inline primary-key and AUTO_INCREMENT declarations

Runtime probes against MySQL 8.4.9 show these representative behaviors:

| Declaration fragment | MySQL behavior |
| --- | --- |
| `a INT PRIMARY KEY` | accepted; shown as `a int NOT NULL` plus `PRIMARY KEY (a)` |
| `a INT KEY` | accepted as an inline primary-key shorthand and normalized like `PRIMARY KEY` |
| `a BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY` | accepted; shown as `bigint unsigned NOT NULL AUTO_INCREMENT` plus primary key |
| `a BIGINT AUTO_INCREMENT` with no key | parses but errors during DDL validation because an auto column must be indexed |
| `a VARCHAR(10) AUTO_INCREMENT PRIMARY KEY` | parses but errors during DDL validation because the type is invalid for `AUTO_INCREMENT` |
| `a DECIMAL AUTO_INCREMENT PRIMARY KEY` | parses but errors during DDL validation |
| `a FLOAT AUTO_INCREMENT PRIMARY KEY` | parses but errors during DDL validation |
| `a BIGINT AUTO_INCREMENT PRIMARY KEY DEFAULT 1` | parses but errors during DDL validation because of the explicit default |
| `a INT NULL PRIMARY KEY` | parses but errors during DDL validation because primary-key parts must be not nullable |
| `a INT NULL AUTO_INCREMENT PRIMARY KEY` | parses but errors during DDL validation |
| duplicate `AUTO_INCREMENT` columns | parse but error during DDL validation |
| duplicate primary-key declarations | parse but error during DDL validation |
| `a INT UNIQUE KEY` | accepted as a unique secondary index and left for the secondary-index task |

For this parse-only task, MyLite accepts the syntax-shape-compatible primary-key
and `AUTO_INCREMENT` declarations and defers type-sensitive and table-sensitive
DDL diagnostics.

### Table-level primary keys

MySQL accepts:

- `PRIMARY KEY (a, b)`
- `CONSTRAINT PRIMARY KEY (a)`
- `CONSTRAINT pk_name PRIMARY KEY (a)`
- `PRIMARY KEY pk_name (a)`
- key parts with prefix lengths on prefix-indexable columns, such as
  `PRIMARY KEY (a(10))`
- key parts with `ASC` or `DESC`, such as `PRIMARY KEY (a DESC, b ASC)`

Primary-key constraint and index names normalize away in MySQL's displayed table
definition. `INFORMATION_SCHEMA.STATISTICS` records primary-key rows with
`INDEX_NAME='PRIMARY'`, `NON_UNIQUE=0`, `INDEX_TYPE='BTREE'`, and
`IS_VISIBLE='YES'`. Descending key parts use `COLLATION='D'`; ascending or
unspecified key parts use `COLLATION='A'`.

MySQL accepts an integer prefix length in syntax even when a later DDL check
rejects it for the column type or value. For example, `PRIMARY KEY (a(1))` on an
integer column and `PRIMARY KEY (a(0))` on a string column parse but fail DDL
validation. Oversized prefix integer tokens are syntax errors.

MySQL parses functional key parts in a primary key and then rejects them because
primary keys cannot be functional indexes. MyLite does not implement functional
key parts in this task because expression key-part support belongs with later
expression and index work.

### Primary-key index options

MySQL accepts these primary-key option shapes:

- `PRIMARY KEY USING BTREE (a)`
- `PRIMARY KEY USING HASH (a)`
- `PRIMARY KEY (a) USING BTREE`
- `PRIMARY KEY USING BTREE (a) COMMENT 'pk' KEY_BLOCK_SIZE 8 VISIBLE`
- `PRIMARY KEY (a) COMMENT 'pk' USING BTREE`
- `PRIMARY KEY (a) KEY_BLOCK_SIZE 8`
- `PRIMARY KEY (a) KEY_BLOCK_SIZE = 8`
- `PRIMARY KEY (a) COMMENT 'pk'`
- `PRIMARY KEY (a) VISIBLE`
- `PRIMARY KEY (a) INVISIBLE`
- `PRIMARY KEY (a) ENGINE_ATTRIBUTE='{}'`
- `PRIMARY KEY (a) ENGINE_ATTRIBUTE '{}'`
- `PRIMARY KEY (a) SECONDARY_ENGINE_ATTRIBUTE=''`
- repeated engine-attribute options

Some accepted syntax produces DDL diagnostics in the verified InnoDB runtime:
`USING HASH` warns and uses the engine default, `INVISIBLE` errors because
primary keys cannot be invisible, unsupported engine attributes error for the
engine, and invalid JSON in `ENGINE_ATTRIBUTE` errors during validation. MyLite
records syntax only and defers these diagnostics.

MySQL rejects these shapes as syntax:

- index options before the key list other than `USING BTREE` or `USING HASH`,
  such as `PRIMARY KEY COMMENT 'pk' (a)`
- `PRIMARY KEY ()`
- `PRIMARY KEY (a,)`
- prefix-length overflow such as `a(18446744073709551616)`
- `KEY_BLOCK_SIZE '8'`
- `KEY_BLOCK_SIZE -1`
- `COMMENT = 'pk'`
- `COMMENT 8`

`USING RTREE` parses in MySQL but is spatial-index syntax and later errors for a
non-spatial column. MyLite leaves `RTREE` primary-key syntax to later spatial
index work and rejects it in this parse-only grammar.

### Keyword treatment

MySQL 8.4.9 treats `btree`, `hash`, `key_block_size`, `auto_increment`,
`engine_attribute`, and `secondary_engine_attribute` as nonreserved words; they
are valid unquoted column names.

`primary`, `key`, `constraint`, `asc`, `desc`, and `index` are reserved and are
not valid unquoted column names. MyLite should not add them to Lemon fallback
identifier handling.

## MyLite behavior

### Parser and AST

MyLite extends the current parse-only `CREATE TABLE` body from a list of column
definitions to a list of table elements. The existing list node continues to
carry the table body so earlier tests and downstream parse-only runtime handling
remain stable.

Column definitions gain two accepted attributes:

- `AUTO_INCREMENT`
- `PRIMARY KEY`, with `KEY` alone producing the same primary-key attribute

Table-level primary keys are represented as primary-key constraint nodes with
children for present optional names, optional index type before the key list, the
key-part list, and the option list. Key parts are identifier-only in this task
and may record a prefix length and order. Index options are retained in source
order.

MyLite performs parser-time validation for syntax-shape errors covered by this
task:

- primary-key key-part lists must be nonempty and comma-separated without a
  trailing comma
- key-part prefix and `KEY_BLOCK_SIZE` values must be integer tokens that fit in
  `uint64_t`
- `KEY_BLOCK_SIZE` does not accept strings or signed numbers
- primary-key comments require string literals and no `=`
- index type accepts only `USING BTREE` or `USING HASH`
- engine-attribute values require string literals, with or without `=`

Semantic DDL checks are deferred. Examples include primary-key uniqueness,
duplicate primary keys, implicit `NOT NULL`, invalid nullable primary-key parts,
auto-increment type restrictions, auto-increment key requirements, explicit
default conflicts, auto-increment allocation, prefix suitability by type, prefix
length zero, primary-key invisibility, engine-option validation, warnings, and
normalization.

### Runtime boundary

Preparing a valid parse-only `CREATE TABLE` statement covered by this task
returns `MYLITE_UNSUPPORTED`, not `MYLITE_PARSE_ERROR`. No table, column, or
index catalog side effect occurs. Malformed or intentionally unsupported syntax
remains `MYLITE_PARSE_ERROR`.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
create_table_statement ::= CREATE TABLE table_name LPAREN table_element_list RPAREN.

table_element_list ::= table_element.
table_element_list ::= table_element_list COMMA table_element.

table_element ::= column_definition.
table_element ::= table_primary_key_constraint.

column_definition ::= identifier column_type column_attribute_list.

column_attribute ::= AUTO_INCREMENT.
column_attribute ::= PRIMARY KEY.
column_attribute ::= KEY.

table_primary_key_constraint ::= PRIMARY KEY opt_primary_key_name opt_index_type
                                 LPAREN key_part_list RPAREN index_option_list.
table_primary_key_constraint ::= CONSTRAINT opt_constraint_name PRIMARY KEY
                                 opt_primary_key_name opt_index_type
                                 LPAREN key_part_list RPAREN index_option_list.

opt_constraint_name ::= .
opt_constraint_name ::= identifier.

opt_primary_key_name ::= .
opt_primary_key_name ::= identifier.

key_part_list ::= key_part.
key_part_list ::= key_part_list COMMA key_part.

key_part ::= identifier opt_key_part_prefix opt_key_part_order.

opt_key_part_prefix ::= .
opt_key_part_prefix ::= LPAREN INTEGER RPAREN.

opt_key_part_order ::= .
opt_key_part_order ::= ASC.
opt_key_part_order ::= DESC.

opt_index_type ::= .
opt_index_type ::= USING index_algorithm.

index_algorithm ::= BTREE.
index_algorithm ::= HASH.

index_option_list ::= .
index_option_list ::= index_option_list index_option.

index_option ::= USING index_algorithm.
index_option ::= KEY_BLOCK_SIZE INTEGER.
index_option ::= KEY_BLOCK_SIZE EQ INTEGER.
index_option ::= COMMENT STRING.
index_option ::= VISIBLE.
index_option ::= INVISIBLE.
index_option ::= ENGINE_ATTRIBUTE STRING.
index_option ::= ENGINE_ATTRIBUTE EQ STRING.
index_option ::= SECONDARY_ENGINE_ATTRIBUTE STRING.
index_option ::= SECONDARY_ENGINE_ATTRIBUTE EQ STRING.
```

The grammar intentionally does not include `UNIQUE`, secondary `KEY` or
`INDEX`, functional key parts, `WITH PARSER`, `FULLTEXT`, `SPATIAL`, `RTREE`,
foreign keys, checks, generated columns, table options, or executable table DDL.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL fragment | Expected MyLite parse behavior |
| --- | --- |
| `a INT PRIMARY KEY` | parse OK |
| `a INT KEY` | parse OK as inline primary-key shorthand |
| `a BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY` | parse OK |
| `a BIGINT AUTO_INCREMENT` | parse OK; missing-key diagnostic deferred |
| `a VARCHAR(10) AUTO_INCREMENT PRIMARY KEY` | parse OK; type diagnostic deferred |
| `a DECIMAL AUTO_INCREMENT PRIMARY KEY` | parse OK; type diagnostic deferred |
| `a FLOAT AUTO_INCREMENT PRIMARY KEY` | parse OK; type diagnostic deferred |
| `a BIGINT AUTO_INCREMENT PRIMARY KEY DEFAULT 1` | parse OK; default conflict deferred |
| `a INT NULL PRIMARY KEY` | parse OK; nullable primary-key diagnostic deferred |
| duplicate auto-increment columns or primary keys | parse OK; DDL diagnostics deferred |
| `PRIMARY KEY (a, b)` | parse OK |
| `CONSTRAINT PRIMARY KEY (a)` | parse OK |
| `CONSTRAINT pk_name PRIMARY KEY (a)` | parse OK |
| `PRIMARY KEY pk_name (a)` | parse OK |
| `PRIMARY KEY (a(10))` | parse OK |
| `PRIMARY KEY (a(0))` | parse OK; prefix semantic diagnostic deferred |
| `PRIMARY KEY (a DESC, b ASC)` | parse OK |
| `PRIMARY KEY USING BTREE (a)` | parse OK |
| `PRIMARY KEY USING HASH (a)` | parse OK |
| `PRIMARY KEY (a) USING BTREE` | parse OK |
| `PRIMARY KEY USING BTREE (a) COMMENT 'pk' KEY_BLOCK_SIZE 8 VISIBLE` | parse OK |
| `PRIMARY KEY (a) COMMENT 'pk' USING BTREE` | parse OK |
| `PRIMARY KEY (a) KEY_BLOCK_SIZE 8` | parse OK |
| `PRIMARY KEY (a) KEY_BLOCK_SIZE = 8` | parse OK |
| `PRIMARY KEY (a) COMMENT 'pk'` | parse OK |
| `PRIMARY KEY (a) VISIBLE` | parse OK |
| `PRIMARY KEY (a) INVISIBLE` | parse OK; semantic diagnostic deferred |
| `PRIMARY KEY (a) ENGINE_ATTRIBUTE='{}'` | parse OK; validation deferred |
| `PRIMARY KEY (a) ENGINE_ATTRIBUTE '{}'` | parse OK; validation deferred |
| `PRIMARY KEY (a) SECONDARY_ENGINE_ATTRIBUTE=''` | parse OK; validation deferred |
| nonreserved option words used as column names | parse OK |
| `a INT UNIQUE KEY` | parse error until secondary indexes land |
| table-level `KEY (a)` or `INDEX (a)` | parse error until secondary indexes land |
| `PRIMARY KEY ()` | parse error |
| `PRIMARY KEY (a,)` | parse error |
| `PRIMARY KEY ((a + 1))` | parse error until functional key parts land |
| overflow prefix length | parse error |
| `KEY_BLOCK_SIZE '8'` | parse error |
| `KEY_BLOCK_SIZE -1` | parse error |
| `PRIMARY KEY COMMENT 'pk' (a)` | parse error |
| `COMMENT = 'pk'` | parse error |
| `COMMENT 8` | parse error |
| `USING RTREE` | parse error until spatial index syntax lands |

Runtime tests should verify that valid covered `CREATE TABLE` statements
prepare as `MYLITE_UNSUPPORTED` and leave `INFORMATION_SCHEMA.TABLES`,
`INFORMATION_SCHEMA.COLUMNS`, and `INFORMATION_SCHEMA.STATISTICS` without
user-object side effects.

## Compatibility gaps

- Executable `CREATE TABLE`, table storage, catalog writes, implicit commits,
  warning records, `SHOW CREATE TABLE`, and information-schema table, column,
  constraint, and index rows are deferred.
- Primary-key uniqueness, duplicate primary-key diagnostics, implicit `NOT
  NULL`, nullable primary-key diagnostics, duplicate key-part diagnostics, and
  key ordering metadata are deferred.
- `AUTO_INCREMENT` allocation, persistence, explicit value handling, overflow,
  `NO_AUTO_VALUE_ON_ZERO`, `LAST_INSERT_ID()`, `auto_increment_increment`,
  `auto_increment_offset`, table-option counters, reset behavior, and insert id
  reporting are deferred.
- `AUTO_INCREMENT` type validation, key membership validation, explicit default
  diagnostics, duplicate auto-column diagnostics, and storage-engine-specific
  sequence behavior are deferred.
- Prefix-length validation by column type, character set, byte length, zero
  length, and storage engine is deferred.
- Primary-key index-option semantics, warnings, visibility diagnostics,
  `KEY_BLOCK_SIZE` effects, engine-attribute JSON validation, unsupported-engine
  diagnostics, and option normalization are deferred.
- Unique indexes, nonunique secondary indexes, full-text indexes, spatial
  indexes, functional key parts, `WITH PARSER`, foreign keys, checks, generated
  columns, generated invisible primary keys, and table options are deferred to
  later roadmap tasks.
