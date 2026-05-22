# Baseline Generated Column Lifecycle

## Summary

This phase adds a narrow generated-column lifecycle for persistent base tables.
It covers descriptor-backed `CREATE TABLE` generated columns, physical SQLite
generated-column storage/evaluation, ordinary `INSERT`/`UPDATE` interaction,
`SELECT` readback, close/reopen persistence, `SHOW COLUMNS`,
`SHOW FULL COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
metadata.

It is intentionally not full MySQL generated-column support. The expression
subset is limited to deterministic integer expressions over same-row descriptor
columns and integer/boolean/`NULL` literals. The physical implementation uses
SQLite generated columns through public SQLite SQL, with MyLite descriptors as
the metadata authority.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, `CREATE TABLE` and generated columns:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table-generated-columns.html>
- Official MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html>
- Existing MyLite design and lifecycle specs:
  - `docs/specs/baseline-implementation-strategy/specs.md`
  - `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
  - `docs/specs/file-backed-mylite-opening-vfs/specs.md`
  - `docs/specs/mylite-file-format/specs.md`
  - `docs/specs/baseline-catalog-foundation/specs.md`
  - `docs/specs/baseline-basic-table-lifecycle/specs.md`
  - `docs/specs/baseline-row-values-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-integer-expression-defaults/specs.md`
  - `docs/specs/baseline-check-constraint-lifecycle/specs.md`
- Observed MySQL 8.4.9 behavior captured by
  `packages/libmylite/tests/mysql_baseline_generated_column_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite behavior, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the user-visible expectations used by this phase:

- `column type AS (expr)` is accepted and defaults to a virtual generated
  column. `GENERATED ALWAYS AS (expr) VIRTUAL` and
  `GENERATED ALWAYS AS (expr) STORED` are accepted.
- `SHOW COLUMNS` and `SHOW FULL COLUMNS` report `VIRTUAL GENERATED` or
  `STORED GENERATED` in `Extra`.
- `INFORMATION_SCHEMA.COLUMNS.EXTRA` uses the same `VIRTUAL GENERATED` /
  `STORED GENERATED` text. `GENERATION_EXPRESSION` contains the expression
  without the extra outer parentheses used by `SHOW CREATE TABLE`.
- `SHOW CREATE TABLE` renders generated columns as
  `GENERATED ALWAYS AS ((...)) VIRTUAL` or `... STORED`.
- Generated-column expressions are recomputed after base-column `INSERT` and
  `UPDATE`; `NULL` operands make the current admitted arithmetic results
  `NULL`.
- `INSERT INTO t(column_list_without_generated) VALUES (...)` omits generated
  columns and computes them. `INSERT INTO t VALUES (..., DEFAULT, ...)` is
  accepted for generated column slots. Explicit non-`DEFAULT` generated-column
  values fail with `3105 / HY000`.
- `UPDATE t SET generated_column = DEFAULT` is accepted and reports zero
  changed rows. `UPDATE t SET generated_column = non_default_value` fails with
  `3105 / HY000`.
- `UPDATE t SET base_column = value, generated_column = DEFAULT` is accepted;
  the generated assignment contributes no physical assignment while ordinary
  assignments still update and recompute generated values.
- `NOT NULL` on a generated column is accepted only after the generated
  expression clause. Runtime writes fail with ordinary `1048 / 23000` if the
  computed value is `NULL`.
- Generated-column result values are checked against the declared integer-family
  target range. Out-of-range generated results fail with `1264 / 22003`.
- `DEFAULT` and `AUTO_INCREMENT` attributes on a generated column fail with
  `1221 / HY000`.
- An unknown base-column reference in a generated expression fails with
  `1054 / 42S22`, with the generated-column diagnostic context.
- Subqueries, non-deterministic functions, string literal expressions, decimal
  expressions, division, modulo, and generated-column references are accepted or
  diagnosed by full MySQL in broader ways, but remain outside this baseline
  unless explicitly admitted here.

## Ownership Boundaries

- Public API: unchanged. Successful generated-column DDL and DML use existing
  `mylite_execute()` and result-handle conventions.
- Statement context: owns diagnostics reset, warning count, affected rows, and
  transaction completion. Supported successful statements produce zero
  warnings unless an existing supported path already defines warnings.
- Lexer/parser/AST: admits generated-column syntax and stores the expression,
  storage kind, and source spans. It does not resolve names or inspect
  descriptors.
- Analyzer/planner: validates the narrow generated-column expression subset,
  resolves referenced columns against the in-flight MyLite descriptor plan,
  rejects unsupported attributes and DML targets, and renders both the MySQL
  display expression and SQLite physical expression.
- Catalog: owns durable generated-column metadata in
  `_mylite_catalog_columns`. Generated-column descriptors include storage kind,
  display expression, and SQLite expression. Catalog rows, descriptor versions,
  and cache generations change only for DDL that creates, clones, alters, drops,
  or renames table descriptors. Ordinary DML does not mutate catalog metadata.
- SQLite physical storage: owns base-row storage and generated-column
  computation through SQLite-generated table columns. Generated SQL uses stable
  physical table names and quoted physical column names. SQLite schema text is
  not metadata authority.
- Storage/VFS/file format: unchanged. Generated-column DML writes only inside
  the shifted SQLite payload and does not touch the `.mylite` preamble.

## Supported SQL

Generated column syntax in persistent `CREATE TABLE`:

```sql
CREATE TABLE table_name (
    column_name column_type generated_clause [generated_column_attribute ...],
    ...
)

