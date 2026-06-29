# Baseline Quantified Subquery Predicates

## Summary

This phase adds a narrow quantified-comparison subquery predicate slice:

```sql
SELECT select_list
FROM outer_descriptor_source
WHERE outer_column comparison_operator ANY  (SELECT inner_column FROM inner_source [WHERE ...])
WHERE outer_column comparison_operator SOME (SELECT inner_column FROM inner_source [WHERE ...])
WHERE outer_column comparison_operator ALL  (SELECT inner_column FROM inner_source [WHERE ...])
```

The supported surface is a descriptor-backed `SELECT` predicate over the
current one-table or joined outer source envelope. The inner subquery reuses
the existing one-column `IN` subquery planning envelope, including optional
`DISTINCT`, optional aliases, optional limited inner predicates, simple
correlated integer equality, and descriptor-backed joined inner sources.

This is not full MySQL quantified subquery support. It intentionally does not
cover literal-left comparisons, row constructors, tableless/`DUAL` subqueries,
`TABLE` subqueries, expression/aggregate/grouped inner projections, quantified
subqueries in DML predicates, or arbitrary expression contexts.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-in-subquery-predicates/specs.md`
  - `docs/specs/baseline-exists-subquery-predicates/specs.md`
  - `docs/specs/baseline-comparison-result-is-predicates/specs.md`
- Official MySQL 8.4 Reference Manual:
  - subqueries with `ANY`, `IN`, or `SOME`:
    <https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html>
  - subqueries with `ALL`:
    <https://dev.mysql.com/doc/refman/8.4/en/all-subqueries.html>
  - subquery errors:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_quantified_subquery_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local MySQL 8.4.9 runtime:

- `ANY` and `SOME` compare the left operand against each row produced by the
  one-column subquery and are true when at least one comparison is true.
- `SOME` is an alias for `ANY`.
- `ALL` is true when the comparison is true for every subquery row.
- `ANY` over an empty subquery is false.
- `ALL` over an empty subquery is true.
- If no decisive true or false result exists and at least one comparison result
  is `NULL`, the quantified predicate result is unknown.
- In a direct `WHERE` predicate, unknown filters the row.
- Postfix `IS UNKNOWN` over a quantified comparison matches the rows where the
  quantified comparison result is `NULL`.
- `NOT (outer_column = ANY (...))` preserves unknown rather than treating it as
  true.
- Default `utf8mb4_0900_ai_ci` string comparisons are case-insensitive for the
  verified ASCII subset.
- A correlated inner predicate such as `i.v = o.v` filters the inner subquery
  per outer row.
- A quantified subquery returning more than one column fails with
  `1241 / 21000`, `Operand should contain 1 column(s)`.
- `LIMIT` inside `IN`, `ALL`, `ANY`, or `SOME` subqueries fails with
  `1235 / 42000` and the MySQL message for
  `LIMIT & IN/ALL/ANY/SOME subquery`.
- `<=> ANY (subquery)` is a MySQL syntax error; the null-safe comparison
  operator is not in the quantified-comparison operator set for this syntax.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public validation, result
  handles, diagnostics, and public misuse behavior.
- Statement context: owns the outer statement boundary, diagnostics reset,
  warning count, row count, found-rows state, and transaction completion.
  Inner quantified subqueries do not publish independent result objects.
- Lexer/parser/AST: admits descriptor-column quantified predicate atoms and
  records the left operand, comparison operator, quantifier, and inner
  `select_statement` as AST nodes only. It does not resolve descriptors or
  lower directly to SQLite.
- Analyzer/planner: resolves the outer column, inner selected column, source
  contexts, inner predicates, simple correlated references, and unsupported
  shapes from MyLite descriptors.
- Catalog: remains authoritative for schemas, object kind, visible temporary
  table shadowing, physical table names, columns, and descriptor metadata.
  Quantified subquery predicates are read-only and must not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical row storage: executes MyLite-generated correlated `EXISTS`
  probes over quoted physical table names and bound predicate parameters.
- Storage/VFS/file format: unchanged. The feature is read-only and does not
  touch the `.mylite` preamble or shifted SQLite payload.

## Supported SQL

Outer statement subset:

```sql
SELECT outer_select_list
FROM descriptor_source
WHERE outer_predicate
[ORDER BY order_column [ASC | DESC]]
[LIMIT select_limit]
```

`descriptor_source` may be the current one-table or supported joined-source
`SELECT` envelope. `outer_predicate` is the existing descriptor-backed
predicate subset plus the new quantified subquery predicate atom, composed
through the existing `NOT`, `AND`, `XOR`, `OR`, and parenthesized predicate
rules.

Supported quantified predicate atoms:

```sql
quantified_subquery_predicate:
    qualified_identifier =  ANY  ( in_subquery )
  | qualified_identifier <> ANY  ( in_subquery )
  | qualified_identifier != ANY  ( in_subquery )
  | qualified_identifier <  ANY  ( in_subquery )
  | qualified_identifier <= ANY  ( in_subquery )
  | qualified_identifier >  ANY  ( in_subquery )
  | qualified_identifier >= ANY  ( in_subquery )
  | qualified_identifier comparison_operator SOME ( in_subquery )
  | qualified_identifier comparison_operator ALL  ( in_subquery )
