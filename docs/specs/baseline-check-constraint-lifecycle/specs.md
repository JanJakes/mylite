# Baseline CHECK Constraint Lifecycle

## Status

This phase adds the first descriptor-owned `CHECK` constraint slice for
persistent MyLite base tables. It supports table-level and inline column
checks in `CREATE TABLE`, stores check descriptors in the MyLite catalog,
renders them through descriptor-driven metadata, and enforces the supported
expression subset for current row-writing statements.

The slice is intentionally narrow. It does not add `ALTER TABLE ADD CHECK`,
`DROP CHECK`, enforcement toggles, deterministic function analysis, generated
columns, foreign-key action dependency rules, or a general table-backed
expression engine. The goal is the common schema lifecycle and DML enforcement
building block without moving MySQL compatibility ownership into SQLite.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing descriptor-backed table, key, foreign-key, information-schema, and
  DML implementations in `packages/libmylite/src/runtime/`
- Existing parser and AST scaffolding under `packages/libmylite/src/sql/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` CHECK constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-check-constraints.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-check-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_check_constraint_lifecycle_expectations.sh`
records runtime probes for this feature. Observed behavior shaping this slice:

- `CREATE TABLE` accepts table-level and column `CHECK` constraints with
  optional `CONSTRAINT name` and optional `ENFORCED` / `NOT ENFORCED`.
- Omitted names are generated as `<table>_chk_<n>`.
- `CHECK` names are unique per schema across `CHECK` constraints, but they may
  match names of unique keys, foreign keys, or indexes. The same `CHECK` name
  may appear in a different schema.
- Column checks may reference only the containing column. Table checks may
  reference any table column, including columns declared later in the table.
- Checks may not reference `AUTO_INCREMENT` columns.
- An enforced check passes when the expression evaluates to `TRUE` or
  `UNKNOWN`; it fails only on `FALSE`.
- `NOT ENFORCED` descriptors are stored and rendered but do not reject rows.
- `CHECK (TRUE)` is accepted and always passes. `CHECK (FALSE)` is accepted
  and rejects rows when enforced. `CHECK (NULL)` is rejected at DDL time as a
  non-boolean expression.
- Plain `INSERT`, `UPDATE`, and `REPLACE` violations fail with
  `3819 / HY000` and message
  `Check constraint '<name>' is violated.`
- `INSERT IGNORE` skips offending rows, records warning `3819 / HY000` for
  each skipped row, and counts only inserted rows.
- `CREATE TABLE ... LIKE` clones check constraints but regenerates all check
  names from the target table name, including originally explicit names.
- `CREATE TABLE ... SELECT` omits check constraints.
- `RENAME TABLE` renames generated check names to the new table prefix while
  preserving explicit names.
- `DROP TABLE` removes check metadata.
- `SHOW CREATE TABLE` renders check constraints after key and foreign-key
  lines, sorted by logical check name in observed cases. `NOT ENFORCED`
  renders as the MySQL version-comment suffix.
- `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` reports
  `CONSTRAINT_CATALOG = 'def'`, the schema name, the check name, and the
  normalized check clause. `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` reports
  `CONSTRAINT_TYPE = 'CHECK'` and `ENFORCED = 'YES'` or `'NO'`.
- Unsupported expression categories produce MySQL diagnostics in observed
  cases: column check references to other columns use `3813`, non-boolean
  expressions use `3812`, auto-increment references use `3818`, subqueries use
  `3815`, variables use `3816`, and nondeterministic or stored functions use
  `3814`.

## Scope

Supported DDL:

- persistent base tables only;
- table-level `[CONSTRAINT name] CHECK (expr) [ENFORCED | NOT ENFORCED]`
  items inside `CREATE TABLE`;
- inline column `[CONSTRAINT name] CHECK (expr) [ENFORCED | NOT ENFORCED]`
  attributes inside supported column definitions;
- omitted-name generation as `<table>_chk_<n>`, with schema-level check-name
  duplicate detection;
- explicit `ENFORCED`, omitted enforcement, and explicit `NOT ENFORCED`;
- `CREATE TABLE ... LIKE` check cloning with target-generated check names;
- `CREATE TABLE ... SELECT` omission of check constraints;
- `RENAME TABLE` logical renaming for generated check names;
- `DROP TABLE` descriptor cleanup.

Supported expression subset:

- unqualified descriptor column references from the same table;
- inline column checks may reference only the containing column;
- supported integer-family, `DECIMAL`, approximate numeric, canonical temporal,
  `YEAR`, `BIT`, and ASCII text descriptors may participate only where their
  current physical storage and conversion path can be safely translated to a
  SQLite check expression; first implementation tests focus on integer-family
  and `NULL` values;
- decimal integer literals with optional unary sign;
- `TRUE` and `FALSE` constants;
- `NULL` only as the right operand of `IS NULL` / `IS NOT NULL`;
- parentheses;
- unary plus and unary minus on numeric terms;
- arithmetic `+`, `-`, and `*` over admitted numeric terms;
- comparisons `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=`;
- `IS NULL` and `IS NOT NULL`.

Supported metadata:

- descriptor-driven `SHOW CREATE TABLE` check lines;
- user rows in `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`;
- `CHECK` rows in `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` with enforced status;
- preserved system-view metadata already implemented for
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`;
- `CREATE TABLE ... LIKE` and `RENAME TABLE` metadata behavior from MyLite
  descriptors, not SQLite schema reflection.

