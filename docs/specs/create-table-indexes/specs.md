# CREATE TABLE unique and secondary indexes

## Scope

This feature extends MyLite's parse-only `CREATE TABLE` grammar for unique and
secondary index declarations. It builds on the primary-key key-part and
index-option parser work from the previous roadmap task.

In scope:

- table-level nonunique indexes using `KEY` or `INDEX`
- optional nonunique index names
- optional pre-list index type `USING BTREE` or `USING HASH`
- identifier key parts with optional prefix lengths and `ASC` or `DESC`
- post-list index options `USING BTREE`, `USING HASH`, `KEY_BLOCK_SIZE [=] n`,
  `COMMENT 'string'`, `VISIBLE`, `INVISIBLE`, `ENGINE_ATTRIBUTE [=] 'string'`,
  and `SECONDARY_ENGINE_ATTRIBUTE [=] 'string'`
- table-level unique indexes and constraints using `UNIQUE`, `UNIQUE KEY`, and
  `UNIQUE INDEX`
- optional unique index names and optional constraint names after `CONSTRAINT`
- inline column attributes `UNIQUE` and `UNIQUE KEY`

The task remains parse-only. Valid `CREATE TABLE` statements covered here
prepare as `MYLITE_UNSUPPORTED`; no SQLite table is created and no MyLite table,
column, index, constraint, or information-schema metadata is written.

Out of scope:

- executable DDL and index metadata
- uniqueness enforcement and duplicate-name diagnostics
- `SHOW CREATE TABLE`, `SHOW INDEX`, and information-schema index population
- standalone `CREATE INDEX`
- `ALTER TABLE` index operations
- `FULLTEXT`, `SPATIAL`, multi-valued indexes, functional-index storage, and
  generated-column interactions
- storage-engine validation, JSON validation for engine attributes, warning
  records, and normalization of index options

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, Invisible indexes:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-indexes.html
- MySQL 8.4 Reference Manual, Column indexes:
  https://dev.mysql.com/doc/refman/8.4/en/column-indexes.html