```

The `comparison_operator` token family is limited to `=`, `<>`, `!=`, `<`,
`<=`, `>`, and `>=`.

Supported postfix comparison-result predicates:

```sql
quantified_subquery_predicate IS NULL
quantified_subquery_predicate IS NOT NULL
quantified_subquery_predicate IS UNKNOWN
quantified_subquery_predicate IS NOT UNKNOWN
```

Supported inner subquery:

```sql
in_subquery:
    SELECT [DISTINCT] qualified_identifier
    FROM descriptor_table_or_supported_join_source
    [WHERE inner_predicate]
```

The selected inner item must be one explicit descriptor column. Inner
tableless/`DUAL`, wildcard, expression, function, aggregate, grouped, derived,
ordered, and limited subquery forms are deferred, except that MyLite returns
the MySQL-compatible `1235 / 42000` diagnostic for an inner `LIMIT`.

Supported value families:

- compatible MyLite integer-family descriptor columns;
- ASCII nonbinary string descriptor columns in the current `CHAR`, `VARCHAR`,
  and baseline `TEXT` subset.

The outer and inner selected descriptor columns must be in compatible
families. This slice does not add cross-family quantified comparison coercion.

## Semantics

The generated predicate preserves MySQL three-valued behavior:

- `ANY` / `SOME`:
  - true when at least one inner row compares true;
  - unknown when no comparison is true and at least one comparison is unknown;
  - false otherwise, including an empty inner subquery.
- `ALL`:
  - false when at least one inner row compares false;
  - unknown when no comparison is false and at least one comparison is unknown;
  - true otherwise, including an empty inner subquery.

MyLite lowers the supported predicate to generated SQLite SQL using two
correlated `EXISTS` probes. One probe checks for decisive true or false rows;
the other checks for unknown rows. The SQL builder binds the inner predicate
parameters for each probe separately, preserving the existing parameter order.

## MyLite Grammar Snippet

These snippets describe the intended MyLite-owned grammar shape.

```lemon
predicate_atom ::=
    qualified_identifier EQUAL quantified_subquery_quantifier
        LPAREN select_statement RPAREN predicate_comparison_result_is_opt.
predicate_atom ::=
    qualified_identifier NOT_EQUAL quantified_subquery_quantifier
        LPAREN select_statement RPAREN predicate_comparison_result_is_opt.
predicate_atom ::=
    qualified_identifier LESS quantified_subquery_quantifier
        LPAREN select_statement RPAREN predicate_comparison_result_is_opt.
predicate_atom ::=
    qualified_identifier LESS_EQUAL quantified_subquery_quantifier
        LPAREN select_statement RPAREN predicate_comparison_result_is_opt.
predicate_atom ::=
    qualified_identifier GREATER quantified_subquery_quantifier
        LPAREN select_statement RPAREN predicate_comparison_result_is_opt.
predicate_atom ::=
    qualified_identifier GREATER_EQUAL quantified_subquery_quantifier
        LPAREN select_statement RPAREN predicate_comparison_result_is_opt.

quantified_subquery_quantifier ::= ANY.
quantified_subquery_quantifier ::= SOME.
quantified_subquery_quantifier ::= ALL.
```

`NULL_SAFE_EQUAL` is intentionally absent because MySQL 8.4.9 rejects
`<=> ANY (subquery)` as syntax.

## SQLite Integration

This feature is implemented as a MyLite wrapper/translation layer. It uses
SQLite as the physical row executor for MyLite-generated SQL over descriptor
resolved physical tables. No new SQLite public extension API or targeted
SQLite fork hook is needed.

## Tests

- `packages/libmylite/tests/mysql_baseline_quantified_subquery_predicates_expectations.sh`
  records MySQL 8.4.9 expectations for supported results and selected
  diagnostics.
- `packages/libmylite/tests/parser_select_test.c` verifies parser acceptance
  and AST shape for quantified predicates.
- `packages/libmylite/tests/runtime_quantified_subquery_predicates_test.c`
  verifies MyLite runtime values, unknown propagation, string collation,
  correlation, and diagnostics.

## Known Gaps

- literal-left and expression-left quantified predicates;
- row-constructor quantified comparisons;
- quantified subqueries in `UPDATE`, `DELETE`, `HAVING`, `ON`, projection,
  `ORDER BY`, DML assignment, default, check, or generated-column contexts;
- `TABLE` subqueries, tableless/`DUAL` subqueries, wildcard subqueries,
  expression/function/aggregate/grouped/derived/ordered/limited subqueries;
- exact optimizer parity, semi-join transformations, or cost-model behavior;
- broad cross-family coercion, enum/set operands, and protocol-grade
  expression metadata.
