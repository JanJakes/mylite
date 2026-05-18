# Baseline IN Subquery Predicates

## Summary

This phase adds a narrow column-subquery predicate slice:

```sql
SELECT select_list
FROM outer_table [outer_alias]
WHERE outer_column [NOT] IN (
    SELECT inner_column
    FROM inner_table [inner_alias]
    [WHERE inner_predicate]
)
```

The goal is to cover common membership filters without claiming general
subquery support. The slice is descriptor-driven, keeps MyLite catalog
descriptors authoritative, and lowers the supported predicate to generated
SQLite SQL over stable physical table names.

This is not full MySQL `IN` subquery support. The supported outer statement is
a single-table descriptor-backed `SELECT` filter. The supported inner query is
one descriptor-backed persistent or visible session temporary base table with
one explicit descriptor column, optional alias, and optional existing
descriptor-backed `WHERE` predicate. Correlated inner predicates reuse the
current `EXISTS` subquery correlation envelope.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-where-in-predicates/specs.md`
  - `docs/specs/baseline-exists-subquery-predicates/specs.md`
  - `docs/specs/baseline-scalar-subquery-projection/specs.md`
  - `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`
- Official MySQL 8.4 Reference Manual:
  - subqueries: <https://dev.mysql.com/doc/refman/8.4/en/subqueries.html>
  - subqueries with `ANY`, `IN`, or `SOME`:
    <https://dev.mysql.com/doc/refman/8.4/en/any-in-some-subqueries.html>
  - correlated subqueries:
    <https://dev.mysql.com/doc/refman/8.4/en/correlated-subqueries.html>
  - subquery errors:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-errors.html>
  - subquery restrictions:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-restrictions.html>
  - subquery optimization:
    <https://dev.mysql.com/doc/refman/8.4/en/subquery-optimization.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_in_subquery_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `expr IN (subquery)` behaves as `expr = ANY (subquery)`.
- `expr NOT IN (subquery)` behaves as `expr <> ALL (subquery)`, not as
  `expr <> ANY (subquery)`.
- A matching non-`NULL` value makes `IN` true.
- An empty subquery makes `IN` false and `NOT IN` true.
- A `NULL` outer value does not match ordinary `IN` values.
- If no non-`NULL` value matches and the subquery produces at least one `NULL`,
  both `IN` and `NOT IN` evaluate to unknown and filter out the row in `WHERE`.
- Duplicate subquery values do not change the visible result.
- Default `utf8mb4_0900_ai_ci` string membership is case-insensitive for the
  ASCII subset verified by this phase.
- Inner `WHERE` predicates filter the subquery before membership is evaluated.
- Correlated inner predicates resolve names inside out. An unqualified name in
  the inner query resolves to the inner source when possible.
- A schema-qualified outer or inner source works like the corresponding
  unqualified source after normal schema resolution.
- MySQL accepts `ORDER BY` inside an `IN` subquery when no `LIMIT` is present,
  but the order has no visible effect for membership.
- MySQL rejects `LIMIT` inside `IN`, `ALL`, `ANY`, or `SOME` subqueries with
  `1235 / 42000` and the message
  `This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'`.
- A subquery returning more than one column fails with `1241 / 21000`,
  `Operand should contain 1 column(s)`.
- Unknown selected inner columns fail in the field list. Unknown inner
  predicate columns and unknown correlated outer references fail in the where
  clause. Unknown inner tables fail with the normal unknown-table diagnostic.
- MySQL also accepts tableless subqueries, `DUAL` subqueries, wildcard
  single-column subqueries, row-constructor `IN` subqueries, quantified
  comparisons, and `IN` subqueries in wider expression contexts. Those are
  deferred by this phase.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation,
  result handles, diagnostics, and public misuse behavior.
- Statement context: owns the outer statement boundary, diagnostics reset,
  warning count, row count, found-rows state, and transaction completion. Inner
  `IN` subqueries do not publish independent result objects or statement
  completion state.
- Lexer/parser/AST: admits `qualified_identifier IN (select_statement)` as a
  predicate atom. Existing keyword `NOT` composition owns `NOT IN`. The parser
  stores source spans and AST children only; it does not bind descriptors.