generated_clause:
    [GENERATED ALWAYS] AS (generated_expression) [VIRTUAL | STORED]

generated_column_attribute:
    NULL
  | NOT NULL
  | COMMENT string_literal
```

The supported expression subset is:

```sql
generated_expression:
    generated_additive

generated_additive:
    generated_multiplicative
  | generated_additive + generated_multiplicative
  | generated_additive - generated_multiplicative

generated_multiplicative:
    generated_factor
  | generated_multiplicative * generated_factor

generated_factor:
    column_name
  | integer_literal
  | TRUE
  | FALSE
  | NULL
  | + generated_factor
  | - generated_factor
  | ( generated_expression )
```

Generated-column target types are limited to the current integer descriptor
families that use integer physical storage, excluding `BIT`, `YEAR`, spatial,
JSON, enum, set, string, binary string, decimal, approximate, and temporal
target descriptors in this phase. Referenced columns are limited to previously
declared non-generated integer descriptor columns that use physical `INTEGER`
storage and are not `BIT` or `YEAR`.

The generated expression may reference only unqualified column names from the
same table definition. Schema-qualified, table-qualified, alias-qualified,
future-column, unknown-column, and generated-column references are rejected.

## Parser And AST

MyLite Lemon-syntax snippets:

```lemon
column_attribute(A) ::= generated_column_clause(B). { A = B; }

generated_column_clause(A) ::=
    generated_always_opt(G) AS(T) LPAREN expression(E) RPAREN(R)
    generated_storage_opt(S).

generated_always_opt ::= .
generated_always_opt ::= GENERATED ALWAYS.

generated_storage_opt(A) ::= .              // defaults to VIRTUAL
generated_storage_opt(A) ::= VIRTUAL(T).
generated_storage_opt(A) ::= STORED(T).
```

The grammar deliberately reuses the existing expression nonterminal so MyLite
can parse broader MySQL-looking expressions and return deterministic
unsupported-feature diagnostics from the planner. The planner admits only the
expression subset listed above.

## Semantics

- Generated columns are visible descriptor columns unless the future invisible
  column feature explicitly marks them invisible. Existing visible-column
  behavior remains unchanged.
- Default storage kind is virtual. Stored generated columns persist computed
  values in SQLite; virtual generated columns are computed by SQLite when read.
  MyLite does not expose this physical distinction except through descriptor
  metadata and MySQL-compatible `Extra` / `SHOW CREATE TABLE` text.
- Generated columns may be nullable or `NOT NULL`. If the physical expression
  computes `NULL` for a `NOT NULL` generated column, SQLite rejects the write and
  MyLite maps the failure to the same `Column '<name>' cannot be null` diagnostic
  family used by existing `NOT NULL` row-value paths, naming the generated
  column reported by the physical constraint.
- Generated results must fit the declared target integer family. MyLite emits
  internal physical `CHECK` constraints for generated descriptor columns so
  SQLite still computes the expression, while MyLite maps range failures back to
  `Out of range value for column '<name>' at row <n>`.
- Generated columns cannot have explicit defaults or `AUTO_INCREMENT`.
- Inline primary/unique keys and table-level indexes over generated columns are
  deferred for this baseline, even though MySQL supports some forms. The
  planner rejects generated key parts with a deterministic unsupported error.
- `CREATE TABLE ... LIKE` clones generated-column descriptors, expressions,
  storage kind, nullability, comments, and supported key descriptors not
  involving generated columns. CTAS creates ordinary selected columns and does
  not carry generated-column attributes.
- Table rename and drop do not rewrite generated expressions because they are
  same-row column expressions. Descriptor cleanup follows existing table
  lifecycle behavior.

## DML Interaction

- Physical `INSERT` omits generated columns from the SQLite column list and bind
  list. MyLite still validates MySQL-visible shape first:
  - omitted insert column list targets all visible descriptor columns, including
    generated columns;
  - generated target slots must contain `DEFAULT`;
  - explicit insert column lists and `INSERT ... SET` may name generated columns
    only when the value is `DEFAULT`;
  - non-`DEFAULT` generated values fail with `3105 / HY000`.
- Physical `UPDATE` omits generated-column assignments when the value is
  `DEFAULT`; the statement succeeds and does not change rows on that assignment
  alone. In multiple-assignment updates, generated `DEFAULT` targets are omitted
  while ordinary assignments still execute. Non-`DEFAULT` generated assignments
  fail with `3105 / HY000`.
- `ON DUPLICATE KEY UPDATE` generated-column assignment targets are rejected
  unless the value is `DEFAULT`; generated-column key participation is deferred,
  so this baseline has no generated-column duplicate-key source.
- Generated columns may be selected, filtered, and ordered through existing
  descriptor-driven read paths when their descriptor type belongs to a currently
  supported predicate/order family.

## Physical SQLite Handling

Generated physical column definitions use this shape:

```sql
"logical_column_name" INTEGER
    GENERATED ALWAYS AS (<sqlite_expression>) VIRTUAL|STORED
    [NOT NULL]
    [, CONSTRAINT "_mylite_generated_column_range_<ordinal>"
        CHECK ("logical_column_name" IS NULL OR
               (typeof("logical_column_name") = 'integer' AND
                "logical_column_name" BETWEEN <min> AND <max>))]
