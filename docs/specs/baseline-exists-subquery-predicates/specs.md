# Baseline EXISTS Subquery Predicates

## Summary

This phase adds the first predicate-subquery slice:

```sql
SELECT select_list
FROM outer_table [outer_alias]
WHERE [NOT] EXISTS (
    SELECT exists_select_list
    [FROM inner_table [inner_alias]]
    [WHERE exists_inner_predicate]
    [LIMIT row_count]
)
```

The goal is to cover common application filters such as "return rows that have
a related row" without claiming general subquery or expression support. The
slice is descriptor-driven, keeps catalog descriptors authoritative, and uses
SQLite only as the physical row executor over MyLite-generated SQL.

This is not full MySQL subquery support. The supported outer statement is a
single-table descriptor-backed `SELECT` filter. The supported inner query is
either tableless/`DUAL` or one descriptor-backed persistent base table with an
optional alias, optional limited predicate, and optional `LIMIT row_count`.
Simple correlated equality predicates are supported only inside the inner
`WHERE` clause.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-where-and-predicates/specs.md`
  - `docs/specs/baseline-where-or-predicates/specs.md`
  - `docs/specs/baseline-scalar-subquery-projection/specs.md`
  - `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - subqueries: <https://dev.mysql.com/doc/refman/8.4/en/subqueries.html>
  - `EXISTS` / `NOT EXISTS` subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/exists-and-not-exists-subqueries.html>
  - correlated subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/correlated-subqueries.html>
  - subquery errors:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html>
  - subquery restrictions:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_exists_subquery_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL Runtime Observations

MySQL 8.4.9 establishes these expectations for the supported slice:

- `EXISTS (subquery)` is true when the subquery produces at least one row.
- `NOT EXISTS (subquery)` is false when the subquery produces at least one row
  and true when the subquery produces no rows.
- The selected values in an `EXISTS` subquery do not affect existence
  semantics. `SELECT *`, `SELECT NULL`, `SELECT 1`, and multi-item projection
  lists are all legal when their names and expressions are otherwise valid.
- Names in the inner projection list are still resolved. For example,
  `EXISTS (SELECT missing FROM orders)` fails with unknown-column diagnostics
  even though the projection value is not used for existence.
- Tableless `EXISTS (SELECT 1)` and `EXISTS (SELECT * FROM DUAL)` are true.
- `LIMIT 0` inside the subquery makes the subquery produce no rows, so
  `EXISTS (...)` is false and `NOT EXISTS (...)` is true.
- A correlated subquery can refer to a table in an outer query. MySQL resolves
  names from the inside outward; an unqualified inner column name resolves to
  the inner table when possible.
- Simple correlated equality such as
  `WHERE inner_table.user_id = outer_table.id` filters the inner subquery per
  outer row. The null-safe `<=>` operator also works and matches two `NULL`
  values.
- Missing default schema for unqualified outer or inner tables fails with
  `1046 / 3D000`. Unknown inner tables fail with `1146 / 42S02`. Unknown inner
  or outer column references fail with `1054 / 42S22`.
- MySQL also accepts `EXISTS` predicates in wider contexts such as selected
  scalar expressions and DML `WHERE` clauses. Those contexts are deferred by
  this phase.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation,
  result handles, diagnostics, and public misuse behavior.
- Statement context: owns the outer statement boundary, diagnostics reset,
  warning count, row count, found-rows state, and transaction completion.
  Inner `EXISTS` subqueries do not publish independent result objects or
  statement-completion state.
- Lexer/parser/AST: admits `EXISTS (select_statement)` as a predicate atom.
  Existing keyword `NOT` composition owns `NOT EXISTS`. The parser stores
  source spans and AST children only; it does not bind descriptors.
- Analyzer/planner: resolves the outer source table, inner source table,
  projection names that must be validated, inner predicate columns, correlated
  outer references, inner limit values, and unsupported shapes from MyLite
  descriptors.
- Catalog: remains authoritative for logical schemas, object kind, table
  identity, physical table name, columns, and descriptor metadata. Supported
  `EXISTS` queries read descriptors only and must not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful supported outer `SELECT` statements return the
  existing row-result shape for the outer query only. `EXISTS` does not add
  projection metadata unless later scalar-expression slices admit it.
