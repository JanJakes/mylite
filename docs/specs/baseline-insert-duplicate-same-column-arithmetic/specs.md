# Baseline INSERT Duplicate Same-Column Arithmetic

## Summary

This phase extends the descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` path with a narrow arithmetic assignment
shape:

```sql
INSERT INTO t (...) VALUES (...)
ON DUPLICATE KEY UPDATE n = n + 1;

INSERT INTO t (...) VALUES (...)
ON DUPLICATE KEY UPDATE n = n - 1;
```

The assignment target and arithmetic source must be the same unqualified
non-key, non-`AUTO_INCREMENT` integer-family descriptor column. The delta is an
unsigned decimal integer literal. The feature is intentionally not a general
expression evaluator for ODKU assignments.

MyLite owns parsing of the admitted syntax, descriptor resolution, integer
range checking, `NULL` propagation, affected-row reporting, warnings, and
diagnostics. SQLite remains the physical row store. Generated SQLite SQL uses
stable physical table names, quoted identifiers, and bound values.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specs:
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-assignments/specs.md`
  - `docs/specs/baseline-insert-duplicate-composite-keys/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-enforced-keys/specs.md`
  - `docs/specs/baseline-insert-duplicate-key-column-assignments/specs.md`
  - `docs/specs/baseline-update-arithmetic-assignment/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
  - out-of-range and overflow behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_on_duplicate_key_update_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for the admitted subset:

- `col = col + unsigned_integer_literal` and
  `col = col - unsigned_integer_literal` are accepted in an ODKU assignment.
- A duplicate update that changes the stored row reports affected rows `2`,
  `@@warning_count = 0`, and no result rows.
- A duplicate update that leaves the stored row unchanged, such as `col = col +
  0`, reports affected rows `0`.
- If the stored value is `NULL`, `NULL + N` and `NULL - N` store `NULL`; when
  the value was already `NULL`, affected rows are `0`.
- Multi-row statements process rows sequentially. Inserted rows count as `1`;
  changed duplicate rows count as `2`; unchanged duplicate rows count as `0`.
- Signed deltas such as `+ +1`, `+ -1`, and `- -1` are accepted by MySQL as
  general expression forms. MyLite deliberately defers signed deltas in this
  phase because the admitted grammar is the smaller unsigned-literal shape.
- MySQL allows arithmetic assignments to key and `AUTO_INCREMENT` columns when
  the resulting row is valid. MyLite deliberately defers those shapes in this
  phase to avoid mixing this expression slice with sequence advancement and
  second-order duplicate-key tuple rewrites.
- MySQL resolves unqualified ODKU arithmetic source names in `INSERT ... SELECT`
  against both the selected source row and the target table; a common
  same-named source/target column is ambiguous unless qualified. MyLite
  deliberately defers `INSERT ... SELECT` arithmetic duplicate assignments in
  this phase because the admitted grammar does not include table-qualified
  arithmetic sources.
- Integer overflow follows the existing MyLite UPDATE arithmetic policy:
  `INT` family range failures report `1264 / 22003`, signed `BIGINT`
  expression overflow reports `1690 / 22003`, and unsigned subtraction
  underflow reports `1690 / 22003` as a `BIGINT UNSIGNED` expression range
  failure.
- A delta literal larger than the signed 64-bit positive range has no effect on
  all-`NULL` duplicate source values, but fails for non-`NULL` duplicate source
  values in this slice with a deterministic MyLite unsupported diagnostic.

## Scope

Supported:

- current supported `INSERT ... VALUES` and one-row `INSERT ... SET` ODKU
  source envelopes;
- persistent and shadowing session temporary base-table targets;
- target tables with no key or with current supported primary and unique key
  descriptors;
- one or more distinct unqualified duplicate assignment targets;
- duplicate assignment values already supported by the existing ODKU path;
- additionally, `column_name = column_name + unsigned_integer_literal` and
  `column_name = column_name - unsigned_integer_literal`;
- `INT`, `INTEGER`, `BIGINT`, and their `UNSIGNED` forms within the current
  signed 64-bit physical storage envelope;
- source `NULL` propagation for nullable integer columns;
- MySQL-style affected rows for inserted, changed duplicate, and unchanged
  duplicate rows;
- warning count `0` for successful same-column arithmetic assignments;
- existing statement atomicity, duplicate-key selection, duplicate-key
  post-assignment validation for any other admitted key assignments in the same
  statement, persistence, independent handles, and file-format safety.

Deferred:

- signed delta tokens such as `+ +1`, `+ -1`, and `- -1`;
- arithmetic assignments to primary-key, unique-key, prefix-key, or
  `AUTO_INCREMENT` columns;
- table-qualified assignment targets or arithmetic source columns;
- cross-column arithmetic, column-to-column assignments, constant arithmetic,
  parentheses around the arithmetic expression, functions, variables,
  parameters, casts, collations, string/decimal/float/hex/bit literals,
  subqueries, default expressions, or general expression evaluation inside the
  ODKU assignment;
- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- ODKU aliases, partitions, joined sources, `TABLE`, CTEs, unsupported
  `INSERT ... SELECT` arithmetic duplicate assignments, triggers, broad
  foreign-key referential actions, privilege semantics, protocol info strings,
  optimizer behavior, and SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` remains the statement entry point
  and no public ABI changes are introduced.
