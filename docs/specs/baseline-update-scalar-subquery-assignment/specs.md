# Baseline UPDATE Scalar Subquery Assignment

## Summary

This phase extends the existing descriptor-driven single-table `UPDATE` slice
with one narrow assignment expression:

```sql
UPDATE target
SET assignment_column = (SELECT source_column FROM source [WHERE ...] [ORDER BY ...] [LIMIT ...])
[WHERE ...]
[ORDER BY ...]
[LIMIT ...]
```

The goal is to support common option-copy statements such as:

```sql
UPDATE _dates
SET option_value = (
    SELECT option_value FROM _options WHERE option_name = 'User 0000019'
)
```

This is not general subquery or expression support. The scalar subquery is
uncorrelated, table-backed, one selected descriptor column, and one descriptor
table source. It reuses the current descriptor-backed `SELECT` source envelope
and preserves the existing UPDATE assignment policy for unchanged rows,
matched-row evaluation, changed-row affected counts, statement atomicity, and
file-backed storage invariants.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-scalar-subquery-projection/specs.md`
  - `docs/specs/baseline-string-equality-predicates/specs.md`
  - `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - scalar subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/scalar-subqueries.html>
  - subquery errors:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html>
  - subquery restrictions:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_scalar_subquery_assignment_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL Runtime Observations

MySQL 8.4.9 establishes these expectations for this slice:

- A scalar subquery can be used as an `UPDATE` assignment value because MySQL
  assignment values are expressions.
- A scalar subquery with exactly one row assigns that value to every matched
  target row.
- A scalar subquery with zero rows evaluates to `NULL`. Assigning that result
  to a nullable target changes the row to `NULL`; assigning it to a `NOT NULL`
  target fails with `1048 / 23000`.
- A scalar subquery with more than one selected column fails with
  `1241 / 21000`, `Operand should contain 1 column(s)`.
- A scalar subquery with more than one produced row fails with
  `1242 / 21000`, `Subquery returns more than 1 row`.
- An `UPDATE` that reads the same base table from a subquery fails with
  `1093 / HY000`, `You can't specify target table '...' for update in FROM
  clause`.
- If the outer `UPDATE` matches no rows, MySQL still reports plan-time errors
  such as multi-column scalar subqueries, unknown source objects, and same-table
  source restrictions, but it does not evaluate runtime scalar values. For
  example, a no-match update skips multi-row scalar cardinality errors and
  skips `NULL`-into-`NOT NULL` assignment errors.
- Successful supported updates report changed rows, not merely matched rows.
  Repeating the same scalar assignment against an already equal row reports
  `ROW_COUNT() = 0`.
- Successful supported updates leave `@@warning_count = 0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation, result
  handle creation/freeing, diagnostics, and public misuse behavior.
- Statement context: owns the outer `UPDATE` statement boundary, diagnostics
  reset, warning count, affected rows, and transaction completion. The scalar
  subquery is an internal operand and must not publish its own public result or
  statement-completion state.
- Lexer/parser/AST: admits a parenthesized `SELECT` as an `update_value` and
  stores it as the existing scalar-subquery AST node. The parser does not bind
  names or decide object kinds.
- Analyzer/planner: resolves the target table, assignment column, outer
  predicate/order columns, inner source table, inner selected column, inner
  predicate/order columns, and unsupported shapes from MyLite descriptors. It
  rejects reserved `_mylite_*` target/source table names before generating
  SQLite SQL.
- Catalog: remains descriptor authority for logical schemas, table identity,
  object kind, physical table names, and column metadata. Scalar subquery
  assignment reads descriptors only; it does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful supported updates return the existing non-row
  result shape: `column_count == 0`, `row_count == 0`, exact changed-row
  `affected_rows`, and `warning_count == 0`.
- SQLite physical storage: owns row storage and mutation inside MyLite-generated
  physical tables. MyLite builds both the scalar source `SELECT` and physical
  `UPDATE` from descriptors, quoted physical identifiers, and bound
  parameters.
- Storage/VFS/file format: unchanged. Supported updates write only the shifted
  SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

The outer statement remains the existing limited single-table `UPDATE`:

```sql
UPDATE table_name
SET column_name = update_value
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

This phase adds one `update_value` form:

