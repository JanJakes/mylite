# Parser Corpus Column Collation Attribute Order

This slice admits additional MySQL 8.4.9 column `COLLATE` attribute orderings
that remain in the parser corpus after the broader column-attribute-order work.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- https://dev.mysql.com/doc/refman/8.4/en/charset-column.html
- https://dev.mysql.com/doc/refman/8.4/en/enum.html
- https://dev.mysql.com/doc/refman/8.4/en/set.html

## MySQL 8.4.9 Observations

Runtime probes show that MySQL accepts:

```sql
CREATE TABLE after_not_null (
  c VARCHAR(255) NOT NULL COLLATE utf8mb4_unicode_ci,
  INDEX (c)
);
CREATE TABLE enum_after_default (
  a ENUM('Y', 'N') DEFAULT 'N' COLLATE utf8mb4_unicode_ci
);
CREATE TABLE varchar_default_before_collate (
  v VARCHAR(10) DEFAULT '' COLLATE utf8mb4_bin
);
CREATE TABLE inline_key_before_collate (
  v VARCHAR(10) UNIQUE KEY COLLATE utf8mb4_bin
);
CREATE TABLE legacy_collations (
  col1 VARCHAR(100) NOT NULL COLLATE latin1_swedish_ci,
  col2 VARCHAR(200) NOT NULL COLLATE utf8mb4_general_ci
);
```

`SHOW CREATE TABLE` normalizes these to type-level
`CHARACTER SET ... COLLATE ...` metadata before nullability/default rendering.

Runtime probes also show that MySQL rejects these orderings as syntax errors:

```sql
CREATE TABLE bad_comment_charset (
  v VARCHAR(10) COMMENT 'x' CHARACTER SET utf8mb4
);
CREATE TABLE bad_null_charset (
  v VARCHAR(10) NOT NULL CHARACTER SET utf8mb4
);
```

## Scope

In scope:

- allow column `COLLATE` after legacy column attributes such as nullability,
  defaults, inline keys, comments, and auto-increment in the existing
  column-definition parser;
- preserve syntax rejection for `CHARACTER SET` / `CHARSET` after legacy
  attributes;
- preserve duplicate charset/collation/default/nullability/key/auto-increment
  attribute checks;
- keep runtime descriptor behavior unchanged except for order-only string
  column definitions that already use supported charset/collation metadata.

Out of scope:

- executable `ENUM` / `SET` column charset or collation metadata;
- new charset or collation names beyond the current registry;
- accepting double-quoted identifiers outside the relevant SQL mode;
- accepting multi-statement corpus fragments, stored-program bodies, or invalid
  `CHARACTER SET` orderings.

## MyLite Grammar Direction

The Lemon grammar already accepts a flexible `column_attribute_list`. This
slice refines post-parse validation:

```lemon
column_attribute ::= nullability.
column_attribute ::= column_default.
column_attribute ::= column_collation_attribute.
```

Validation remains MySQL-shaped:

- `COLLATE` may follow legacy attributes, and runtime planning resolves it as
  type-level collation metadata.
- `CHARACTER SET` remains before legacy attributes.
- generated-column order guards remain strict.

## Runtime Behavior

For supported `CHAR`, `VARCHAR`, and `TEXT` descriptors, runtime planning
already resolves charset/collation attributes by AST kind rather than source
order, so accepted `COLLATE` after nullability, default, inline key, or comment
produces the same metadata as the existing pre-attribute spelling.

For `ENUM` and `SET`, parser admission is added only to reduce valid MySQL
syntax failures; runtime still reports the existing unsupported diagnostic for
explicit column charset/collation metadata on those type families.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for accepted and rejected orderings;
- parser acceptance for `COLLATE` after nullability and `ENUM` default;
- parser acceptance for `COLLATE` after `VARCHAR` default, inline key, and
  comment attributes;
- parser rejection for `CHARACTER SET` after nullability and `COMMENT` before
  charset;
- runtime metadata and `SHOW CREATE TABLE` behavior for supported `VARCHAR`
  and legacy collation cases;
- runtime unsupported diagnostics for parser-admitted `ENUM DEFAULT ...
  COLLATE`.

## Compatibility Status

This slice extends column-definition parser compatibility and executable
string-column DDL order compatibility. It does not mark executable `ENUM` /
`SET` charset/collation metadata as supported.