- Statement context: unchanged. It owns diagnostics, warnings, affected rows,
  transaction completion, and the non-row result shape.
- Parser/AST: expands only the ODKU duplicate-update value grammar for the
  narrow same-column arithmetic syntax and reuses existing binary-expression
  AST nodes.
- Analyzer/planner: resolves the target table, insert columns, duplicate
  assignment target, same-column arithmetic source, integer target type,
  key/auto-increment exclusion, and delta literal from MyLite descriptors before
  any SQLite SQL is generated.
- Catalog: remains authoritative for schema names, table identity, physical
  names, columns, defaults, generated flags, keys, visibility, and
  auto-increment metadata. This feature does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful statements return the existing non-row result with
  zero columns, zero result rows, affected rows, and warning count.
- SQLite physical storage: owns row mutation and physical uniqueness. MyLite
  supplies stable physical table names, quoted identifiers, bound values, and
  descriptor-built predicates.
- Storage/VFS/file format: unchanged. Writes occur only in the shifted SQLite
  payload and must not touch the `.mylite` preamble.

## Supported SQL

This phase extends the duplicate-update value grammar:

```sql
duplicate_assignment:
    column_name = duplicate_update_value

duplicate_update_value:
    supported_insert_value
  | DEFAULT
  | VALUES ( column_name )
  | column_name + unsigned_integer_literal
  | column_name - unsigned_integer_literal
```

MyLite Lemon-syntax sketch:

```lemon
duplicate_assignment ::= qualified_identifier EQUAL duplicate_update_value.

duplicate_update_value ::= insert_value.
duplicate_update_value ::= VALUES LPAREN qualified_identifier RPAREN.
duplicate_update_value ::= arithmetic_duplicate_source_column PLUS INTEGER.
duplicate_update_value ::= arithmetic_duplicate_source_column MINUS INTEGER.

arithmetic_duplicate_source_column ::= IDENTIFIER.
arithmetic_duplicate_source_column ::= QUOTED_IDENTIFIER.
```

The parser admits only the unqualified arithmetic source forms above. Runtime
still validates that the source resolves to the same descriptor column as the
assignment target.

## Resolution And Semantics

Schema and target-table resolution follow the current `INSERT` policy:

- unqualified target names use the selected schema;
- schema-qualified targets use the named schema without requiring a selected
  schema;
- reserved `_mylite_*` names are rejected before generated SQLite SQL;
- unknown schema, unknown table, unsupported object kind, and missing default
  schema diagnostics reuse existing `INSERT` behavior.

Duplicate assignment resolution remains descriptor-driven:

- every assignment target resolves through the target table descriptor;
- assignment targets may explicitly name invisible descriptor columns;
- assignment targets must be unqualified;
- each target column may appear at most once;
- the arithmetic source column must resolve to the same descriptor column as
  the assignment target;
- the arithmetic target must be an integer-family descriptor column, must not
  be an `AUTO_INCREMENT` column, and must not participate in a supported primary
  or unique key descriptor;