Supported enforcement:

- enforced checks apply to current `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, `REPLACE ... SET`, and single-table `UPDATE` paths;
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` skip rows that fail
  an enforced check and append MySQL-compatible warnings;
- `NOT ENFORCED` checks do not reject rows;
- successful supported writes preserve existing affected-row and warning-count
  semantics except for skipped `INSERT IGNORE` rows;
- `UPDATE` and `REPLACE` check failures roll back the statement through the
  existing statement transaction path.

Out of scope:

- temporary tables;
- `ALTER TABLE ADD CHECK`, `DROP CHECK`, and `ALTER CHECK ... ENFORCED`;
- deterministic function support in check expressions;
- string literals, decimal/float literals, hex/bit literals, temporal literals,
  parameters, variables, functions, stored functions, subqueries, generated
  columns, expression default reuse, collations, casts, `IN`, `BETWEEN`,
  `LIKE`, `REGEXP`, `AND`, `OR`, `XOR`, and `NOT`;
- `CHECK` dependency rules for foreign-key referential actions beyond rejecting
  unsupported action forms already outside the current FK slice;
- broad `INSERT ... SELECT`, `REPLACE ... SELECT`, or
  `INSERT ... ON DUPLICATE KEY UPDATE` check-specific behavior unless the
  existing row path can enforce it without widening this slice;
- SQLite trigger-based enforcement, native SQLite schema reflection, or SQLite
  fork patches.

## Ownership Boundary

- Public API: unchanged. Applications continue through `mylite_execute()` and
  existing result accessors.
- Statement context: owns diagnostics, warning counts, affected rows,
  `ROW_COUNT()`, transactional cleanup, and failure rollback.
- Lexer/parser/AST: admits the narrow `CHECK` syntax and carries the optional
  constraint name, expression tree, enforcement mode, and inline/table
  placement. It does not resolve descriptors or inspect SQLite schema text.
- Analyzer/planner: resolves schemas, tables, columns, duplicate check names,
  generated names, expression admissibility, expression result kind, and
  physical expression rendering from MyLite descriptors.
- Catalog: owns durable check descriptors, descriptor versions, logical names,
  physical SQLite constraint names, enforcement flags, generated-name flags,
  generated ordinals, and normalized check clauses. SQLite schema text is not
  the metadata authority.
- Result and introspection builders: render `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` from descriptors.
- SQLite physical storage: stores rows and enforces emitted physical check
  clauses for enforced constraints. MyLite maps SQLite check failures back to
  descriptors by physical constraint name and reports MySQL diagnostics.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.

## Syntax

MyLite Lemon-syntax sketch for the admitted grammar:

```lemon
create_table_item ::= column_definition.
create_table_item ::= primary_key_definition.
create_table_item ::= secondary_index_definition.
create_table_item ::= unique_index_definition.
create_table_item ::= foreign_key_definition.
create_table_item ::= check_constraint_definition.

column_attribute ::= NULL.
column_attribute ::= NOT NULL.
column_attribute ::= DEFAULT default_value.
column_attribute ::= AUTO_INCREMENT.
column_attribute ::= PRIMARY KEY.
column_attribute ::= UNIQUE.
column_attribute ::= check_constraint_definition.

check_constraint_definition ::=
    constraint_name_opt CHECK LPAREN check_expression RPAREN check_enforcement_opt.

constraint_name_opt ::= .
constraint_name_opt ::= CONSTRAINT identifier.

check_enforcement_opt ::= .
check_enforcement_opt ::= ENFORCED.
check_enforcement_opt ::= NOT ENFORCED.

check_expression ::= expression.
```

`check_expression` reuses the existing expression AST but the planner accepts
only the subset listed above. Unsupported shapes parse when already admitted by
the expression scaffold and fail deterministically during planning.

## Name Semantics

The catalog stores:

