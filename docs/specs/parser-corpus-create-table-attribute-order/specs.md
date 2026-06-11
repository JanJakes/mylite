# Parser Corpus CREATE TABLE Attribute Order

This slice admits MySQL dump-style column attribute ordering in supported
`CREATE TABLE`, `CREATE TEMPORARY TABLE`, `ALTER TABLE ... ADD COLUMN`,
`ALTER TABLE ... MODIFY COLUMN`, and `ALTER TABLE ... CHANGE COLUMN` column
definitions.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html

## MySQL 8.4.9 Runtime Observations

The expectation script for this slice verifies representative behavior against
MySQL 8.4.9:

- `DEFAULT value NOT NULL` is accepted and renders a `NO` nullability column
  with the declared default;
- inline key attributes may appear before or after defaults:
  `PRIMARY KEY DEFAULT value`, `DEFAULT value PRIMARY KEY`, and
  `UNIQUE DEFAULT value` are accepted;
- `NOT NULL AUTO_INCREMENT PRIMARY KEY` remains accepted.
- `DEFAULT NULL NULL` is accepted as nullable syntax, while
  `DEFAULT NULL NOT NULL` is syntactically accepted and then rejected with
  `1067 / 42000` invalid-default diagnostics.

Observed probe:

```sql
CREATE TABLE default_before_not_null (
  id INT DEFAULT '0' NOT NULL,
  name VARCHAR(80) DEFAULT '' NOT NULL,
  PRIMARY KEY (id)
);
CREATE TABLE primary_before_default (id INT PRIMARY KEY DEFAULT 3, b INT);
CREATE TABLE default_before_primary (id INT DEFAULT 3 PRIMARY KEY, b INT);
CREATE TABLE unique_before_default (id INT UNIQUE DEFAULT 3, b INT);
CREATE TABLE auto_increment_legacy (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(32) DEFAULT '' NOT NULL
);
CREATE TABLE invalid_default_null_order (id INT DEFAULT NULL NOT NULL);
```

MySQL returns the `SHOW COLUMNS` rows recorded in
`mysql_parser_corpus_create_table_attribute_order_expectations.sh`.

## Scope

In scope:

- allowing `DEFAULT`, `NULL` / `NOT NULL`, inline `PRIMARY KEY`, inline
  `KEY`, inline `UNIQUE`, and `AUTO_INCREMENT` attributes in any relative
  order accepted by MySQL for the currently supported column-definition subset;
- preserving duplicate-attribute rejection;
- preserving the existing parser guard that explicit column
  `CHARACTER SET` / `CHARSET` attributes must precede legacy
  nullability/default/key/comment/auto-increment attributes in MyLite's admitted
  subset, while the later collation-order slice relaxes `COLLATE` placement for
  MySQL-accepted forms;
- applying the same parser behavior to `CREATE TABLE`, temporary table DDL, and
  supported `ALTER TABLE` column replacement/addition grammar because they
  share the same column-definition parser;
- parser corpus movement measurement over
  `build/perf-data/mysql-server-tests-queries.csv`.

Out of scope:

- broadening executable default expressions beyond existing supported default
  value semantics;
- broadening admitted column character sets or collations;
- changing generated-column ordering rules;
- changing runtime descriptor planning for unsupported data types, defaults, or
  indexes;
- accepting duplicate nullability/default/key/auto-increment attributes;
- adding execution support for corpus DDL blocked by unrelated table options,
  partitioning, stored-program bodies, unsupported types, or quoted identifier
  modes.

## MyLite Grammar Snippet

The Lemon grammar already accepts repeated column attributes in a flexible list.
The intended MyLite-owned grammar shape is:

```lemon
column_definition ::= identifier column_type column_attribute_list_opt.

column_attribute_list ::= column_attribute.
column_attribute_list ::= column_attribute_list column_attribute.

column_attribute ::= nullability.
column_attribute ::= column_default.
column_attribute ::= inline_key_attribute.
column_attribute ::= AUTO_INCREMENT.
column_attribute ::= column_charset_attribute.
column_attribute ::= column_collation_attribute.
column_attribute ::= column_comment_attribute.
column_attribute ::= generated_column_clause.
```

The implementation change is in post-parse validation: MyLite must validate
duplicates and MyLite's supported charset/collation ordering, but it must not
reject otherwise valid MySQL orderings among nullability, defaults, inline keys,
and auto-increment.

## Runtime Behavior

No SQLite fork hook is needed. Runtime descriptor construction already resolves
column attributes by AST kind, so source order is not semantically significant
for the admitted subset. Existing unsupported runtime diagnostics continue to
apply after syntax is accepted.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for default-before-nullability and inline key
  order variants;
- parser acceptance of the representative order variants and preserved
  rejection of `CHARACTER SET` / `CHARSET` after legacy attributes;
- runtime `CREATE TABLE` / `SHOW COLUMNS` / `SHOW CREATE TABLE` behavior for
  supported variants;
- parser corpus benchmark counts before and after the slice.

Corpus movement over `build/perf-data/mysql-server-tests-queries.csv`:

- before this slice, after the CTE placeholder slice: `69037 OK / 558 errors`;
- after this slice: `69098 OK / 497 errors`
  (`lexer_error=21`, `syntax_error=475`, `stack_overflow=1`).

## Compatibility Status

This slice improves the supported `CREATE TABLE` and column-definition parser
surface. It does not add new executable type, default-expression, or index
semantics.
