# Parser Corpus DDL Key and Type Surfaces

This slice reduces MySQL server-test parser-corpus failures around compact key
syntax, index options, primary-prefix key parts, and deprecated numeric
attributes. The goal is to move repeated MySQL 8.4-shaped DDL forms away from
raw syntax errors while preserving MyLite's current runtime correctness
boundary.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- https://dev.mysql.com/doc/refman/8.4/en/numeric-type-attributes.html

## MySQL 8.4.9 Observations

The focused MySQL probe used a `mysql:8.4.9` runtime:

```sql
CREATE TABLE inline_key (id INT KEY, v INT);
SHOW CREATE TABLE inline_key;

CREATE TABLE prefix_pk (a VARCHAR(100), b INT, PRIMARY KEY (a(10), b));
SHOW CREATE TABLE prefix_pk;

CREATE TABLE idx_kbs (a INT, KEY a_idx (a) KEY_BLOCK_SIZE=1024) ENGINE=InnoDB;
CREATE INDEX idx_kbs_create ON idx_kbs (a) KEY_BLOCK_SIZE=512;
SHOW CREATE TABLE idx_kbs;

CREATE TABLE zint (a INT(4) ZEROFILL, b DECIMAL(5,2) ZEROFILL, c FLOAT ZEROFILL);
SHOW CREATE TABLE zint;
SHOW WARNINGS;
```

Observed behavior:

- Inline column `KEY` is accepted as a primary-key column attribute.
- Primary-key prefix parts are accepted and render as `PRIMARY KEY (col(length))`.
- InnoDB accepts index-level `KEY_BLOCK_SIZE` and does not render it back in
  `SHOW CREATE TABLE` for the probed ordinary indexes.
- `ZEROFILL` is accepted for integer, decimal, and approximate numeric columns,
  makes the column unsigned, preserves the attribute in `SHOW CREATE TABLE`,
  and emits deprecation warnings.

## Scope

### Inline column KEY

MySQL permits `KEY` as a compact column attribute equivalent to `PRIMARY KEY`.
MyLite should parse `column type KEY` in the same column-attribute slot that
already accepts `PRIMARY KEY`, then route it through the existing inline primary
key planner. Supported inline primary-key descriptor limits remain unchanged:
integer-family and ASCII `CHAR` / `VARCHAR` columns only, with existing
single-primary-key diagnostics and metadata rendering.

### Index-level KEY_BLOCK_SIZE

MySQL index options include `KEY_BLOCK_SIZE [=] value` for `CREATE TABLE`,
`ALTER TABLE ... ADD INDEX`, and standalone `CREATE INDEX` forms. MyLite
already stores table-level `KEY_BLOCK_SIZE` metadata, but does not have
per-index compression/page-size storage semantics. This slice admits
index-level `KEY_BLOCK_SIZE` and ignores it as an index option placeholder. It
must not alter generated SQLite index DDL or MyLite index descriptors.

### Primary-prefix key parts

MySQL accepts `PRIMARY KEY (column(length))` for supported storage engines and
uses the prefix length in primary-key metadata and uniqueness. MyLite currently
does not implement primary-key prefix descriptors or enforcement. This slice
admits the syntax in the same key-part grammar shape used by secondary prefix
indexes, then rejects it before descriptor mutation with a deterministic
unsupported diagnostic. Secondary and unique prefix indexes keep their existing
runtime behavior.

### ZEROFILL

MySQL accepts deprecated `ZEROFILL` on numeric columns, implies `UNSIGNED`, and
affects column metadata plus result formatting. MyLite does not yet implement
zero-padding readback, `SHOW CREATE TABLE` `zerofill` rendering, or the full
warning and unsigned-interaction surface. This slice treats `ZEROFILL` in
`CREATE TABLE` and `ALTER TABLE` column-definition statements as a parser-level
unsupported utility placeholder when the ordinary grammar cannot parse the
statement. This keeps the corpus from failing with an accidental syntax gap
without creating subtly wrong descriptors.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
column_attribute ::= KEY.
```

```lemon
primary_key_part ::= identifier LPAREN INTEGER RPAREN index_key_direction_opt.
```

```lemon
index_option ::= KEY_BLOCK_SIZE equal_opt INTEGER.
```

`ZEROFILL` remains outside the ordinary type grammar for this slice. The parser
fallback classifies MySQL-shaped `CREATE TABLE` and `ALTER TABLE` statements
containing `ZEROFILL` as unsupported utility statements when they otherwise
would produce a syntax error.

## Runtime Behavior

This slice uses MyLite parser/AST and runtime planning only. It does not need a
SQLite fork hook.

- Inline `KEY` executes through the existing inline primary-key path.
- Index-level `KEY_BLOCK_SIZE` is accepted and ignored when applying index
  options.
- Primary-prefix key parts are rejected before catalog mutation for both
  `CREATE TABLE` and `ALTER TABLE ... ADD PRIMARY KEY`.
- `ZEROFILL` placeholder statements return the standard unsupported utility
  diagnostic and do not mutate catalog state.

## Tests

Focused tests cover:

- parser acceptance and runtime metadata for inline `KEY`;
- parser acceptance and runtime ignore behavior for index-level
  `KEY_BLOCK_SIZE`;
- parser acceptance but runtime rejection for primary-prefix key parts with no
  durable descriptor mutation;
- runtime unsupported diagnostics for `ZEROFILL` create/alter column forms;
- MySQL 8.4.9 expectation script output for representative accepted MySQL
  forms.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice implements compact inline `KEY` and admits index-level
`KEY_BLOCK_SIZE` as an ignored metadata placeholder. It does not implement
primary-prefix key enforcement or numeric `ZEROFILL` semantics. Those remain
documented incompatibilities until MyLite has descriptor storage, duplicate-key
checking, readback formatting, and metadata rendering for those behaviors.