- SQLite physical row storage: executes the MyLite-generated correlated or
  uncorrelated physical `EXISTS` subquery over generated MyLite user tables.
  MyLite quotes every generated identifier and binds predicate/limit literals.
- Storage/VFS/file format: unchanged. Supported `EXISTS` predicates are
  read-only and must not touch the `.mylite` preamble or shifted SQLite
  payload.

## Supported SQL

Outer statement subset:

```sql
SELECT outer_select_list
FROM table_name [table_alias]
WHERE outer_predicate
[ORDER BY order_column [ASC | DESC]]
[LIMIT select_limit]
```

`outer_predicate` is the existing descriptor-backed `WHERE` subset plus the new
`exists_predicate` atom, composed through the existing `NOT`, `AND`, `XOR`,
`OR`, and parenthesized predicate rules.

Supported `EXISTS` atom:

```sql
exists_predicate:
    EXISTS ( exists_select )
```

Supported inner table-backed subquery:

```sql
exists_select:
    SELECT exists_select_list
    FROM table_name [table_alias]
    [WHERE exists_inner_predicate]
    [LIMIT row_count]
```

Supported inner tableless subquery:

```sql
exists_select:
    SELECT exists_select_list
  | SELECT exists_select_list FROM DUAL
```

Supported `exists_select_list` forms:

- `*`;
- one or more decimal integer, boolean, `NULL`, or string literal items;
- one or more inner descriptor columns, optionally qualified by the inner table
  name or alias.

The selected values are ignored for existence, but every non-wildcard selected
column must resolve successfully. Unknown selected columns fail in the field
list like MySQL.

Supported `exists_inner_predicate` forms:

- the current baseline descriptor-backed predicate subset over inner source
  columns and literal right operands;
- existing logical composition with `NOT`, `AND`, `XOR`, `OR`, and
  parentheses;
- one simple correlated comparison atom:
  - `inner_integer_column = outer_integer_column`;
  - `outer_integer_column = inner_integer_column`;
  - `inner_integer_column <=> outer_integer_column`;
  - `outer_integer_column <=> inner_integer_column`.

Correlated column comparisons are limited to compatible MyLite integer-family
descriptors within the current physical integer range. At least one side must
resolve to the inner source and the other side must resolve to the directly
containing outer source. Unqualified names inside the inner predicate resolve
to the inner source before outer sources, matching MySQL's inside-out rule.
Applications that need deterministic outer references should use a table or
alias qualifier.

Supported inner limit:

- `LIMIT row_count` where `row_count` is an unsigned decimal integer literal in
  MyLite's existing `SELECT LIMIT` range.

`LIMIT 0` is meaningful and makes the inner subquery non-producing. Offset
forms, signed limits, expression limits, parameters, string/decimal/float/hex
limits, and `@@sql_select_limit` effects are deferred.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's supported subset, not MySQL's full grammar:

```lemon
predicate_atom(A) ::= EXISTS(E) LPAREN select_statement(S) RPAREN(R).

exists_select ::=
    SELECT exists_select_item_list
  | SELECT exists_select_item_list FROM DUAL
  | SELECT exists_select_item_list FROM table_name table_alias_opt
    where_clause_opt exists_limit_opt.

exists_select_item ::=
    STAR
  | INTEGER
  | TRUE
  | FALSE
  | NULL
  | STRING
  | qualified_identifier.

exists_correlated_predicate ::=
    qualified_identifier EQ qualified_identifier
  | qualified_identifier NULL_SAFE_EQUAL qualified_identifier.

exists_limit_opt ::= .
exists_limit_opt ::= LIMIT INTEGER.
```

`NOT EXISTS` uses the existing `NOT predicate` production and precedence.

## Semantics

Planning:

1. Resolve the outer selected/default schema and one descriptor-backed outer
   table using the existing `SELECT` source policy. Schema-qualified names use
   the named schema. Reserved `_mylite_*` schemas and table names are rejected
   before generated SQLite SQL exists.
2. Resolve outer selected columns, predicate columns outside the `EXISTS` atom,
   order columns, and limit values through existing descriptor-backed `SELECT`
   planning.
3. For each `EXISTS` atom, validate that the inner statement is one supported
   `SELECT` block. Reject inner joins, derived tables, CTEs, set operations,
   grouping, aggregates, locking clauses, select modifiers, `ORDER BY`,
   offset limits, nested subqueries, and arbitrary expressions.