- Analyzer/planner: resolves the outer source table, outer membership column,
  inner source table, inner selected column, inner predicate columns,
  correlated outer references, and unsupported shapes from MyLite descriptors.
- Catalog: remains authoritative for logical schemas, object kind, table
  identity, physical table name, columns, descriptor metadata, and visible
  temporary-table shadowing. Supported `IN` subqueries read descriptors only
  and must not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- Result builder: successful supported outer `SELECT` statements return the
  existing row-result shape for the outer query only. `IN` subqueries do not
  add projection metadata.
- SQLite physical row storage: executes the MyLite-generated physical
  membership subquery over generated MyLite tables. MyLite quotes every
  generated identifier and binds predicate literals.
- Storage/VFS/file format: unchanged. Supported `IN` subqueries are read-only
  and must not touch the `.mylite` preamble or shifted SQLite payload.

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
`in_subquery_predicate` atom, composed through the existing `NOT`, `AND`,
`XOR`, `OR`, and parenthesized predicate rules.

Supported `IN` subquery atom:

```sql
in_subquery_predicate:
    qualified_identifier IN ( in_subquery )
```

Supported `NOT IN` form:

```sql
not_in_subquery_predicate:
    NOT in_subquery_predicate
```

The parser may construct `NOT IN` by wrapping the `IN` predicate with the
existing `NOT` predicate node.

Supported inner subquery:

```sql
in_subquery:
    SELECT qualified_identifier
    FROM table_name [table_alias]
    [WHERE inner_predicate]
```

The selected inner item must be one explicit descriptor column. The selected
value participates in membership semantics, so MyLite resolves and lowers it
through descriptor metadata. `SELECT *`, tableless subqueries, `DUAL`
subqueries, expression items, aggregate items, scalar subquery items, and
multi-column projection lists are deferred.

Supported inner predicate forms:

- the current baseline descriptor-backed predicate subset over inner source
  columns and literal right operands;
- existing logical composition with `NOT`, `AND`, `XOR`, `OR`, and
  parentheses;
- simple correlated comparison atoms currently admitted by the `EXISTS`
  subquery slice:
  - `inner_integer_column = outer_integer_column`;
  - `outer_integer_column = inner_integer_column`;
  - `inner_integer_column <=> outer_integer_column`;
  - `outer_integer_column <=> inner_integer_column`.

Supported membership value families:

- compatible MyLite integer-family descriptors, including `TINYINT`,
  `SMALLINT`, `MEDIUMINT`, `INT` / `INTEGER`, `BIGINT`, aliases, and their
  currently supported unsigned physical ranges;
- ASCII nonbinary string descriptors in the current `CHAR`, `VARCHAR`, and
  baseline `TEXT` family subset, using MyLite's registered
  `utf8mb4_0900_ai_ci` collation for the verified ASCII comparison behavior.

The outer membership column and inner selected column must be in the same
supported family. MyLite does not perform implicit cross-family conversion in
this phase. `NULL` values in either source keep MySQL three-valued membership
semantics.

Unsupported inner clauses for this phase:

- `LIMIT`, including `LIMIT 0`;
- `ORDER BY`, even though MySQL accepts it without `LIMIT`;
- `DISTINCT`, grouping, aggregates, joins, locking clauses, select modifiers,
  set operations, nested subqueries, CTEs, `TABLE`, and `VALUES`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's supported subset, not MySQL's full grammar:

```lemon
predicate_atom(A) ::= qualified_identifier(C) IN(I) LPAREN select_statement(S) RPAREN(R).
predicate_atom(A) ::= qualified_identifier(C) NOT(N) IN(I) LPAREN select_statement(S) RPAREN(R).

in_subquery ::=
    SELECT qualified_identifier
    FROM table_name table_alias_opt
    where_clause_opt.
```

The concrete parser may reuse the existing `MYLITE_SQL_AST_IN_PREDICATE` node
with child 0 as the outer descriptor column and child 1 as the inner
`SELECT_STATEMENT`.

## Semantics

Planning:

1. Resolve the outer selected/default schema and one descriptor-backed outer
   table using existing `SELECT` source policy. Schema-qualified names use the
   named schema. Reserved `_mylite_*` schemas and table names are rejected
   before generated SQLite SQL exists.