```

`<sqlite_expression>` is rendered from the admitted descriptor expression. Every
identifier is emitted through MyLite's SQLite identifier quoting helper. Integer,
boolean, and `NULL` literals are rendered from parsed tokens or known constants;
ordinary DML values remain bound parameters. Internal generated range-check
constraint names are not catalog descriptors and are not exposed by
`SHOW CREATE TABLE` or information-schema rows. MyLite does not add SQLite
triggers or materialize generated values in memory.

No SQLite fork patch is required for this phase. If a later phase needs MySQL
expression semantics that SQLite cannot express efficiently or correctly through
public SQL and function APIs, that phase must specify a targeted fork hook
separately.

## Metadata

- `SHOW COLUMNS` and `SHOW FULL COLUMNS` report generated columns with the same
  `Field`, `Type`, `Null`, `Key`, `Default`, `Collation`, `Privileges`, and
  `Comment` conventions used by existing descriptor columns, and `Extra` equal
  to `VIRTUAL GENERATED` or `STORED GENERATED`.
- `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` is SQL `NULL` for generated
  columns. `EXTRA` is `VIRTUAL GENERATED` or `STORED GENERATED`.
  `GENERATION_EXPRESSION` is the descriptor display expression. Nongenerated
  columns continue to report an empty `GENERATION_EXPRESSION`.
- `SHOW CREATE TABLE` renders `GENERATED ALWAYS AS (<display_expression>)`
  followed by `VIRTUAL` or `STORED`, then nullable/comment suffixes according to
  the verified MySQL order.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported generated-column expression shapes;
- generated columns on unsupported target column families;
- unknown, qualified, future, or generated-column expression references;
- generated column default or auto-increment attributes;
- generated column key parts and indexes in this baseline;
- explicit non-`DEFAULT` generated-column values in `INSERT`, `REPLACE`,
  `UPDATE`, and `ON DUPLICATE KEY UPDATE`;
- computed `NULL` for a `NOT NULL` generated column;
- generated result range failures;
- physical SQLite failures, allocation failures, and public API misuse through
  existing conventions.

Where the current codebase already has a MySQL-compatible diagnostic, reuse it.
Otherwise, use a MyLite-specific unsupported error that names the unsupported
generated-column surface without pretending to be a full MySQL error.

## Tests

Tests must cover:

- parser acceptance for `AS (...)`, `GENERATED ALWAYS AS (...) VIRTUAL`, and
  `... STORED`;
- successful virtual and stored integer generated columns over base integer
  columns, literals, `NULL`, booleans, unary signs, parentheses, and `+`, `-`,
  `*`;
- `NULL` propagation and `NOT NULL` generated-column write failure;
- `INSERT` omitted column list, explicit column list, and explicit `DEFAULT`
  generated slots;
- rejection of explicit non-`DEFAULT` generated values in `INSERT` and
  `UPDATE`;
- update recomputation after base-column changes and no-op `SET generated =
  DEFAULT` affected-row behavior, including generated `DEFAULT` no-ops combined
  with ordinary multiple assignments;
- range failures for generated integer-family targets;
- `SELECT`, `WHERE`, `ORDER BY`, and `LIMIT` readback through generated
  descriptor columns where currently admitted;
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` metadata;
- close/reopen persistence for stored and virtual generated columns;
- `CREATE TABLE ... LIKE` clone and CTAS omission behavior;
- table rename/drop cleanup where applicable;
- file-backed preamble preservation and independent file-backed handles;
- catalog migration from the previous schema version and zero-initialized
  cleanup for new planner/catalog structures;
- deterministic rejection for unsupported expression functions, subqueries,
  string/decimal/division/modulo expressions, qualified references, generated
  references, default attributes, auto-increment attributes, and generated key
  parts.
