# Baseline DROP CONSTRAINT Lifecycle

## Summary

This phase adds MySQL's general single-action constraint drop form for the
constraint kinds already represented by MyLite descriptors:

```sql
ALTER TABLE table_name DROP CONSTRAINT constraint_name
```

The statement resolves the requested name across primary-key, unique,
foreign-key, and check-constraint descriptors, then reuses the existing
descriptor-driven drop implementations for the one resolved constraint kind.
It does not introduce new constraint storage, new physical row storage, a
general constraint namespace, or SQLite schema reflection.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline primary-key, unique-index, foreign-key, and CHECK specs under
  `docs/specs/`
- Baseline `ALTER TABLE ... DROP PRIMARY KEY`,
  `ALTER TABLE ... DROP INDEX|KEY`, `ALTER TABLE ... DROP FOREIGN KEY`, and
  `ALTER TABLE ... DROP CHECK` implementations
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_drop_constraint_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t DROP CONSTRAINT \`PRIMARY\`` removes a primary key and reports
  `ROW_COUNT()` equal to the current table row count and
  `@@warning_count == 0`, matching the constraint-specific
  `DROP PRIMARY KEY` path.
- `ALTER TABLE t DROP CONSTRAINT PRIMARY` is a syntax error because unquoted
  `PRIMARY` is not an identifier in this position.
- named unique constraints and named unique indexes are both removable through
  `DROP CONSTRAINT name`. The unique index disappears from `SHOW CREATE TABLE`,
  `SHOW INDEX`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`.
- nonunique indexes are not constraints for this statement. Dropping a
  nonunique index name through `DROP CONSTRAINT` fails with `3940 / HY000`.
- named CHECK constraints are removable through `DROP CONSTRAINT name`. The
  check disappears from `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`.
- named foreign keys are removable through `DROP CONSTRAINT name`. The FK
  descriptor disappears from `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `REFERENTIAL_CONSTRAINTS`, while the child supporting index remains.
- unknown names fail with `3940 / HY000` and message
  `Constraint '<name>' does not exist.`
- a name shared by multiple constraint types fails with `3939 / HY000` and a
  diagnostic instructing callers to use a constraint-specific drop clause.
- `DROP CONSTRAINT IF EXISTS name` and `DROP CONSTRAINT name IF EXISTS` are
  syntax errors.
- dropping a primary key that is the only key on an auto-increment column still
  fails with `1075 / 42000`; dropping a primary key referenced by a supported
  foreign key still fails with `1553 / HY000`.
- `ALGORITHM` / `LOCK` option tails are accepted for the admitted single-action
  statement and validated through the same online-DDL option policy as the
  underlying constraint kind.

## Scope

Supported:

- persistent MyLite base tables only;
- one single-action
  `ALTER TABLE table_name DROP CONSTRAINT constraint_name` statement;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- one unqualified or quoted constraint identifier;
- primary-key constraints resolved only by quoted logical name `PRIMARY`;
- descriptor-owned unique secondary indexes, including unique indexes created
  by `CREATE TABLE`, `ALTER TABLE ... ADD UNIQUE`, named unique constraints,
  and standalone `CREATE UNIQUE INDEX`;
- descriptor-owned foreign keys from the current persistent FK subset;
- descriptor-owned CHECK constraints from the current persistent CHECK subset;
- case-insensitive matching under the current MyLite catalog identifier policy;
- MySQL-compatible unknown-name and ambiguous-name diagnostics for this
  statement;
- successful no-row DDL result shape with `warning_count == 0`; affected rows
  are zero for unique, foreign-key, and CHECK drops, and current table row
  count for primary-key drops;
- preservation of row values, table descriptors, column descriptors, remaining
  constraints and indexes, `.mylite` preamble bytes, and shifted SQLite payload
  invariants.

Deferred:

- temporary tables;
- views and unsupported object kinds;
- multi-action `ALTER TABLE` lists containing `DROP CONSTRAINT`;
- `DROP CONSTRAINT IF EXISTS` or trailing `IF EXISTS`;
- `ALTER CONSTRAINT`;
- nonunique index drops through `DROP CONSTRAINT`;
- fulltext and spatial metadata-only indexes, which are indexes but not
  constraints in the current MyLite model;
- named primary-key constraints beyond MySQL's logical `PRIMARY` name;
- new constraint namespaces, privilege semantics, metadata locks, partitions,
  cascades beyond existing FK action behavior, triggers, and broader online
  DDL scheduling;
- SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result and diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, statement completion, transaction boundaries, and cleanup on
  failure.
- Parser/AST: admits the narrow single-action statement and stores the target
  table node, constraint-name node, and option tail. It does not resolve
  descriptors or inspect SQLite schema.
- Analyzer/planner/runtime: resolves the writable table, classifies the
  constraint name from MyLite descriptors, rejects unknown and ambiguous names,
  enforces auto-increment and FK dependency rules, and dispatches to the
  existing physical/catalog drop path for the resolved constraint kind.
- Catalog: MyLite descriptors remain authoritative for primary keys, unique
  indexes, foreign keys, and CHECK constraints. SQLite schema text is not used
  to discover logical constraints.
- Result/introspection builders: existing descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, and `INFORMATION_SCHEMA` paths render the
  post-drop state.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: generated physical index drops and CHECK-table
  rebuilds use existing public SQLite DDL helpers. This phase adds no fork
  hook.

## Supported Grammar

MyLite admits:

```sql
ALTER TABLE table_name DROP CONSTRAINT constraint_name
ALTER TABLE table_name DROP CONSTRAINT constraint_name, ALGORITHM = value
ALTER TABLE table_name DROP CONSTRAINT constraint_name, LOCK = value
ALTER TABLE table_name DROP CONSTRAINT constraint_name, ALGORITHM = value, LOCK = value
```

The target table may be unqualified or schema-qualified. The constraint name
must parse as one identifier or quoted identifier. Unquoted `PRIMARY` remains a
syntax error because it is a keyword token, while `` `PRIMARY` `` is accepted.

MyLite Lemon-style sketch:

```lemon
statement(A) ::= alter_table_drop_constraint_statement(B). {
    A = B;
}

alter_table_drop_constraint_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP CONSTRAINT identifier(C)
    alter_table_option_tail_opt(O). {
    A = mylite_sql_parser_make_alter_table_drop_constraint_statement(
        state, A1, T, C, O);
}
```

The AST kind remains distinct from `DROP PRIMARY KEY`, `DROP INDEX|KEY`,
`DROP FOREIGN KEY`, and `DROP CHECK` because its diagnostics depend on
cross-kind name resolution.

## Resolution Semantics

Target table resolution follows the existing persistent writable-table policy:

- unqualified table names require a selected/default schema;
- schema-qualified table names use the explicit schema;
- missing default schema fails with `1046 / 3D000`;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  is generated;
- only persistent base-table descriptors are supported.

Constraint classification is descriptor-owned:

- load current column, index, foreign-key, and CHECK descriptors for the table;
- count a primary-key match only when the requested name equals `PRIMARY` and
  a primary-key descriptor exists;
- count a unique-constraint match for a secondary index descriptor whose
  `is_unique` flag is true and whose logical index name matches the requested
  name;
- do not count ordinary nonunique, fulltext, or spatial index descriptors;
- count a foreign-key match when the FK logical name matches;
- count a CHECK match when the CHECK logical name matches;
- if no matches exist, fail with `3940 / HY000`;
- if more than one constraint kind matches, fail with `3939 / HY000`;
- otherwise dispatch to the resolved drop path.

Name matching follows the current case-insensitive descriptor lookup policy.
The catalog still stores each descriptor kind separately; this feature does not
create a shared constraint namespace.

## Descriptor and Physical Semantics

Primary-key dispatch:

- reuses the `ALTER TABLE ... DROP PRIMARY KEY` planner and executor;
- preserves MySQL's `DROP CONSTRAINT` affected-row behavior by returning the
  current table row count, matching the specific `DROP PRIMARY KEY` form;
- keeps existing auto-increment and referenced-parent-key rejection behavior.

Unique-index dispatch:

- reuses the `DROP INDEX` physical/catalog path after proving that the matched
  secondary descriptor is unique;
- removes the index descriptor and index-column descriptors;
- drops the generated SQLite unique index when present;
- preserves row values, columns, primary key, nonmatching indexes, foreign keys,
  and CHECK constraints.

Foreign-key dispatch:

- reuses the `ALTER TABLE ... DROP FOREIGN KEY` catalog path after proving that
  the matched FK name is the only matching constraint;
- removes FK descriptors only;
- preserves the child supporting index and row values.

CHECK dispatch:

- reuses the `ALTER TABLE ... DROP CHECK` planner and executor after proving
  that the matched CHECK name is the only matching constraint;
