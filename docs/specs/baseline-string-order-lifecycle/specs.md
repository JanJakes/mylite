# Baseline String Order Lifecycle

## Summary

This phase extends the existing descriptor-backed one-column `ORDER BY` path to
nonbinary MyLite string descriptors. The immediate application-facing target is
queries such as:

```sql
UPDATE t SET v = 'x' WHERE k = 'a' ORDER BY k LIMIT 1
```

where `k` and `v` are `CHAR`, `VARCHAR`, or baseline `TEXT` family columns.
The slice also applies to the shared single-table descriptor `SELECT`, `DELETE`,
`INSERT ... SELECT`, `REPLACE ... SELECT`, and scalar-subquery source planning
paths that already use the same `ORDER BY` planner.

This is not full MySQL string ordering. MyLite currently has an ASCII
`utf8mb4_0900_ai_ci` collation implementation for equality predicates and key
checks. This phase uses that registered collation for supported ordering and
claims MySQL parity only for stored string values in the verified ASCII subset.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing string, ordering, and DML slices:
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-delete-lifecycle/specs.md`
  - `docs/specs/baseline-string-equality-predicates/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-text-type/specs.md`
  - `docs/compatibility/sql-query-expressions.md`
  - `docs/compatibility/sql-table-dml.md`
- Official MySQL 8.4 Reference Manual:
  - sorting rows:
    <https://dev.mysql.com/doc/refman/8.4/en/sorting-rows.html>
  - character sets and collations:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-general.html>
  - `UPDATE` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - `DELETE` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/delete.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_order_lifecycle_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 behavior used for this slice:

- `ORDER BY string_column` is accepted for `CHAR`, `VARCHAR`, and `TEXT`
  columns in ordinary single-table `SELECT`, single-table `UPDATE`, and
  single-table `DELETE`.
- The default direction is ascending; `ASC` is explicit ascending; `DESC`
  reverses the sort direction.
- `NULL` values sort before non-`NULL` values in ascending order and after
  non-`NULL` values in descending order.
- Under `utf8mb4_0900_ai_ci`, ASCII case differences do not create distinct
  ordering weights. If two values compare equal at the admitted sort key, this
  phase does not claim which tied row MySQL picks for limited DML.
- `VARCHAR` and `TEXT` preserve trailing spaces under the existing MyLite
  storage policy and MySQL's default no-pad collation surface used by the
  probes. `CHAR` ordering observes the stored canonical `CHAR` readback shape.
- For single-table `UPDATE` and `DELETE`, `ORDER BY` is meaningful for this
  slice when paired with `LIMIT row_count`; `ORDER BY` without `LIMIT` is
  accepted but has no additional visible row-set effect for the constant
  assignment/delete cases admitted here.
- Successful supported statements report warning count `0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to return non-row results
  for DML and row results for `SELECT` through the existing result API.
- Statement context: no new statement state. Warning counts, affected rows, and
  result ownership follow the existing `SELECT`, `UPDATE`, and `DELETE`
  conventions.
- Lexer/parser/AST: unchanged. Existing grammar admits one `ORDER BY`
  qualified identifier with optional `ASC`/`DESC`; unsupported wider order
  shapes continue to be parsed or rejected by the current grammar/analyzer.
- Analyzer/planner: expands the existing order-column type gate. Order columns
  still resolve through MyLite descriptors and current source/alias rules.
- Catalog: read-only descriptor authority. This phase does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: generated SQL continues to use stable MyLite
  physical table names and quoted descriptor identifiers. String order keys add
  MyLite's registered SQLite collation name to the generated order expression.
- SQLite integration: uses the existing public `sqlite3_create_collation_v2()`
  registration path. No SQLite fork patch is required.
- Storage/VFS: unchanged. Sorted reads and ordered limited DML do not touch the
  `.mylite` preamble or shifted SQLite payload invariants beyond normal row
  writes for DML.

## Supported SQL

The existing one-column `ORDER BY` grammar remains the syntax boundary:

```sql
SELECT ... FROM table_name [WHERE predicate]
  [ORDER BY string_column [ASC | DESC]] [LIMIT ...]

UPDATE table_name SET assignment_list [WHERE predicate]
  [ORDER BY string_column [ASC | DESC]] [LIMIT row_count]

DELETE FROM table_name [WHERE predicate]
  [ORDER BY string_column [ASC | DESC]] [LIMIT row_count]
```

The same source `SELECT` order support is available to currently supported
`INSERT ... SELECT`, `REPLACE ... SELECT`, and scalar-subquery update-assignment
source forms because they reuse descriptor `SELECT` planning.

`string_column` may be an existing unqualified or currently accepted qualified
descriptor reference according to the statement family:

- supported table `SELECT` may use the current descriptor/source-qualification
  and select-item alias rules;
- `UPDATE` and `DELETE` remain unqualified descriptor-only order keys;
- unsupported order expressions, ordinals, string-literal order keys, and
  multiple order keys remain outside this phase.

