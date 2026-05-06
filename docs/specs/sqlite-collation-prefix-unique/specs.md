# SQLite Collation Prefix Uniqueness

## Status

This slice strengthens the collation foundation around MySQL string comparison
and unique-key behavior:

- keep using SQLite's public `sqlite3_create_collation_v2()` surface for
  native string comparison, indexes, equality predicates, `ORDER BY`, grouping,
  and duplicate elimination where a SQLite expression carries the right
  collation;
- preserve MySQL column collation explicitly when MyLite lowers prefix unique
  checks through SQLite expressions such as `substr(column,1,n)`;
- cover case-insensitive prefix uniqueness, binary prefix uniqueness,
  `PAD SPACE`, and `NO PAD` uniqueness behavior in direct SQLite and public
  MyLite CRUD-style tests.

No SQLite fork patch is required for this slice. The important integration
point is recognizing that SQLite's public collation API is sufficient only if
MyLite-generated SQL preserves the collation on derived expressions.

Deferred scope:

- full Unicode weight tables for `utf8mb4_0900_ai_ci`,
  `utf8mb4_unicode_520_ci`, and `utf8mb4_unicode_ci`;
- accent-insensitive comparison;
- collation derivation/coercibility for arbitrary expressions;
- optimizer expression indexes for MySQL prefix indexes;
- direct SQLite parser support for MySQL prefix-key syntax.

## Sources

- MySQL 8.4 Reference Manual, Character Sets and Collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset.html
- MySQL 8.4 Reference Manual, The `CHAR` and `VARCHAR` Types:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, Multiple-Column Indexes and prefix index parts:
  https://dev.mysql.com/doc/refman/8.4/en/multiple-column-indexes.html
- SQLite application-defined collating sequences:
  https://www.sqlite.org/c3ref/create_collation.html
- SQLite expression indexes:
  https://www.sqlite.org/expridx.html

This specification is independently authored from official documentation,
observed MySQL 8.4.9 runtime behavior, and the current MyLite codebase.

## MySQL 8.4.9 Behavior Baseline

Runtime probes were executed on 2026-05-06 against the official
`mysql:8.4.9` Docker image in container `mylite-mysql-849`, using MySQL's
default strict SQL mode.

For `mysql-collation-prefix-unique.sql`, MySQL produced the fixture in
`mysql-collation-prefix-unique.expected.tsv`. The observed behavior establishes:

- `UNIQUE KEY (slug(4))` over `utf8mb4_unicode_ci` treats `Post Alpha` and
  `post Beta` as duplicates because the first four characters compare
  case-insensitively.
- The same prefix index over `utf8mb4_bin` accepts both rows because the first
  four bytes are distinct.
- `utf8mb4_unicode_ci` has `PAD SPACE` behavior: `trail` and `trail ` compare
  as duplicates for a unique key.
- `utf8mb4_0900_ai_ci` has `NO PAD` behavior: `trail` and `trail ` are
  distinct stored `VARCHAR` values.

## Runtime Design

SQLite's public collation API is the correct native primitive for this slice:
collations participate in SQLite b-tree comparisons, equality predicates,
sorting, grouping, `DISTINCT`, and unique-index enforcement without additional
VDBE patches. MyLite already registers supported MySQL collation names on each
SQLite connection.

The subtle failure mode is generated SQL. SQLite columns carry their declared
collation, but derived expressions such as `substr(column,1,n)` do not
automatically retain the column's collation. MySQL prefix indexes are naturally
lowered through such expressions for duplicate probes and existing-row
validation. Therefore, MyLite-generated prefix-key expressions must append
`COLLATE <column_collation>` when the source column has a character collation.

This slice applies that rule to:

- `INSERT` unique probes;
- `ON DUPLICATE KEY UPDATE` conflict lookup;
- `UPDATE` unique probes;
- `CREATE UNIQUE INDEX` existing-row duplicate validation;
- `ALTER TABLE ... ADD UNIQUE` existing-row duplicate validation.

The implementation intentionally does not fork SQLite for this behavior. A fork
point may still be needed later for direct MySQL prefix-key parser support and
for attaching prefix-index metadata to SQLite schema objects without MyLite's
current statement layer.

## Tests

Executable coverage includes:

- direct SQLite collation behavior for case-insensitive predicates,
  byte-sensitive `_bin` predicates, `PAD SPACE`, `NO PAD`, and expression
  prefix unique indexes;
- public MyLite `INSERT IGNORE` behavior for case-insensitive prefix
  duplicates, binary prefix distinct rows, `PAD SPACE`, and `NO PAD`;
- public MyLite duplicate validation for `CREATE UNIQUE INDEX` and
  `ALTER TABLE ... ADD UNIQUE` over existing case-insensitive prefix
  duplicates;
- MySQL fixture diff against
  `mysql-collation-prefix-unique.expected.tsv`.

## Compatibility Status

This feature remains `🟡`: the SQLite extension surface is enough for the
current ASCII-oriented supported collation subset and prefix unique checks now
preserve column collation, but full MySQL Unicode collation weights,
coercibility, direct parser integration, and physical prefix expression indexes
remain future work.
