# Baseline UPDATE Subquery Predicates

## Summary

This phase extends the existing descriptor-driven single-table `UPDATE` slice
with the already supported `IN` and `EXISTS` subquery predicate envelopes in the
outer `WHERE` clause:

```sql
UPDATE target
SET assignment_column = supported_update_value
WHERE target_column [NOT] IN (
    SELECT source_column FROM source [WHERE inner_predicate]
)

UPDATE target
SET assignment_column = supported_update_value
WHERE [NOT] EXISTS (
    SELECT select_item_list FROM source [WHERE inner_predicate] [LIMIT row_count]
)
```

The goal is to cover common membership and existence update filters, including
WordPress-style `UPDATE ... WHERE option_name IN (SELECT option_name FROM ...)`
statements, without claiming general DML subquery support. The implementation
reuses MyLite's descriptor-backed `SELECT` predicate planning for `IN` and
`EXISTS`, aliases the physical target table only when required by subquery
predicate SQL, and keeps catalog descriptors authoritative.

This is not full MySQL subquery support. The outer statement remains the
current single-table persistent or shadowing session-temporary base-table
`UPDATE` subset. The inner subquery shapes remain the current limited `IN` and
`EXISTS` descriptor envelopes. Joined updates, scalar assignment expansion,
derived tables, nested subqueries, quantified comparisons, broad expressions,
and general optimizer behavior are deferred.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-in-subquery-predicates/specs.md`
  - `docs/specs/baseline-exists-subquery-predicates/specs.md`
  - `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - subqueries: <https://dev.mysql.com/doc/refman/8.4/en/subqueries.html>
  - subqueries with `ANY`, `IN`, or `SOME`:
    <https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html>
  - `EXISTS` and `NOT EXISTS` subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/exists-and-not-exists-subqueries.html>
  - correlated subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/correlated-subqueries.html>
  - subquery errors:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html>
  - subquery restrictions:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_subquery_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `UPDATE ... WHERE column IN (SELECT column FROM other_table)` updates rows
  where membership is true and reports changed rows through `ROW_COUNT()`.
- `NOT IN` preserves MySQL three-valued semantics. If the inner result contains
  `NULL` and no non-`NULL` value matches, the predicate is unknown and does not
  update the row.
- `IN` over an empty subquery is false. `NOT IN` over an empty subquery is true.
- `EXISTS` over a nonempty subquery is true for all evaluated outer rows.
  `EXISTS` over an empty subquery is false. `NOT EXISTS` is the inverse.
- `EXISTS (SELECT ... LIMIT 0)` is false and updates no rows.
- Inner `WHERE` predicates filter the subquery before membership or existence
  is evaluated.
- Correlated inner predicates may reference the target row. Supported probes
  include integer equality and null-safe equality shapes already admitted by
  MyLite's baseline `SELECT` subquery predicates.
- `ORDER BY ... LIMIT row_count` on the outer `UPDATE` applies after the outer
  subquery predicate filter. For example, descending target ordering with
  `LIMIT 1` updates the highest ordered matching target row.
- A subquery that reads the same base table being updated fails at plan time
  with `1093 / HY000`, `You can't specify target table '...' for update in
  FROM clause`.
- `IN` subqueries with inner `LIMIT` keep MySQL's existing
  `1235 / 42000` diagnostic for this subquery family.