Admitted descriptor types:

- `CHAR`;
- `VARCHAR`;
- `TINYTEXT`;
- `TEXT`;
- `MEDIUMTEXT`;
- `LONGTEXT`.

### MyLite Lemon-Syntax Snippet

No parser production changes are needed. Existing MyLite grammar already admits
the order clause shape:

```lemon
order_clause_opt(A) ::= .
order_clause_opt(A) ::= ORDER BY qualified_identifier(K) order_direction_opt(D).

order_direction_opt(A) ::= .
order_direction_opt(A) ::= ASC.
order_direction_opt(A) ::= DESC.
```

Analyzer acceptance for this phase:

```lemon
string_order_key(A) ::= descriptor_string_column(B).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Semantics

Planning:

1. Resolve the order key using the existing descriptor-column or select-item
   alias resolver for the current statement kind.
2. Continue to reject unknown, ambiguous, qualified-where-disallowed,
   expression, ordinal, and multiple-key forms with deterministic diagnostics.
3. Accept the order key when it is an existing numeric/temporal order type or
   one of the admitted nonbinary string descriptor types.
4. Keep `ASC` as the default direction and support explicit `ASC` / `DESC`.
5. Preserve existing `LIMIT` and `OFFSET` rules for `SELECT` and existing
   `LIMIT row_count` rules for `UPDATE` / `DELETE`.

Generated SQLite order SQL for string descriptors:

```sql
ORDER BY "column_name" COLLATE "utf8mb4_0900_ai_ci" ASC
ORDER BY "column_name" COLLATE "utf8mb4_0900_ai_ci" DESC
```

For joined or otherwise qualified current source shapes, the descriptor column
is generated with the existing source alias and then the same collation suffix.
`TIME` and other existing non-string order keys retain their current generated
forms.

Supported comparison behavior:

- MyLite's registered `utf8mb4_0900_ai_ci` collation folds ASCII `A` through
  `Z` to `a` through `z` and otherwise compares byte sequences.
- The verified compatibility claim covers ASCII stored strings. Full Unicode
  UCA 9.0 weights, accent-insensitive comparison, contractions, expansions,
  locale-tailored weights, and connection-collation effects remain deferred.
- `NULL` ordering is inherited from SQLite's `ORDER BY` behavior and verified
  against MySQL for the admitted directions.
- Rows tied by the single admitted sort key have no MyLite-guaranteed relative
  order. Tests must not depend on which tied row is selected by
  `ORDER BY string_column LIMIT n`.

## Diagnostics

Existing diagnostics remain in force:

- syntax errors through existing parse diagnostics;
- missing default schema, unknown schema, unknown table, and unsupported object
  kind through existing source/target resolvers;
- reserved `_mylite_*` names through existing policy;
- unknown order columns through current descriptor diagnostics;
- ambiguous order aliases through current select-order diagnostics;
- table-qualified `UPDATE` / `DELETE` order columns through existing
  deterministic unsupported diagnostics;
- unsupported order expressions, ordinals, and multiple keys through current
  deterministic unsupported diagnostics;
- unsupported descriptor types such as `ENUM`, `SET`, `JSON`, binary string,
  `DECIMAL`, and approximate numeric order keys continue to fail with an
  unsupported order diagnostic unless another feature has explicitly admitted
  them;
- physical SQLite failures and allocation failures use existing runtime error
  handling.

## Tests

Add a fast C runtime test, preferably `runtime_string_order_lifecycle`, covering:

- `SELECT ... ORDER BY` over `VARCHAR`, `CHAR`, and `TEXT` descriptors;
- default, explicit `ASC`, and `DESC` direction;
- `NULL` placement;
- trailing-space visibility for `VARCHAR` / `TEXT` and canonical `CHAR`
  readback;
- `UPDATE ... WHERE string_predicate ORDER BY string_column LIMIT row_count`;
- `UPDATE ... ORDER BY text_column` without `LIMIT` accepted for the admitted
  constant assignment case;
- `DELETE ... ORDER BY string_column LIMIT row_count`;
- `INSERT ... SELECT ... ORDER BY string_column LIMIT row_count` source order
  reuse when the existing no-key target restrictions are met;
- reopen persistence after an ordered limited string-key update;
- independent file-backed handles retaining independent ordered update state;
- deterministic rejection of binary string, `ENUM`, `SET`, `JSON`, decimal, or
  approximate order keys where those are still unsupported;
- unchanged parser/runtime lifecycle tests for existing numeric and temporal
  ordering.

Expected user-visible MySQL behavior is verified by
`packages/libmylite/tests/mysql_baseline_string_order_lifecycle_expectations.sh`.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/sql-table-dml.md`.

Do not claim full string ordering, multiple order keys, ordinals, expressions,
explicit `COLLATE`, mutable connection collation behavior, binary-string
ordering, enum/set ordering, JSON ordering, or full Unicode
`utf8mb4_0900_ai_ci` parity.