- `name`: logical MySQL check name shown in metadata and diagnostics;
- `physical_name`: stable SQLite constraint name emitted when the physical
  table is created;
- `name_is_generated`: whether MyLite may rename the logical name during table
  rename or `LIKE` cloning;
- `generated_ordinal`: the `<n>` suffix for generated names;
- `ordinal_position`: creation-order position for stable cloning and internal
  iteration;
- `is_enforced`: whether the check is enforced.

Explicit check names are stored as both logical and physical names. Generated
names are stored as the current logical name and the physical name used when
the SQLite table was created. On `RENAME TABLE`, generated logical names change
to `<new_table>_chk_<generated_ordinal>`, while `physical_name` remains stable
because MyLite does not rebuild the physical SQLite table. When SQLite reports
a physical check failure, MyLite resolves the physical name back to the current
logical descriptor name before emitting diagnostics.

Check names are compared using the current descriptor identifier comparison
rules. This slice treats names as case-insensitive for deterministic MyLite
duplicate detection until the catalog has full MySQL accent-sensitive/case-
sensitive identifier comparison. That limitation must be documented in the
compatibility table.

## Expression Semantics

The accepted expression must be boolean at DDL-planning time:

- comparisons and `IS [NOT] NULL` produce boolean results;
- `TRUE` and `FALSE` are boolean constants;
- arithmetic terms are allowed only as operands of a boolean expression;
- a bare numeric term, bare `NULL`, or unsupported expression kind is rejected
  before catalog mutation.

Column references resolve only against MyLite column descriptors in the target
table. Unknown columns fail with a MySQL-compatible check-column diagnostic.
Table-qualified columns, cross-table references, and aliases are unsupported.

For inline column checks, every column reference must resolve to the containing
column. A reference to any other table column fails with the MySQL column-check
diagnostic.

Any reference to an `AUTO_INCREMENT` descriptor fails before physical SQLite
SQL is generated.

Enforced expressions are translated into standard SQLite `CHECK` expressions
using quoted physical column identifiers. MySQL-compatible logical check
clauses are stored separately for `SHOW CREATE TABLE` and information schema.
The stored logical clause is independently normalized by MyLite; it is not read
from SQLite schema SQL.

## Physical SQLite Handling

MyLite-created user tables are ordinary rowid SQLite tables. This feature does
not depend on rowid values or ordering.

For enforced checks, MyLite emits table constraints in the generated physical
`CREATE TABLE` statement:

```sql
CREATE TABLE "_mylite_user_table_<table_id>" (
    "_mylite_column_<column_id>" ...,
    CONSTRAINT "<physical_check_name>" CHECK (<sqlite_expression>)
)
```

`NOT ENFORCED` descriptors are not emitted into SQLite physical DDL. They are
stored only in MyLite descriptors and metadata views.

All generated SQLite identifiers are quoted. Check expressions are assembled
only from descriptor-resolved physical column identifiers and validated literal
tokens. No user SQL fragment is interpolated without validation. This uses
MyLite wrapper/translation plus public SQLite prepared statements and requires
no SQLite fork hook.

SQLite check errors are treated as an enforcement mechanism, not metadata
authority. MyLite maps the failed physical constraint name to a catalog
descriptor and emits MySQL-compatible diagnostics. If SQLite returns a check
failure without a mappable name, MyLite emits a deterministic physical-failure
diagnostic.

## Catalog Design

The catalog schema advances by one version and adds:

```sql
_mylite_catalog_check_constraints(
  check_constraint_id INTEGER PRIMARY KEY,
  table_id INTEGER NOT NULL,
  name TEXT NOT NULL,
  physical_name TEXT NOT NULL,
  check_clause TEXT NOT NULL,
  sqlite_expression TEXT NOT NULL,
  is_enforced INTEGER NOT NULL,
  name_is_generated INTEGER NOT NULL,
  generated_ordinal INTEGER NOT NULL,
  ordinal_position INTEGER NOT NULL,
  descriptor_version INTEGER NOT NULL,
  created_catalog_generation INTEGER NOT NULL,
  updated_catalog_generation INTEGER NOT NULL
)
```

The table references a MyLite table descriptor by `table_id`; schema-level
duplicate-name checks are implemented by catalog queries joining through the
table descriptor. Dropping a table removes its check descriptors in the same
mutation. Renaming a table updates generated logical names but leaves physical
names unchanged.

## Metadata Rendering

`SHOW CREATE TABLE` renders check constraints after current key and foreign-key
lines. Logical check names are sorted by current descriptor name for the
observed MySQL order in this slice. `NOT ENFORCED` appends the MySQL
version-comment suffix:

```sql
CONSTRAINT `name` CHECK (<check_clause>) /*!80016 NOT ENFORCED */
```