- removes CHECK descriptors;
- performs the existing descriptor-driven physical table rebuild only when an
  enforced physical CHECK clause must be removed.

All successful persistent mutations update descriptor identity/generation
through the underlying helper. Failed planning or execution must leave both
catalog descriptors and physical SQLite objects unchanged.

## Online DDL Option Semantics

`DROP CONSTRAINT` accepts the same comma-separated `ALGORITHM` / `LOCK` option
tail syntax as the existing supported single-action `ALTER TABLE` statements.
Validation is applied according to the resolved operation category:

- primary-key physical rebuilds reject unsupported instant or inplace
  assertions through existing rebuild-option policy;
- unique and foreign-key metadata/index drops use existing metadata-option
  policy;
- CHECK drops accept currently supported option values after shared value
  validation, matching observed MySQL 8.4.9 behavior for this slice, even when
  MyLite internally rebuilds a physical table to remove an enforced CHECK;
- unsupported option values remain syntax/option diagnostics through the
  shared parser/runtime validation path.

This is an assertion/diagnostic compatibility surface only. MyLite does not
implement MySQL's online DDL scheduler or concurrent metadata-lock behavior.

## Result Semantics

Successful execution returns through existing non-row statement conventions:

- no row result set;
- `affected_rows == 0` for unique, foreign-key, and CHECK drops;
- `affected_rows == current table row count` for primary-key drops;
- `warning_count == 0`;
- statement diagnostics remain clear;
- `ROW_COUNT()` returns the same affected-row value as the successful
  statement.

## Diagnostics

Required diagnostics:

- syntax errors for unquoted `PRIMARY`, `IF EXISTS`, multi-action forms, and
  unsupported grammar: existing parser `1064 / 42000`;
- public API misuse: existing public API misuse behavior;
- allocation failure: `MYLITE_NOMEM` plus existing diagnostics policy;
- missing default schema: existing `1046 / 3D000`;
- unknown schema: existing `1049 / 42000`;
- unknown table: existing `1146 / 42S02`;
- reserved target names: existing reserved-name diagnostic;
- unsupported object kind: deterministic MyLite unsupported diagnostic;
- unknown constraint name: `3940 / HY000`,
  `Constraint '<name>' does not exist.`;
- ambiguous constraint name: `3939 / HY000`,
  `Table has multiple constraints with the name '<name>'. Please use constraint specific 'DROP' clause.`;
- primary-key drop with no key: normally hidden behind `3940`; specific
  dependency failures reuse the existing primary-key drop diagnostics;
- last auto-increment key removal: existing `1075 / 42000`;
- referenced parent-key removal: existing `1553 / HY000`;
- physical SQLite failures: existing physical schema error mapping from the
  underlying helper.

## Performance and SQLite Integration

The resolver reads descriptor metadata only. It does not scan user rows unless
the resolved underlying operation already requires a physical CHECK rebuild.
Unique, primary-key, and foreign-key drops stay on catalog/index DDL paths and
do not materialize table contents in MyLite. No query data is passed through a
MyLite-side row loop for this feature.

SQLite integration uses existing public SQLite statement and schema execution
helpers. No SQLite fork patch is needed.

## Tests

Fast C tests must cover:

- parser acceptance for unique, FK, CHECK, and quoted-primary forms;
- parser rejection for unquoted `PRIMARY`, `IF EXISTS`, and multi-action
  `DROP CONSTRAINT`;
- successful unique drop from named unique constraint and standalone unique
  index descriptors, including metadata removal and later duplicate DML;
- nonunique index name rejected with `3940` and index preservation;
- successful quoted-primary drop with table-row-count affected rows, metadata removal,
  duplicate DML after drop, and auto-increment/referenced-FK diagnostics;
- successful FK drop preserving the child index and removing FK metadata;
- successful CHECK drop with later violating DML accepted;
- unknown-name and ambiguous-name diagnostics where MyLite can represent the
  duplicate cross-kind names;
- schema-qualified target resolution, missing default schema, unknown schema,
  unknown table, and reserved target names;
- runtime option-tail acceptance and invalid option-value precedence before
  target table or constraint-name resolution;
- reopen persistence, `.mylite` preamble preservation, and independent
  file-backed handles;
- result object shape, affected rows for each resolved constraint kind,
  `ROW_COUNT()`, and warning count.

The MySQL expectation script records every user-visible behavior introduced by
this slice against MySQL 8.4.9.