- Successful supported updates return no result rows, report changed-row
  affected counts, and leave `@@warning_count = 0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation,
  diagnostics, result handles, and public misuse behavior.
- Statement context: owns the outer update statement boundary, diagnostics
  reset, warning count, changed-row affected count, and transaction completion.
  Inner subqueries do not publish independent result objects or completion
  state.
- Lexer/parser/AST: already admits the relevant predicate AST nodes in `WHERE`.
  No grammar broadening is required for this phase. The AST stores predicate
  shape and source spans only; it does not bind names or object kinds.
- Analyzer/planner: resolves the update target, assignment column, outer
  predicate column, inner source table, inner selected column, inner predicate
  columns, correlated outer references, order columns, limits, and unsupported
  shapes from MyLite descriptors.
- Catalog: remains authoritative for logical schema names, table identity,
  object kind, physical table names, column metadata, visible temporary-table
  shadowing, and descriptor versions. Supported update subquery predicates read
  descriptors only and must not mutate catalog rows, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- Result builder: successful supported updates return the existing non-row
  statement shape with exact changed-row `affected_rows`, zero columns, zero
  result rows, and `warning_count == 0`.
- SQLite physical row storage: executes a MyLite-generated physical `UPDATE`
  over generated table names. MyLite quotes identifiers and binds assignment,
  predicate, and limit values. The physical target table is aliased as
  `_mylite_s0` only when predicate SQL needs descriptor source qualification.
- Storage/VFS/file format: unchanged. Supported updates mutate only ordinary
  SQLite payload rows and must not touch the `.mylite` preamble or VFS shifted
  payload invariants.

## Supported SQL

Outer statement subset:

```sql
UPDATE table_name
SET supported_assignment_list
[WHERE update_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

The supported assignment list is exactly the current single-table `UPDATE`
assignment subset. This phase does not add new assignment expression forms.

`update_predicate` is the existing baseline descriptor predicate boolean
expression plus the following atoms:

```sql
qualified_identifier IN ( in_subquery )
qualified_identifier NOT IN ( in_subquery )
EXISTS ( exists_subquery )
NOT EXISTS ( exists_subquery )
```

The parser may represent `NOT IN` and `NOT EXISTS` through the existing
predicate node structure or a `NOT` wrapper. The concrete AST shape is an
implementation detail as long as the planned predicate semantics match MySQL
for the admitted subset.

Supported `IN` inner subquery:

```sql
in_subquery:
    SELECT qualified_identifier
    FROM table_name [table_alias]
    [WHERE inner_predicate]
```

Supported `EXISTS` inner subquery:

```sql
exists_subquery:
    SELECT exists_select_item [, exists_select_item ...]
    [FROM table_name [table_alias]]
    [WHERE inner_predicate]
    [LIMIT row_count]
```

`EXISTS` keeps the existing tableless and `DUAL` support from the
`SELECT ... WHERE EXISTS` slice.

Supported inner predicates:

- the current descriptor-backed literal predicate subset over inner source
  columns;
- existing logical composition with `NOT`, `AND`, `XOR`, `OR`, and
  parentheses;
- existing correlated comparison atoms admitted by the baseline `SELECT`
  subquery predicate slices:
  - `inner_integer_column = outer_integer_column`;
  - `outer_integer_column = inner_integer_column`;
  - `inner_integer_column <=> outer_integer_column`;
  - `outer_integer_column <=> inner_integer_column`.

Supported `IN` membership value families are exactly the current
`SELECT ... WHERE IN (subquery)` families:

- compatible MyLite integer-family descriptors, including aliases and currently
  supported unsigned physical ranges;
- compatible ASCII nonbinary string-family descriptors in the current
  `CHAR`, `VARCHAR`, and baseline `TEXT` family subset, using MyLite's
  registered `utf8mb4_0900_ai_ci` collation for the verified ASCII behavior.

Unsupported in this phase:

- joined or multi-table `UPDATE` subquery predicates;
- subquery predicates in `DELETE`;
- inner `IN` subquery `LIMIT`, `ORDER BY`, `DISTINCT`, aggregates, grouping,
  joins, CTEs, nested subqueries, derived tables, tableless/`DUAL`,
  expressions, functions, parameters, wildcard or row-constructor projection;
- `EXISTS` inner joins, grouping, ordering, locking clauses, CTEs, nested
  subqueries, or broad expression evaluation beyond its current select-item
  and predicate subset;
- scalar assignment expansion, multi-table update, optimizer hint behavior
  beyond existing no-op validation, triggers, cascades beyond currently
  supported descriptor foreign-key actions, or privilege semantics.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's supported subset, not MySQL's full grammar:

```lemon
update_statement ::=
    UPDATE update_target SET update_assignment_list update_where_opt
           update_order_opt update_limit_opt.

update_where_opt ::= .
update_where_opt ::= WHERE update_predicate.

update_predicate ::= baseline_predicate.
update_predicate ::= qualified_identifier IN LPAREN in_subquery RPAREN.
update_predicate ::= qualified_identifier NOT IN LPAREN in_subquery RPAREN.
update_predicate ::= EXISTS LPAREN exists_subquery RPAREN.
update_predicate ::= NOT EXISTS LPAREN exists_subquery RPAREN.

in_subquery ::=
    SELECT qualified_identifier FROM table_name table_alias_opt
           where_clause_opt.

exists_subquery ::=
    SELECT exists_select_list from_clause_opt where_clause_opt limit_clause_opt.
```

The concrete parser already has the needed predicate and subquery AST nodes, so
this phase should not introduce new grammar unless implementation discovery
proves a missing parser path.

## Semantics

Planning:

1. Resolve the target using the existing selected/default schema policy.
   Schema-qualified targets use the named schema. Unqualified targets require
   the selected/default schema. Visible session-temporary tables keep the
   current shadowing policy.
2. Reject reserved `_mylite_*` schema or table names before any SQLite SQL is
   generated.
3. Resolve assignment targets, outer predicate columns, ordering columns, and
   limits through MyLite descriptors.
4. For an `IN` or `EXISTS` predicate, construct a one-source outer context for
   the update target. The context is the logical target descriptor source and
   maps to physical source alias `_mylite_s0` in generated SQL whenever the
   planned predicate requires column qualification.
5. Resolve the inner source through descriptors, including schema-qualified
   names and visible temporary-table shadowing.
6. Reject unsupported object kinds once non-base-table descriptors exist.
7. Resolve inner selected columns and inner predicate columns through the inner
   descriptor source first. Resolve correlated references through the outer
   update target source when the existing correlation subset permits them.
8. Reject any `IN` or table-backed `EXISTS` subquery whose resolved inner source
   table id is the same base table id as the update target, using MySQL's
   `1093 / HY000` same-table update diagnostic.
9. Keep the current baseline `IN` type-compatibility checks and `EXISTS`
   select-list validation.

Execution:

- No MyLite rowset materialization is added for this feature. MyLite lowers the
  supported predicate to a SQLite subquery inside the physical update `WHERE`
  condition.
- If the update has no `LIMIT`, generated SQL is shaped as:

  ```sql
  UPDATE "physical_target" [AS _mylite_s0]
  SET "assignment_column" = ?1
  WHERE <descriptor-built predicate>
    AND <changed-row condition>
  ```

- If the update has `ORDER BY` or `LIMIT`, MyLite keeps its existing portable
  rowid-filter shape rather than relying on SQLite's optional
  `UPDATE ... ORDER BY ... LIMIT` syntax:

  ```sql
  UPDATE "physical_target" [AS _mylite_s0]
  SET "assignment_column" = ?1
  WHERE rowid IN (
      SELECT rowid
      FROM "physical_target" [AS _mylite_s0]
      WHERE <descriptor-built predicate>
      ORDER BY <descriptor-built order>
      LIMIT ?N
  )
    AND <changed-row condition>
  ```

  The rowid usage remains an internal invariant of generated MyLite user
  physical tables. It is not exposed in public SQL.

- `EXISTS` subqueries lower to `EXISTS (SELECT 1 ... )`. `EXISTS` with an
  admitted `LIMIT 0` may lower to a constant false predicate.
- `IN` subqueries lower to `outer_descriptor_value IN (SELECT
  inner_descriptor_value FROM inner_physical AS _mylite_s1 ...)`.
- String-family membership values and predicates use MyLite's registered ASCII
  `utf8mb4_0900_ai_ci` collation in the same places as the baseline
  `SELECT ... IN (subquery)` slice.
- Assignment conversion, non-strict/`IGNORE` handling, duplicate-key checks,
  foreign-key checks, and changed-row detection remain owned by the existing
  update execution path. This phase changes row filtering only.

## Diagnostics

Use existing MySQL-compatible diagnostics where the supported planning path
already has one:

- syntax errors: `1064 / 42000`;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- unknown assignment, predicate, order, or inner selected columns:
  `1054 / 42S22`;
- same target table read from an update subquery: `1093 / HY000`;
- multi-column `IN` subquery projection: `1241 / 21000`;
- unsupported `IN` subquery `LIMIT`: `1235 / 42000`;
- unsupported object kind, inner clauses, broad expressions, or planner shapes:
  deterministic MyLite capability errors with `1064 / 42000`;
- physical SQLite failures and allocation failures: existing internal
  diagnostic policy.

Successful supported updates return no rows, `warning_count == 0`, and affected
rows equal changed rows.

## Performance And Storage

The feature stays close to SQLite's execution path. MyLite does not prefetch or
materialize inner subquery result sets in C. It builds a descriptor-authorized
physical SQL predicate and lets SQLite evaluate the filter and mutation. The
only additional MyLite work is descriptor planning, parameter binding, and
existing changed-row/constraint bookkeeping. No indexes, schema rows, file
format bytes, VFS behavior, or SQLite fork patches are added.

## Tests

Add `packages/libmylite/tests/runtime_update_subquery_predicates_test.c` and a
MySQL expectation script
`packages/libmylite/tests/mysql_baseline_update_subquery_predicates_expectations.sh`.

Cover:

- successful `UPDATE ... WHERE id IN (SELECT user_id FROM orders)`;
- inner `WHERE` filtering for `IN`;
- `NOT IN` with and without inner `NULL` values;
- `IN` over an empty subquery;
- `EXISTS`, `NOT EXISTS`, empty table `EXISTS`, and `EXISTS ... LIMIT 0`;
- correlated `IN` and correlated `EXISTS` using `=` and `<=>` within the
  existing integer correlation subset;
- schema-qualified update targets and inner sources;
- `ORDER BY ... LIMIT` over a subquery-filtered update;
- string-family `IN` subquery filtering with the registered ASCII collation;
- same-target table subquery rejection with `1093 / HY000`;
- unknown inner source/column diagnostics and unsupported inner clauses;
- persistence after close/reopen and `.mylite` preamble preservation;
- existing update, `IN` subquery, `EXISTS` subquery, parser, and full workflow
  regression checks.

## Verification

Required before committing:

1. `cmake --build --preset dev`
2. Focused CTest entries for update lifecycle, update scalar subquery
   assignment, update subquery predicates, `IN` subquery predicates, and
   `EXISTS` subquery predicates.
3. `./packages/libmylite/tests/mysql_baseline_update_subquery_predicates_expectations.sh`
4. `cmake --workflow --preset check`
5. Feature review through `mylite-review-feature`, fixes amended where needed,
   commit, and push `origin main`.