`INFORMATION_SCHEMA.CHECK_CONSTRAINTS` emits one row per descriptor:

| Column | Value |
| --- | --- |
| `CONSTRAINT_CATALOG` | `def` |
| `CONSTRAINT_SCHEMA` | owning schema name |
| `CONSTRAINT_NAME` | logical check name |
| `CHECK_CLAUSE` | stored logical check clause |

`INFORMATION_SCHEMA.TABLE_CONSTRAINTS` emits `CHECK` rows with
`ENFORCED = 'YES'` for enforced descriptors and `ENFORCED = 'NO'` for
`NOT ENFORCED` descriptors.

`CREATE TABLE ... LIKE` clones supported check descriptors in metadata order,
assigns every cloned check a generated target name `<target>_chk_<n>`, emits
physical enforced checks for the target table, and preserves enforcement mode
and expression clauses with target-table column descriptors.

`CREATE TABLE ... SELECT` intentionally omits check constraints, matching
observed MySQL behavior and the existing MyLite CTAS policy for indexes and
constraints.

## Diagnostics

Supported diagnostics:

- syntax errors: existing parser diagnostics;
- missing default schema, unknown schema, unknown table, reserved MyLite names,
  unsupported object kinds, and create-table collisions: existing lifecycle
  diagnostics;
- duplicate check name in a schema: `3822 / HY000`;
- column check references another column: `3813 / HY000`;
- non-boolean check expression such as `CHECK (NULL)` or a bare numeric term:
  `3812 / HY000`;
- check references an auto-increment column: `3818 / HY000`;
- check references an unknown column: `3820 / HY000`;
- unsupported subquery in a check expression: `3815 / HY000`;
- unsupported variable in a check expression: `3816 / HY000`;
- unsupported function in a check expression: `3814 / HY000` when the function
  name is available, otherwise a deterministic unsupported-expression
  diagnostic;
- unsupported literal or expression kind: deterministic MyLite unsupported
  diagnostic unless a MySQL-compatible code above applies;
- enforced check violation in plain DML: `3819 / HY000`;
- enforced check violation in `INSERT IGNORE`: warning `3819 / HY000`, skipped
  row, and no inserted-row affected count;
- physical SQLite failures not recognized as check violations: existing
  physical SQL diagnostic;
- allocation failures: existing `MYLITE_NOMEM` path and safe cleanup.

Successful supported DDL and DML emit no warnings unless an admitted unrelated
feature already emits warnings.

## Performance And Storage

MyLite does not evaluate supported checks by materializing candidate rows in C.
For ordinary `INSERT`, `UPDATE`, and `REPLACE`, SQLite evaluates enforced
physical `CHECK` clauses while executing the row write. MyLite only performs
planning-time descriptor resolution and failure-time diagnostic mapping.

`INSERT IGNORE` may handle row-by-row writes because the existing insert path
already does that to preserve warning and skipped-row semantics. This does not
add table-wide materialization.

Metadata queries scan compact MyLite catalog rows and do not inspect
`sqlite_schema`. No `.mylite` preamble bytes change. Existing shifted SQLite
payload and VFS invariants are preserved.

## Tests

Tests must cover:

- table-level and inline column checks with explicit and generated names;
- `ENFORCED`, omitted enforcement, and `NOT ENFORCED`;
- `SHOW CREATE TABLE` rendering, including generated names, explicit names,
  sorted output, and `NOT ENFORCED`;
- `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` and
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` rows;
- full-table successful inserts, updates, and replacements for enforced
  checks;
- `TRUE`, `FALSE`, `UNKNOWN`/`NULL`, comparison, arithmetic, `<=>`,
  `IS NULL`, and `IS NOT NULL` expressions in the supported subset;
- `INSERT IGNORE` skipped rows, warning count, diagnostics, and affected rows;
- `UPDATE` and `REPLACE` check violations;
- `CREATE TABLE ... LIKE` check cloning and target-generated names;
- `CREATE TABLE ... SELECT` omission of checks;
- table rename generated-name updates and explicit-name preservation;
- drop-table metadata cleanup;
- duplicate check names in one schema, same names in different schemas, and
  names matching non-check constraints;
- column checks referencing other columns, table checks referencing later
  columns, auto-increment references, unknown columns, non-boolean constants,
  unsupported literals, functions, variables, and subqueries;
- reopen persistence and independent handles;
- no `.mylite` preamble mutation beyond normal SQLite payload changes;
- zero-initialized cleanup for new planner/catalog objects;
- all existing parser, lifecycle, key, foreign-key, metadata, DML, file-format,
  VFS, and registration tests still pass.