```sql
update_value:
    ( SELECT scalar_source_column
      FROM table_name [table_alias]
      [WHERE baseline_predicate]
      [ORDER BY order_column [ASC | DESC]]
      [LIMIT select_limit] )
```

Supported inner `SELECT` subset:

- one descriptor-backed persistent base-table source;
- one selected descriptor column, optionally source-qualified where current
  `SELECT` column resolution supports it;
- optional source table alias using the existing descriptor SELECT alias
  policy;
- optional existing baseline source `WHERE` predicate subset, including the
  string equality predicates required by the motivating example;
- optional existing one-column descriptor `ORDER BY` subset;
- optional existing descriptor `SELECT` `LIMIT` subset, including offset forms
  already supported for table `SELECT`.

Supported source-to-target assignment values:

- `NULL` from an empty or `NULL` scalar result, subject to the target
  nullability policy;
- non-`NULL` values copied only between compatible descriptor families using
  the existing `INSERT ... SELECT` descriptor-copy policy:
  - integer physical columns to integer targets, with target range validation;
  - string-family columns to `CHAR`, `VARCHAR`, and baseline `TEXT` family
    targets, with target length and canonical `CHAR` validation;
  - compatible `DECIMAL`, approximate numeric, and canonical temporal
    descriptor values where the existing descriptor-copy validators already
    support them.

Implicit MySQL conversions such as string-to-integer or integer-to-string are
deferred. MyLite reports deterministic unsupported diagnostics for non-`NULL`
scalar results that would require those conversions. Because MySQL does not
evaluate assignment values for no-match updates, MyLite also skips this
unsupported conversion check when the outer update matches no rows.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's supported extension, not MySQL's full grammar:

```lemon
update_assignment ::= qualified_identifier EQUAL update_value.

update_value ::= INTEGER.
update_value ::= DECIMAL.
update_value ::= FLOAT.
update_value ::= PLUS INTEGER.
update_value ::= PLUS DECIMAL.
update_value ::= PLUS FLOAT.
update_value ::= MINUS INTEGER.
update_value ::= MINUS DECIMAL.
update_value ::= MINUS FLOAT.
update_value ::= NULL.
update_value ::= TRUE.
update_value ::= FALSE.
update_value ::= STRING.
update_value ::= DEFAULT.
update_value ::= LPAREN select_statement RPAREN.

scalar_update_subquery ::=
    LPAREN SELECT select_item FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt RPAREN.
```

Analyzer acceptance narrows `select_item` to one descriptor column and narrows
the source to one persistent base-table descriptor that is not the updated
target table.

## Semantics

Planning:

1. Resolve the outer target table using the existing selected/default schema
   policy. Schema-qualified targets use the named schema. Reserved `_mylite_*`
   names are rejected before physical SQL is generated.
2. Resolve the assignment target from the target table descriptor. The current
   update slice still supports exactly one unqualified assignment column.
3. If the assignment value is not a scalar subquery, preserve existing
   literal/default conversion behavior.
4. If the assignment value is a scalar subquery, validate that it contains one
   inner `SELECT`, one selected descriptor column, one descriptor-backed source
   table, and only admitted optional clauses.
5. Resolve inner source schema/table and inner column references through MyLite
   descriptors. Unknown schemas, tables, assignment columns, predicate columns,
   order columns, or source select columns use existing MySQL-compatible
   diagnostics for those name classes.
6. Reject same-table source/target scalar subqueries with `1093 / HY000`.
7. Reject multi-column scalar subqueries with `1241 / 21000`.

Execution:

1. Start the existing update transaction and test whether the outer update
   matches at least one row.
2. If no row matches, skip scalar value execution and complete with
   `affected_rows == 0`, preserving MySQL's no-match evaluation behavior.
3. If rows match, execute the inner source `SELECT` using descriptor-built
   SQLite SQL. MyLite binds all predicate and limit values. When the inner
   select has no `LIMIT`, or has a row count above two, MyLite may internally
   cap the physical read to two rows because only scalar cardinality matters.
   Explicit `LIMIT 0` and `LIMIT 1` keep their visible scalar semantics.
4. If the inner select returns zero rows, materialize SQL `NULL`.
5. If it returns one row, materialize that single column through
   descriptor-owned conversion and validation before binding it to the physical
   update.