- unknown assignment or arithmetic source columns fail before mutation;
- descriptor case sensitivity and collation follow the current catalog
  identifier policy used by existing ODKU assignment resolution.

For a duplicate row, arithmetic conversion is evaluated from the conflicting
stored row value:

- if the stored value is `NULL`, the assignment value is `NULL`;
- otherwise the stored value must be an integer storage value;
- the unsigned delta literal is parsed by MyLite before binding;
- the result must fit the target descriptor's logical signed or unsigned range
  and the current signed 64-bit physical storage envelope;
- successful arithmetic produces one planned integer value that is bound into
  the existing descriptor-built SQLite `UPDATE`.

The generated physical update keeps the existing ODKU shape:

```sql
UPDATE "physical_table"
   SET "target" = ?1[, ...]
 WHERE "conflicting_key_part" = ?N
   AND ("target" IS NULL OR "target" <> ?M)
```

For `NULL` assignment values, the changed-row predicate uses
`"target" IS NOT NULL`, matching the existing non-arithmetic ODKU path. All
identifiers are quoted. Assignment values, conflicting key values, and changed
condition values are bound parameters. Literal text is not interpolated into
generated SQLite SQL.

## Diagnostics

The feature preserves existing diagnostics for public API misuse, syntax
errors, missing default schema, unknown schema, unknown table, reserved target
names, unsupported object kinds, unknown assignment columns, duplicate
assignment targets, `NULL` into `NOT NULL` through non-arithmetic assignments,
physical SQLite failures, and allocation failures.

New or refined deterministic diagnostics:

- unsupported arithmetic shape, signed delta token, table-qualified arithmetic
  source, cross-column arithmetic, non-integer target, key target, or
  `AUTO_INCREMENT` target:
  `INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only
  same-column integer + or - unsigned integer literals on non-key columns`;
- unsigned delta literal outside the signed 64-bit range when a non-`NULL`
  duplicate source row would be evaluated:
  `INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only
  unsigned integer deltas in signed 64-bit range`;
- signed `BIGINT` arithmetic overflow: `1690 / 22003`;
- unsigned subtraction underflow: `1690 / 22003`;
- smaller integer-family range failures: `1264 / 22003` with the existing
  MySQL-shaped column range message;
- any `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE` same-column arithmetic
  assignment:
  `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE arithmetic assignment is not
  supported`;
- unexpected physical storage type for an integer descriptor source row:
  physical SQLite row error.

Successful in-range updates emit no warnings. `VALUES()` warnings in other
assignments remain unchanged.

## Tests

Tests must be MySQL-runtime verified against MySQL 8.4.9 and cover:

- parser acceptance for `n = n + 1` and `n = n - 1`;
- successful duplicate update for `INT`, `INTEGER`, `BIGINT`, `INT UNSIGNED`,
  `INTEGER UNSIGNED`, and `BIGINT UNSIGNED` target columns;
- successful duplicate update from both `INSERT ... VALUES` and one-row
  `INSERT ... SET` source envelopes;
- `NULL` propagation and no-op affected rows for `NULL + N`;
- `+ 0` no-op affected rows;
- multi-row sequential affected-row accumulation;
- coexistence with existing ODKU literal and `VALUES()` assignments when
  targets are distinct;
- reopen persistence and independent file-backed handles;
- range boundaries and out-of-range diagnostics for signed and unsigned
  integer families;
- `INSERT ... SELECT` arithmetic duplicate assignments, unknown source columns,
  cross-column arithmetic, key target arithmetic, auto-increment target
  arithmetic, signed deltas, table-qualified sources, string/decimal/float/hex/bit
  deltas, function expressions, parameters, and subqueries rejected
  deterministically;
- unchanged `.mylite` preamble after arithmetic updates;
- existing parser/runtime/ODKU/update/insert lifecycle tests still pass.

## Performance And SQLite Integration

The implementation is a MyLite wrapper/translation feature over public SQLite
prepared statements. It does not require a SQLite fork hook.

Only rows that hit the duplicate-key branch need the conflicting stored row
value. MyLite fetches the existing duplicate row through the already selected
descriptor key predicate, computes the narrow arithmetic value once, and then
issues the same parameterized physical update shape used by existing ODKU.
There is no table-wide materialization for this feature.
