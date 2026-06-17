# Baseline Insert Select Dual Scalar Where

## Status

This feature expands the current row-scalar `FROM DUAL` source path with a
small scalar predicate filter. It applies to the shared row-scalar planner used
by top-level `SELECT ... FROM DUAL`, row-scalar `INSERT ... SELECT ... FROM
DUAL`, and row-scalar `REPLACE ... SELECT ... FROM DUAL`.

The original implementation was deliberately narrow. A later source-free
expression slice extends the same `FROM DUAL` path to the documented
source-free row-scalar predicate envelope while preserving this slice's target
validation, zero-row, and `[NOT] EXISTS (subquery)` behavior. It still rejects
source ordering, source limits outside the top-level tableless scalar `SELECT`
path, aliases, CTEs, joins, and arbitrary SQLite pass-through for this source
class.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing `baseline-insert-select-dual-source` spec:
  `docs/specs/baseline-insert-select-dual-source/specs.md`
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_select_dual_scalar_where_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INSERT INTO target SELECT ... FROM DUAL WHERE 1` inserts one row.
- `WHERE 0` and `WHERE NULL` produce a successful zero-row source with
  `ROW_COUNT() == 0` and no warnings.
- Scalar-literal comparisons such as `1 = 1` and `NULL <=> NULL` filter the
  single `DUAL` row using normal MySQL truth semantics.
- Keyword `NOT`, `AND`, `OR`, and `XOR`, parentheses, and scalar-literal
  `IS [NOT] NULL` / `TRUE` / `FALSE` / `UNKNOWN` participate in the filter
  when the operands belong to the admitted scalar-literal predicate subset.
- Target/source column-count validation happens even when the scalar filter is
  false; `INSERT INTO t(a,b) SELECT 1 FROM DUAL WHERE 0` fails with
  `1136 / 21S01`.
- Source projection resolution happens before the filter result; an unknown
  select item still fails in the field-list context when the filter is false.
- Unknown identifiers in the scalar `WHERE` expression fail in the
  `where clause` context with MySQL's unknown-column diagnostic.
- Omitted target defaults and `NOT NULL` no-default validation are applied only
  when the source filter produces a row. A false filter skips omitted-column
  validation and reports zero affected rows.
- `SELECT ... FROM DUAL WHERE scalar_predicate` returns either one row or no
  rows. Successful SELECT statements leave `ROW_COUNT() == -1`.
- Later source-free expression slices implement no-source `SELECT ... WHERE`,
  source-free/`DUAL` `ORDER BY` and `LIMIT`, scalar-literal `BETWEEN`/`IN`,
  function-left predicates, and symbolic logical operators in this area.

## Scope

The implementation must add:

- row-scalar `FROM DUAL WHERE source_free_predicate` filtering for top-level
  row-scalar `SELECT`, row-scalar `INSERT ... SELECT`, and row-scalar
  `REPLACE ... SELECT`;
- scalar-literal predicate atoms over signed 64-bit decimal integer literals
  with optional unary sign, `TRUE`, `FALSE`, and `NULL`;
- scalar-literal truth tests, scalar-literal comparisons using `=`, `<=>`,
  `<>`, `!=`, `<`, `<=`, `>`, and `>=`;
- scalar-literal `IS [NOT] NULL`, `IS [NOT] TRUE`,
  `IS [NOT] FALSE`, and `IS [NOT] UNKNOWN`;
- scalar-literal `BETWEEN`/`NOT BETWEEN` and `IN`/`NOT IN` predicates admitted
  by the later tableless scalar `WHERE` expression slice;
- supported source-free row-scalar function truth, comparison, `IS`, range,
  and membership predicates admitted by the later tableless scalar `WHERE`
  expression slice;
- keyword `NOT`, `AND`, `OR`, `XOR`, symbolic `&&`/`||`, and parentheses over
  admitted atoms;
- preservation of the existing `WHERE [NOT] EXISTS (select_statement)` filter;
- MySQL-compatible affected-row and warning-count behavior for successful
  row-scalar DML source filters;
- descriptor-owned target validation, defaults, auto-increment, key checks,
  foreign-key checks, and physical writes through the existing insert/replace
  paths;
- focused parser/runtime tests and a MySQL-runtime expectation script.

## Non-Goals

This feature must not implement:

- no-source `SELECT ... WHERE` grammar in this original slice; later
  tableless expression slices cover it;
- row-scalar `DUAL` source `ORDER BY`, `LIMIT`, locking clauses, grouping,
  `HAVING`, `DISTINCT`, wildcard `DUAL`, CTEs, joins, `TABLE`, `VALUES`, or
  parenthesized query expressions;
- general expression predicates outside the documented source-free row-scalar
  subset, such as `LIKE`, `REGEXP`, scalar subqueries outside the existing
  `[NOT] EXISTS` / scalar-subquery `IS NULL` forms, parameters, row
  constructors, or column references in the tableless `DUAL` filter;
- broader target conversion, warning demotion, triggers, generated columns,
  privileges, or protocol metadata changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` still owns public call
  validation, dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()` state, and non-row result finalization.
- Lexer/parser/AST already admit `SELECT ... FROM DUAL WHERE predicate`; this
  feature does not add new tokens or AST node kinds.
- The row-scalar planner owns deciding whether a tableless/`DUAL` predicate is
  the existing `EXISTS` filter or the new scalar-literal predicate filter.
- The shared predicate planner owns admitted scalar-literal truth semantics and
  generated SQL for `NOT`, `AND`, `OR`, `XOR`, comparisons, and `IS` tests.
- Existing insert/replace execution owns descriptor target mapping, conversion,
  default materialization, generated auto-increment values, key checks,
  foreign-key checks, and physical row insertion/replacement.
- The catalog remains the metadata authority. This feature must not mutate
  catalog rows except through existing auto-increment advancement when a row is
  actually inserted or replaced.
- SQLite executes the generated one-row `SELECT` with a `WHERE` predicate and
  bound scalar parameters. MyLite builds the SQL from planned expressions and
  never treats SQLite schema text as metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature writes only through the existing physical row storage paths.

## Supported SQL Grammar

Supported additions within the existing row-scalar source grammar:

```sql
SELECT select_item[, select_item ...]
FROM DUAL
[WHERE dual_scalar_predicate]