4. For tableless and `DUAL` inner subqueries, validate the select list and plan
   a constant one-row subquery unless `LIMIT 0` is present.
5. For table-backed inner subqueries, resolve the inner table through
   descriptors and reject unsupported object kinds once non-base-table
   descriptors exist.
6. Validate every non-wildcard inner selected descriptor column. The value is
   not used, but MySQL still performs name resolution.
7. Resolve inner predicate names using inside-out scoping: inner source first,
   then the immediately containing outer source. Ambiguous or unsupported
   references receive deterministic diagnostics.
8. Admit correlated column comparisons only when one side resolves to the inner
   source, the other side resolves to the outer source, and both descriptors
   are compatible integer-family descriptors.

Execution:

1. Build one public result for the outer `SELECT`; the inner `EXISTS` does not
   create a public result.
2. Lower an admitted tableless or `DUAL` subquery to `EXISTS (SELECT 1)` or a
   false constant when an admitted `LIMIT 0` is present.
3. Lower an admitted table-backed subquery to a SQLite `EXISTS` predicate over
   the generated physical table name. Inner and outer physical table aliases
   are stable MyLite aliases such as `_mylite_s0` and `_mylite_s1`.
4. Bind all inner literal predicate values and limits with the same prepared
   statement that executes the outer query. Correlated column comparisons emit
   quoted physical column references, not literal interpolation.
5. For `EXISTS`, the predicate is true when SQLite finds at least one inner
   row. For `NOT EXISTS`, the existing `NOT` predicate wrapper negates the
   three-valued result; admitted `EXISTS` itself is never `NULL`.
6. Successful supported queries report no warnings. A following `ROW_COUNT()`
   follows the existing selected-row statement behavior.

Ordering and determinism:

- This slice does not support inner `ORDER BY`; existence is row-presence
  based, and `LIMIT row_count` only changes whether rows are allowed to exist.
- Duplicate inner rows and duplicate correlated key values do not have a
  visible order-dependent result.
- For unqualified inner predicate names that exist in both sources, MyLite must
  prefer the inner source, matching MySQL's inside-out resolution behavior.

## Generated SQLite Shape

No SQLite fork hook is needed. This is a MyLite wrapper/planner translation
over public SQLite prepared statements.

Outer single-table SELECTs that contain correlated subqueries receive a stable
physical alias so inner SQL can refer to the outer row:

```sql
SELECT "_mylite_s0"."outer_col"
FROM "_mylite_user_table_<outer_id>" AS "_mylite_s0"
WHERE EXISTS (
    SELECT 1
    FROM "_mylite_user_table_<inner_id>" AS "_mylite_s1"
    WHERE "_mylite_s1"."inner_col" = "_mylite_s0"."outer_col"
)
```

Literal predicates and limits use parameters:

```sql
WHERE EXISTS (
    SELECT 1
    FROM "_mylite_user_table_<inner_id>" AS "_mylite_s1"
    WHERE "_mylite_s1"."status" = ?1
    LIMIT ?2
)
```

Identifier quoting uses the existing generated identifier quoting helpers.
Logical names are never interpolated into generated physical SQL before
descriptor resolution. Reserved `_mylite_*` logical names are rejected before
physical SQL generation.

## Performance

The implementation must stay close to SQLite's physical execution path. It
must not materialize the inner table or outer result set in MyLite memory to
evaluate existence. SQLite evaluates the generated `EXISTS` predicate and can
use generated physical indexes when its planner chooses them. MyLite's work is
descriptor resolution, SQL generation, parameter binding, diagnostics, and
result shaping.

For correlated subqueries, SQLite may execute the inner probe per outer row.
That is acceptable for this baseline slice and matches the narrow semantics.
Future planner work may introduce descriptor-driven semi-join rewrites or
SQLite extension hooks only when measurement shows a real need.

## Unsupported

Deferred until later slices:

- `EXISTS` as a selected scalar expression, `HAVING` predicate, `JOIN`
  condition, `UPDATE` / `DELETE` predicate, `SET`, or `DO` operand;
- `IN (subquery)`, `ANY`, `SOME`, `ALL`, row subqueries, derived tables, CTEs,
  `TABLE` / `VALUES` subqueries, set operations, and nested subqueries;