2. Resolve the outer membership column through descriptors.
3. Validate that the inner statement is one supported plain `SELECT` block with
   one descriptor table source, one explicit selected descriptor column, no
   unsupported clauses, and optional supported `WHERE`.
4. Resolve the inner source through descriptors and reject unsupported object
   kinds once non-base-table descriptors exist.
5. Resolve the inner selected descriptor column through the inner source. The
   selected column must be type-compatible with the outer membership column for
   this slice.
6. Resolve inner predicate names using inside-out scoping: inner source first,
   then the immediately containing outer source. Ambiguous or unsupported
   references receive deterministic diagnostics.
7. Admit correlated column comparisons only when one side resolves to the inner
   source, the other side resolves to the outer source, and both descriptors
   are compatible integer-family descriptors.
8. Reject `LIMIT` inside the inner subquery with MySQL-compatible
   `1235 / 42000` before generating SQLite SQL. Reject `ORDER BY` and other
   unsupported inner shapes with deterministic MyLite diagnostics.

Execution:

1. Build one public result for the outer `SELECT`; the inner `IN` subquery does
   not create a public result.
2. Lower the admitted table-backed subquery to a SQLite membership subquery
   over the generated physical table name. Inner and outer physical table
   aliases are stable MyLite aliases such as `_mylite_s0` and `_mylite_s1`.
3. Bind all inner literal predicate values with the same prepared statement
   that executes the outer query. Correlated column comparisons emit quoted
   physical column references, not literal interpolation.
4. Let SQLite evaluate membership three-valued logic for `NULL`, empty
   subqueries, and duplicate inner values. MyLite controls descriptor-compatible
   value SQL and collation so the admitted integer and ASCII string results
   match the verified MySQL slice.
5. `NOT IN` uses the existing `NOT` predicate wrapper, preserving MySQL's
   unknown-result filtering when the inner subquery contains `NULL` and no
   match exists.
6. Successful supported queries report no warnings. A following `ROW_COUNT()`
   follows the existing selected-row statement behavior.

Ordering and determinism:

- This slice rejects inner `ORDER BY`; membership has no visible row-order
  result, and MyLite should not claim a broader ordered-subquery surface until
  it is specified.
- Duplicate inner rows do not have a visible order-dependent result.
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
WHERE "_mylite_s0"."outer_col" IN (
    SELECT "_mylite_s1"."inner_col"
    FROM "_mylite_user_table_<inner_id>" AS "_mylite_s1"
    WHERE "_mylite_s1"."group_id" = "_mylite_s0"."group_id"
)
```

Literal predicates use parameters:

```sql
WHERE "_mylite_s0"."outer_col" IN (
    SELECT "_mylite_s1"."inner_col"
    FROM "_mylite_user_table_<inner_id>" AS "_mylite_s1"
    WHERE "_mylite_s1"."status" = ?1
)
```

String-family membership uses descriptor value SQL with MyLite's registered
ASCII `utf8mb4_0900_ai_ci` collation on the compared values.

Identifier quoting uses the existing generated identifier quoting helpers.
Logical names are never interpolated into generated physical SQL before
descriptor resolution. Reserved `_mylite_*` logical names are rejected before
physical SQL generation.

## Performance

The implementation must stay close to SQLite's physical execution path. It
must not materialize the inner table or outer result set in MyLite memory to
evaluate membership. SQLite evaluates the generated `IN (SELECT ...)`
predicate and can use generated physical indexes when its planner chooses them.
MyLite's work is descriptor resolution, SQL generation, parameter binding,
diagnostics, and result shaping.

For correlated subqueries, SQLite may execute the inner probe per outer row.
That is acceptable for this baseline slice. Future planner work may introduce
descriptor-driven semi-join rewrites or SQLite extension hooks only when
measurement shows a real need.

## Unsupported

Deferred until later slices:

- `IN` subqueries in selected scalar expressions, `HAVING`, join conditions,
  `UPDATE` / `DELETE` predicates, `CHECK` constraints, `SET`, or `DO`;
- tableless and `DUAL` `IN` subqueries;
- wildcard inner projection, even when the source has one column;
- row-constructor `IN` subqueries;
- quantified `ANY`, `SOME`, and `ALL` comparisons;
- inner joins, aliases on more than one source, index hints, partitions,
  select modifiers, `DISTINCT`, grouping, aggregates, windows, locking clauses,
  `ORDER BY`, `LIMIT`, and offset limits inside the `IN` subquery;
- arbitrary inner projection expressions, functions, parameters, user
  variables, arithmetic, casts, explicit collations, and nested subqueries;
- correlated references outside the inner `WHERE` clause;
- membership over decimal, approximate, binary string, `BIT`, `ENUM`, `SET`,
  `JSON`, temporal, or non-ASCII/full-Unicode collation-sensitive descriptors;
- implicit cross-family conversion;
- arbitrary SQLite SQL pass-through or SQLite fork patches.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- missing default schema: `1046 / 3D000`, `No database selected`;
- unknown explicit schema: existing unknown-database diagnostic;
- unknown table: `1146 / 42S02`, `Table '<schema>.<table>' doesn't exist`;
- reserved `_mylite_*` schema or table names: existing MyLite reserved-name
  diagnostic before generated SQLite SQL;