- MyLite functional index key-part parser acceptance spec:
  `docs/specs/functional-index-key-parts/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, including the Task 10 probe set supplied by Jan.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 behavior summary

### Table-level secondary indexes

Representative accepted forms:

| Declaration fragment | MySQL behavior |
| --- | --- |
| `KEY (a)` | accepted; omitted name normalizes to first key-part column name |
| `INDEX (b)` | accepted; omitted name normalizes to first key-part column name |
| `KEY idx USING BTREE (a)` | accepted |
| `KEY USING BTREE (a)` | accepted with no explicit index name |
| `KEY idx (a) USING HASH USING BTREE` | accepted; later `USING` option wins in displayed output |
| `KEY idx_b (b(5) DESC, a ASC) COMMENT 'hello' VISIBLE KEY_BLOCK_SIZE = 8` | accepted; `DESC` and comment are preserved, while default `ASC`, `VISIBLE`, and InnoDB-ignored key-block size are not displayed |

`USING HASH` is accepted for simple InnoDB secondary index declarations but is
normalized away. `USING HASH` combined with explicit descending key parts can
produce a DDL validation error after parsing. MyLite accepts the syntax and
defers storage-engine validation.

MySQL rejects these secondary-index shapes as syntax:

- `KEY ()`
- `KEY (a,)`
- `KEY idx`
- `COMMENT = 'x'`

### Table-level unique indexes and constraints

Representative accepted forms:

| Declaration fragment | MySQL behavior |
| --- | --- |
| `UNIQUE (a)` | accepted; omitted index name normalizes to first key-part column name |
| `UNIQUE KEY uk_b (b)` | accepted |
| `UNIQUE INDEX ux_c USING BTREE (c)` | accepted |
| `CONSTRAINT uq_d UNIQUE KEY unique_d (d)` | accepted |
| `CONSTRAINT UNIQUE (a)` | accepted with no explicit constraint name |
| `CONSTRAINT UNIQUE uq_a (a)` | accepted; `uq_a` is the index name |
| `UNIQUE KEY USING BTREE (a)` | accepted with no explicit index name |
| `UNIQUE KEY uq (a) USING HASH USING BTREE` | accepted; later `USING` option wins in displayed output |
| `UNIQUE KEY uq USING BTREE (b(5) DESC, a ASC) COMMENT 'hello' VISIBLE KEY_BLOCK_SIZE 8` | accepted |

Unique indexes enforce distinct non-`NULL` key values when DDL execution and
writes exist. MySQL permits multiple `NULL` values in nullable unique-key parts.
If a prefix length is specified, uniqueness applies to the indexed prefix.
MyLite records only syntax in this task and defers all uniqueness semantics.

MySQL rejects these unique-index shapes as syntax:

- `UNIQUE KEY idx`
- `COMMENT = 'x'`

### Inline unique attributes

`a INT UNIQUE` and `a INT UNIQUE KEY` are accepted by MySQL and normalize to
table-level unique indexes named from the column. `a INT UNIQUE INDEX` is a
syntax error. MyLite follows that parse boundary.

Existing inline `KEY` remains MySQL's inline primary-key shorthand and is not
changed by this feature.

### Engine attributes and storage-engine validation

`ENGINE_ATTRIBUTE` and `SECONDARY_ENGINE_ATTRIBUTE` accept string literals with
or without `=` in index definitions. MySQL parses those options before
storage-engine validation; InnoDB currently rejects unsupported engine
attributes during DDL execution. Invalid JSON similarly parses and fails later.
MyLite accepts the string-valued option syntax and defers validation.

### Excluded syntax

MySQL parses `FULLTEXT KEY`, `SPATIAL KEY`, and functional key parts such as
`KEY idx ((a + 1))`. Functional key parts are now accepted by MyLite's shared
key-part parser and rejected during DDL validation with no mutation until
functional-index storage is implemented. `FULLTEXT KEY`, `SPATIAL KEY`, and
multi-valued key parts remain intentionally excluded from this task and stay
parse errors until their dedicated compatibility work is specified and
implemented.

### Keyword treatment

MySQL 8.4.9 treats unquoted `unique`, `index`, and `key` as invalid column
names. MyLite must not add fallback identifier handling for these reserved
tokens.

The existing fallback treatment for `btree`, `hash`, `key_block_size`,
`engine_attribute`, and `secondary_engine_attribute` remains correct; these
tokens can continue to appear as unquoted identifiers where the grammar expects
an identifier.

## MyLite behavior

### Parser and AST

MyLite extends table elements to include:

- primary-key constraints from the previous task
- unique index definitions
- secondary index definitions

Unique and secondary index declarations reuse the existing key-part list,
index-type, and index-option AST nodes. New top-level AST node kinds distinguish
unique index declarations from nonunique index declarations.

Child order for a secondary-index node is:

1. optional index name
2. optional pre-list index type
3. key-part list
4. index-option list

Child order for a unique-index node is:

1. optional constraint name
2. optional index name
3. optional pre-list index type
4. key-part list
5. index-option list

Absent optional children are omitted, matching the existing AST convention.

Inline `UNIQUE` and `UNIQUE KEY` column attributes produce column-attribute
nodes distinct from the existing primary-key attribute. The parser does not
expand inline unique attributes into table-level unique-index nodes because
normalization belongs to executable DDL.

### Syntax validation

MyLite accepts:

- `KEY (a)` and `INDEX (a)`
- `KEY name (a)` and `INDEX name (a)`
- `KEY USING BTREE (a)` and `KEY name USING HASH (a)`
- `KEY name (a) USING HASH USING BTREE`
- key parts with `identifier[(integer)] [ASC|DESC]`
- functional key parts with `((expression)) [ASC|DESC]`
- all Task 9 index options in post-list position
- `UNIQUE (a)`
- `UNIQUE name (a)`
- `UNIQUE KEY (a)`, `UNIQUE KEY name (a)`, and `UNIQUE KEY USING BTREE (a)`
- `UNIQUE INDEX (a)`, `UNIQUE INDEX name USING BTREE (a)`
- `CONSTRAINT UNIQUE (a)`
- `CONSTRAINT UNIQUE name (a)`
- `CONSTRAINT symbol UNIQUE KEY name (a)`
- inline `UNIQUE` and `UNIQUE KEY`

MyLite rejects as syntax errors:

- empty key-part lists
- trailing commas in key-part lists
- missing key-part lists such as `KEY idx` and `UNIQUE KEY idx`
- multi-valued key parts
- `FULLTEXT` and `SPATIAL` index declarations
- `COMMENT = 'x'`
- string, signed, or overflowing values where an integer prefix length or
  `KEY_BLOCK_SIZE` value is required
- `a INT UNIQUE INDEX`

Semantic DDL diagnostics are deferred, including duplicate names, duplicate
indexes, unsupported functional key parts, invalid index types for storage
engines, invalid `USING HASH` plus descending-order combinations, prefix
suitability by column type, unsupported engine attributes, invalid
engine-attribute JSON, key-block-size effects, visibility metadata, and
warnings.

### Runtime boundary

Preparing a valid parse-only `CREATE TABLE` statement covered by this task
returns `MYLITE_UNSUPPORTED` with a `NULL` statement handle. It must not mutate
`INFORMATION_SCHEMA.TABLES`, `INFORMATION_SCHEMA.COLUMNS`, or
`INFORMATION_SCHEMA.STATISTICS`.

Malformed or intentionally excluded syntax returns `MYLITE_PARSE_ERROR`, also
with a `NULL` statement handle.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
table_element ::= column_definition.
table_element ::= table_primary_key_constraint.
table_element ::= table_secondary_index.
table_element ::= table_unique_index.

column_attribute ::= UNIQUE.
column_attribute ::= UNIQUE KEY.

table_secondary_index ::= secondary_index_keyword opt_index_name opt_index_type
                          LPAREN key_part_list RPAREN index_option_list.

secondary_index_keyword ::= KEY.
secondary_index_keyword ::= INDEX.

table_unique_index ::= UNIQUE opt_unique_index_keyword opt_index_name opt_index_type
                       LPAREN key_part_list RPAREN index_option_list.
table_unique_index ::= CONSTRAINT opt_constraint_name UNIQUE opt_unique_index_keyword
                       opt_index_name opt_index_type
                       LPAREN key_part_list RPAREN index_option_list.

opt_unique_index_keyword ::= .
opt_unique_index_keyword ::= KEY.
opt_unique_index_keyword ::= INDEX.

opt_constraint_name ::= .
opt_constraint_name ::= identifier.

opt_index_name ::= .
opt_index_name ::= identifier.

key_part_list ::= key_part.
key_part_list ::= key_part_list COMMA key_part.

key_part ::= identifier opt_key_part_prefix.
key_part ::= identifier opt_key_part_prefix ASC.
key_part ::= identifier opt_key_part_prefix DESC.
key_part ::= LPAREN expression RPAREN.
key_part ::= LPAREN expression RPAREN ASC.
key_part ::= LPAREN expression RPAREN DESC.

opt_key_part_prefix ::= .
opt_key_part_prefix ::= LPAREN INTEGER RPAREN.

opt_index_type ::= .
opt_index_type ::= index_type.

index_type ::= USING BTREE.
index_type ::= USING HASH.

index_option_list ::= .
index_option_list ::= index_option_list index_option.

index_option ::= index_type.
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

## Test plan

Parser tests should inspect AST shape for:

- unnamed and named secondary indexes
- `KEY` and `INDEX` synonyms
- `USING BTREE` and `USING HASH` before the key list
- repeated post-list `USING` options
- prefix lengths, functional key parts, `ASC`, and `DESC`
- `COMMENT`, `KEY_BLOCK_SIZE`, visibility, and engine-attribute options
- unnamed and named unique indexes
- `UNIQUE`, `UNIQUE KEY`, `UNIQUE INDEX`, and `CONSTRAINT ... UNIQUE` forms
- inline `UNIQUE` and `UNIQUE KEY`
- keyword fallback for existing nonreserved option words
- malformed or excluded syntax

Runtime boundary tests should verify:

- valid parse-only secondary and unique index forms return `MYLITE_UNSUPPORTED`
  with no statement handle
- functional key parts on the executable `CREATE TABLE` subset return
  `MYLITE_UNSUPPORTED` during statement execution and create no table or index
  metadata
- no table, column, or statistics rows are created
- malformed or excluded forms return `MYLITE_PARSE_ERROR`