6. If it returns a second row, fail the statement with `1242 / 21000`.
7. Execute the existing physical `UPDATE` shape with a bound assignment
   parameter, the existing outer predicate/order/limit handling, and the
   existing changed-row condition so affected rows count changed rows.
8. Roll back the transaction on any scalar subquery, conversion, constraint,
   or physical SQLite failure.

Ordering:

- Inner `ORDER BY` affects which scalar row is chosen only when paired with a
  limiting form that leaves at most one row.
- For duplicate order-key values without further sort keys, this phase does
  not promise which tied row MySQL chooses.
- Inner `ORDER BY` without a `LIMIT` can still affect physical scan order, but
  if more than one row is produced the scalar assignment fails with `1242`.

## Generated SQLite Shape

Scalar source reads use the existing descriptor SELECT generator:

```sql
SELECT "source_column"
FROM "_mylite_user_table_<source_table_id>"
[WHERE descriptor_predicate_with_bound_values]
[ORDER BY "order_column" ASC|DESC]
[LIMIT ? [OFFSET ?]]
```

Outer updates keep the existing physical shape:

```sql
UPDATE "_mylite_user_table_<target_table_id>"
SET "assignment_column" = ?1
WHERE ...
```

All generated identifiers are descriptor-derived and quoted. User SQL literals
are never interpolated into generated SQLite SQL. Assignment values, predicate
values, and limit values are bound through prepared statements.

No SQLite fork patch or new SQLite extension point is needed for this phase.

## Diagnostics

Supported diagnostics include:

- parser syntax errors through existing parse diagnostics;
- missing default schema, unknown schema, unknown target/source table, reserved
  `_mylite_*` names, and unsupported object kinds through existing catalog
  diagnostics;
- unknown assignment columns, source select columns, predicate columns, and
  order columns through existing deterministic column diagnostics;
- `1241 / 21000` for scalar subqueries that project more than one column;
- `1242 / 21000` for matched updates whose scalar subquery returns more than
  one row;
- `1093 / HY000` for scalar source reads from the same base table being
  updated;
- `1048 / 23000` for assigning scalar `NULL` to a `NOT NULL` target when the
  outer update matches rows;
- existing target range, data-too-long, temporal, decimal, approximate, unique
  key, and physical SQLite diagnostics when the materialized scalar value
  fails target validation or mutation;
- deterministic unsupported diagnostics for no-source/`DUAL` scalar assignment
  subqueries, correlated subqueries, multi-table sources, joins, derived
  tables, CTEs, aggregate or expression inner projections, wildcard scalar
  subqueries, `DISTINCT`, query modifiers, locking clauses, parameters,
  implicit non-`NULL` cross-family conversion, and general expression
  assignment.

## Tests

Add MySQL-runtime expectation coverage and C runtime coverage for:

- string-family copy using the motivating `UPDATE ... SET option_value =
  (SELECT option_value FROM _options WHERE option_name = ...)` shape;
- integer-family copy, including changed rows and no-op affected rows;
- scalar subquery returning zero rows to nullable targets and to `NOT NULL`
  targets;
- scalar subquery returning `NULL`;
- multi-row scalar subquery errors only when the outer update matches rows;
- multi-column scalar subquery errors even when the outer update matches no
  rows;
- same-table scalar subquery errors even when the outer update matches no rows;
- unknown source tables and unknown source/predicate/order columns;
- inner `ORDER BY ... LIMIT 1`, `LIMIT 0`, and offset forms already admitted by
  the table `SELECT` subset;
- outer `WHERE`, `ORDER BY`, and `LIMIT` interaction with scalar assignment;
- persistence after close/reopen and unchanged `.mylite` preamble behavior;
- parser coverage for scalar subquery update values and still-rejected wider
  assignment expression forms;
- existing parser, scalar subquery projection, row-scalar, update, select,
  delete, row-values, storage, catalog, and check-workflow regressions.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-table-dml.md`
- `docs/compatibility/sql-query-expressions.md`

Do not claim general subqueries, correlated subqueries, expression assignment,
implicit conversion, DML assignment outside the limited table-backed scalar
source shape, same-table update-source workarounds, or arbitrary SQLite
pass-through.