- unsupported object kind: deterministic MyLite unsupported-object diagnostic;
- unknown selected inner column: `1054 / 42S22`, unknown column in field list;
- unknown inner predicate column: `1054 / 42S22`, unknown column in where
  clause;
- unknown outer correlated column: `1054 / 42S22`, unknown column in where
  clause;
- multiple inner selected columns: `1241 / 21000`,
  `Operand should contain 1 column(s)`;
- inner `LIMIT`: `1235 / 42000`,
  `This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'`;
- unsupported inner select list expression, source shape, clauses, nested
  subqueries, parameters, functions, or expression predicates: deterministic
  unsupported diagnostic;
- unsupported membership descriptors or implicit conversion:
  deterministic MyLite unsupported diagnostic;
- physical SQLite failure: existing SQLite execution diagnostic;
- allocation failure: existing `MYLITE_NOMEM` / out-of-memory diagnostic.

## Tests

Add MySQL-runtime expectation coverage for:

- uncorrelated integer `IN` over matching, duplicate, and `NULL` inner values;
- integer `NOT IN` with a `NULL` inner value, without `NULL`, and over an empty
  subquery;
- outer `NULL` values with `IN` and `NOT IN`;
- ASCII string membership under default case-insensitive collation;
- inner `WHERE` predicates;
- correlated integer equality and null-safe equality;
- inner unqualified name resolution preferring the inner source;
- schema-qualified outer and inner tables;
- unknown inner table, unknown selected column, unknown inner predicate column,
  unknown outer correlated column, multi-column subquery, and MySQL's `LIMIT`
  rejection;
- MySQL-accepted wider contexts intentionally deferred by MyLite, including
  tableless subqueries, wildcard single-column subqueries, and inner `ORDER BY`
  without `LIMIT`.

Add C runtime tests under `packages/libmylite/tests/`, preferably a new
`runtime_in_subquery_predicates` binary:

- successful filtered SELECTs for all supported `IN` and `NOT IN` shapes;
- parser AST coverage for `IN (SELECT ...)`, `NOT IN (SELECT ...)`, source
  spans, and nested parentheses;
- descriptor authority for schema-qualified and unqualified outer/inner
  tables, visible temporary sources if admitted by implementation, and
  selected-schema independence;
- integer and ASCII string membership, including `NULL` handling;
- correlated equality and null-safe equality over integer-family descriptors;
- deterministic unsupported diagnostics for tableless subqueries, `DUAL`,
  wildcard projection, inner joins, derived tables, nested subqueries,
  functions, expression projections, `ORDER BY`, `LIMIT`, offset limits,
  parameters, unsupported descriptor families, and wider predicate contexts;
- no catalog mutation, descriptor generation change, or file-format preamble
  change for file-backed reads;
- independent file-backed handles with independent selected-schema state;
- zero-initialized cleanup of new planner/predicate objects;
- regression coverage for existing parser, select-where, where-in, exists
  subquery, joined-select, runtime handle, catalog, VFS, and file-format tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-subqueries.md`,
`docs/compatibility/operators.md`, and
`docs/compatibility/sql-query-expressions.md` with limited wording once the
implementation is complete. Do not claim general subqueries, scalar `IN`
projection, DML `WHERE IN (subquery)`, derived tables, nested subqueries, row
subqueries, quantified comparisons, general correlation, expression predicates,
or optimizer parity.