- inner joins, aliases on more than one source, index hints, partitions,
  select modifiers, `DISTINCT`, grouping, aggregates, windows, locking clauses,
  `ORDER BY`, and offset limits inside the `EXISTS` subquery;
- arbitrary inner projection expressions, functions, parameters, user
  variables, arithmetic, casts, collations, and row constructors;
- correlated references outside the inner `WHERE` clause;
- correlated comparisons over string, decimal, approximate, binary, enum, set,
  JSON, temporal, or collation-sensitive descriptors;
- outer-table fallback for unqualified names when the inner table lacks that
  name, unless the implementation explicitly proves and tests that resolution
  path in a later slice;
- arbitrary SQLite SQL pass-through or SQLite fork patches.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- missing default schema: `1046 / 3D000`, `No database selected`;
- unknown explicit schema: existing `1049 / 42000` unknown-database
  diagnostic;
- unknown table: `1146 / 42S02`, `Table '<schema>.<table>' doesn't exist`;
- reserved `_mylite_*` schema or table names: existing MyLite reserved-name
  diagnostic before generated SQLite SQL;
- unsupported object kind: deterministic MyLite unsupported-object diagnostic;
- unknown selected inner column: `1054 / 42S22`, unknown column in field list;
- unknown inner predicate column: `1054 / 42S22`, unknown column in where
  clause;
- unknown outer correlated column: `1054 / 42S22`, unknown column in where
  clause;
- unsupported inner select list expression: deterministic unsupported
  diagnostic;
- unsupported inner source shape, clauses, nested subqueries, offset limits,
  parameters, functions, or expression predicates: deterministic unsupported
  diagnostic;
- unsupported correlated comparison descriptors: deterministic MyLite
  unsupported diagnostic;
- out-of-range inner limit: existing select-limit diagnostic;
- physical SQLite failure: existing SQLite execution diagnostic;
- allocation failure: existing `MYLITE_NOMEM` / out-of-memory diagnostic.

## Tests

Add MySQL-runtime expectation coverage for:

- uncorrelated `EXISTS` over nonempty and empty source tables;
- `NOT EXISTS` over nonempty and empty source tables;
- tableless and `DUAL` `EXISTS`;
- correlated `EXISTS` and `NOT EXISTS` using integer equality;
- correlated `<=>` with nullable integer values;
- inner literal predicates combined with correlated predicates through `AND`;
- inner unqualified name resolution preferring the inner source;
- `LIMIT 0`, `LIMIT 1`, and row counts larger than the matching set;
- schema-qualified outer and inner tables;
- missing default schema, unknown inner table, unknown selected column, unknown
  inner predicate column, and unknown outer correlated column;
- MySQL-accepted wider contexts intentionally deferred by MyLite, including
  selected `EXISTS` scalar expressions and DML `WHERE EXISTS`.

Add C runtime tests under `packages/libmylite/tests/`, preferably a new
`runtime_exists_subquery_predicates` binary:

- successful filtered SELECTs for all supported `EXISTS` and `NOT EXISTS`
  shapes;
- parser AST coverage for `EXISTS`, `NOT EXISTS`, source spans, nested
  parentheses, and unsupported wider forms that should parse for runtime
  rejection;
- descriptor authority for schema-qualified and unqualified outer/inner tables;
- inner select-list name validation even though values are ignored;
- correlated equality and null-safe equality over integer-family descriptors;
- `LIMIT 0` false behavior and accepted positive row-count limits;
- deterministic unsupported diagnostics for projection `EXISTS`, DML
  predicates, inner joins, derived tables, nested subqueries, functions,
  expression projections, `ORDER BY`, offset limits, parameters, and
  unsupported correlated descriptor families;
- no catalog mutation, descriptor generation change, or file-format preamble
  change for file-backed reads;
- independent file-backed handles with independent selected-schema state;
- zero-initialized cleanup of new planner/predicate objects;
- regression coverage for existing parser, select-where, scalar subquery,
  update scalar subquery, joined-select, runtime handle, catalog, VFS, and
  file-format tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-subqueries.md`,
`docs/compatibility/operators.md`, and
`docs/compatibility/sql-query-expressions.md` with limited wording. Do not
claim general subqueries, scalar `EXISTS` projection, DML `WHERE EXISTS`,
derived tables, nested subqueries, `IN` subqueries, quantified comparisons,
general correlation, expression predicates, or optimizer parity.