INSERT [INTO] table_name [(column_name[, column_name] ...)]
SELECT select_item[, select_item ...]
FROM DUAL
[WHERE dual_scalar_predicate]

REPLACE [INTO] table_name [(column_name[, column_name] ...)]
SELECT select_item[, select_item ...]
FROM DUAL
[WHERE dual_scalar_predicate]
```

`dual_scalar_predicate` is limited to:

```sql
scalar_literal
scalar_literal comparison_operator scalar_literal
scalar_literal BETWEEN predicate_value AND predicate_value
scalar_literal NOT BETWEEN predicate_value AND predicate_value
scalar_literal IN (predicate_value[, predicate_value ...])
scalar_literal NOT IN (predicate_value[, predicate_value ...])
scalar_literal IS [NOT] NULL
scalar_literal IS [NOT] TRUE
scalar_literal IS [NOT] FALSE
scalar_literal IS [NOT] UNKNOWN
source_free_row_scalar_function
source_free_row_scalar_function comparison_operator predicate_value
source_free_row_scalar_function IS [NOT] NULL
source_free_row_scalar_function BETWEEN predicate_value AND predicate_value
source_free_row_scalar_function NOT BETWEEN predicate_value AND predicate_value
source_free_row_scalar_function IN (predicate_value[, predicate_value ...])
source_free_row_scalar_function NOT IN (predicate_value[, predicate_value ...])
NOT dual_scalar_predicate
dual_scalar_predicate AND dual_scalar_predicate
dual_scalar_predicate && dual_scalar_predicate
dual_scalar_predicate OR dual_scalar_predicate
dual_scalar_predicate || dual_scalar_predicate
dual_scalar_predicate XOR dual_scalar_predicate
(dual_scalar_predicate)
EXISTS (select_statement)
NOT EXISTS (select_statement)
```

`scalar_literal` is a supported signed decimal integer literal, `TRUE`,
`FALSE`, or `NULL`. `predicate_value` is the current predicate value envelope
accepted by the row-scalar predicate planner, and the row-scalar function
predicate forms are limited to source-free planned expressions.

### MyLite Lemon-Syntax Snippet

No new parser production is required. The existing independently authored
MyLite production remains the syntax envelope for this feature:

```lemon
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
    FROM(F) DUAL(D) where_clause_opt(W) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D),
        W, NULL, NULL, NULL, NULL, K);
}
```

Runtime planning rejects unsupported `WHERE` predicate nodes and unsupported
optional clauses with deterministic MyLite diagnostics unless an existing
MySQL-compatible diagnostic applies.

## Semantics

For a `DUAL` row-scalar source, MyLite evaluates a single synthetic source row.
The optional scalar filter decides whether that row exists for the caller:

- true filter result: the single source row is projected and consumed;
- false or unknown filter result: the source is empty;
- successful false/unknown filters produce zero warnings and no rows;
- for DML sources, zero-row filters report `affected_rows == 0` and do not
  validate omitted target defaults that would matter only for a produced row.

Source projection planning still happens before the filter is executed. This
preserves MySQL behavior for field-list errors and target/source column-count
errors even when the filter would be false at runtime.

`WHERE [NOT] EXISTS (...)` continues to use the existing uncorrelated EXISTS
subquery planner. Scalar-literal predicates do not admit subqueries in this
slice.

## SQLite Handling

For a scalar-literal filter, MyLite generates a standard SQLite `SELECT` with
no `FROM` clause and a `WHERE` predicate built from bound scalar values:

```sql
SELECT ?1, ?2 WHERE (?3 = ?4)
```

For `EXISTS`, the existing generated SQL shape remains:

```sql
SELECT ?1 WHERE EXISTS (SELECT 1 FROM "physical_inner" AS "_mylite_s1" WHERE ...)
```

No SQLite fork patch is required. The work stays in MyLite planning and SQL
translation over public SQLite prepared statements.

## Diagnostics

Diagnostics must cover:

- unsupported `DUAL` source optional clauses such as `ORDER BY` and `LIMIT`;
- unsupported wildcard `DUAL` projections through the existing `1096 / HY000`
  path;
- target/source column-count mismatch with `1136 / 21S01`, including false
  filters;
- unknown select-list identifiers in the field-list context, including false
  filters;
- unknown scalar-filter identifiers in the `where clause` context;
- omitted `NOT NULL` no-default targets when the scalar filter produces a row;
- selected `NULL` into `NOT NULL` targets when the scalar filter produces a
  row;
- unsupported scalar filter atoms such as functions, `BETWEEN`, `IN`, `LIKE`,
  `REGEXP`, parameters, variables, and subqueries outside the existing
  `[NOT] EXISTS` path;
- physical SQLite failures, allocation failures, and public API misuse through
  existing runtime policies.

## Test Plan

- MySQL expectation script for true, false, and unknown scalar filters,
  scalar-literal comparisons, null-safe equality, logical composition, `IS`
  predicates, column-count validation on false filters, unknown select-list and
  where-clause names, omitted-column validation skipped on false filters and
  applied on true filters, top-level `SELECT ... FROM DUAL WHERE`, row-scalar
  `INSERT ... SELECT`, row-scalar `REPLACE ... SELECT`, and unsupported
  `BETWEEN`/function predicates in MyLite.
- Parser tests proving scalar `WHERE` remains present in the existing AST shape
  for row-scalar `INSERT ... SELECT ... FROM DUAL`.
- Runtime tests for:
  - successful one-row and zero-row row-scalar insert filters;
  - top-level `SELECT ... FROM DUAL WHERE` one-row and zero-row behavior;
  - `REPLACE ... SELECT ... FROM DUAL WHERE` behavior on no-key and key-bearing
    targets where existing replacement semantics already apply;
  - source/target count mismatch despite a false filter;
  - unknown field-list and where-clause names;
  - omitted required target validation only for produced rows;
  - affected rows, warning counts, and no row result for successful DML;
  - close/reopen persistence and preamble preservation through existing
    row-scalar DML coverage.
