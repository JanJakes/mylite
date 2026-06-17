# Baseline WHERE IS Boolean Predicates

## Status

This phase expands MyLite's descriptor-backed `WHERE` predicate subset with
truth tests against integer descriptor columns:

```sql
column_name IS TRUE
column_name IS FALSE
column_name IS UNKNOWN
column_name IS NOT TRUE
column_name IS NOT FALSE
column_name IS NOT UNKNOWN
```

The implementation is intentionally narrow. It applies only to descriptor
columns already supported by the single-table predicate planner. It does not add
general expression truth evaluation, bare `WHERE TRUE`, literal-left truth
tests, or string/temporal boolean conversion.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline WHERE AND predicates:
  `docs/specs/baseline-where-and-predicates/specs.md`
- Baseline WHERE OR predicates:
  `docs/specs/baseline-where-or-predicates/specs.md`
- Baseline WHERE NOT predicates:
  `docs/specs/baseline-where-not-predicates/specs.md`
- Baseline WHERE BETWEEN predicates:
  `docs/specs/baseline-where-between-predicates/specs.md`
- Baseline WHERE IN predicates:
  `docs/specs/baseline-where-in-predicates/specs.md`
- Baseline delete lifecycle:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, comparison functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- MySQL 8.4 Reference Manual, expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, operator precedence:
  https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

Supported `WHERE` predicate additions:

```sql
column_reference IS TRUE
column_reference IS FALSE
column_reference IS UNKNOWN
column_reference IS NOT TRUE
column_reference IS NOT FALSE
column_reference IS NOT UNKNOWN
NOT column_reference IS TRUE
NOT column_reference IS FALSE
NOT column_reference IS UNKNOWN
```

`column_reference` is the existing descriptor-backed predicate column reference:

- `SELECT` source predicates may use unqualified names, the source table name,
  the selected schema plus table name, or a supported source table alias.
- single-table `DELETE` and `UPDATE` predicate columns share the qualified descriptor-column policy documented in [baseline qualified predicate columns](../baseline-qualified-predicate-columns/specs.md).

The predicate is available wherever the shared descriptor-backed `WHERE`
planner is already used:

- table `SELECT`;
- aggregate source filters;
- grouped aggregate source filters;
- `SELECT` sources reused by `INSERT ... SELECT`, `REPLACE ... SELECT`, and
  `CREATE TABLE ... SELECT`;
- single-table `DELETE`;
- single-table `UPDATE`.

## Out Of Scope

This phase does not add:

- bare boolean predicates such as `WHERE TRUE`, `WHERE FALSE`, or
  `WHERE column_name`;
- literal-left truth tests such as `1 IS TRUE`, even though MySQL accepts them;
- expression-left truth tests such as `column_name + 1 IS TRUE`;
- string, decimal, float, hex, bit, temporal, JSON, parameter, variable,
  function, cast, collation, subquery, row-constructor, or arithmetic operands;
- `IS TRUE` / `IS FALSE` / `IS UNKNOWN` in `HAVING`;
- symbolic `!`;
- `XOR` or broader boolean expression support;
- aliases for `DELETE` or `UPDATE`;
- table-qualified `DELETE` or `UPDATE` predicate columns;
- joins, multi-table DML, full MySQL resolver rules, optimizer hints, locks,
  privileges, indexes, constraints, triggers, cascades, or SQLite fork patches.

MySQL accepts many broader forms. MyLite must reject them deterministically until
the expression planner owns their semantics.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted surface:

- `expr IS TRUE` is true for non-`NULL` numeric values that are not zero.
- `expr IS FALSE` is true for non-`NULL` numeric zero.
- `expr IS UNKNOWN` is true for SQL `NULL`.
- `expr IS NOT TRUE` is true for SQL `NULL` or numeric zero.
- `expr IS NOT FALSE` is true for SQL `NULL` or nonzero numeric values.
- `expr IS NOT UNKNOWN` is true for non-`NULL` values.
- For this descriptor integer subset, `IS UNKNOWN` is equivalent to `IS NULL`,
  and `IS NOT UNKNOWN` is equivalent to `IS NOT NULL`.
- `NOT expr IS TRUE`, `NOT expr IS FALSE`, and `NOT expr IS UNKNOWN` behave as
  keyword `NOT` applied to the corresponding `IS` predicate.
- `IS` binds at the comparison-operator precedence level, above keyword `NOT`,
  `AND` / `&&`, and `OR` / `||`.
- Supported truth-test predicates record no warnings.
- `WHERE` filtering happens before grouping, aggregate calculation, DML row
  mutation, ordering, and limiting.
- `UPDATE` affected rows remain changed-row counts; `DELETE` affected rows
  remain deleted-row counts.

The feature's MySQL expectation script is:

```text
packages/libmylite/tests/mysql_baseline_where_is_boolean_predicates_expectations.sh
```

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public call validation,
  result-handle ownership, public misuse behavior, and cleanup on failure.
- Statement context owns the statement boundary: diagnostics reset, warning
  count, affected rows, and backend execution status.
- Lexer/parser/AST own syntax admission and source spans. They represent
  descriptor truth-test predicates independently of runtime, catalog, storage,
  and SQLite.
- Analyzer/planner code resolves the tested column against MyLite catalog
  descriptors, rejects unsupported operands, and builds the physical SQLite
  predicate from descriptor metadata.
- The catalog module remains the metadata authority. Truth-test predicates must
  not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- The result builder owns descriptor-driven row results and non-query result
  conventions. Truth tests add no public result metadata surface.
- SQLite owns physical row storage and executes the generated
  scan/filter/update/delete. SQLite schema text and `PRAGMA` output are not
  metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Truth-test predicates must not write through byte range `[0, 4096)` except
  through existing DML payload behavior.

## Supported Grammar

The feature extends only the reusable `predicate` grammar used by `WHERE`.

```sql
where_clause:
    WHERE predicate

predicate:
    predicate OR predicate
  | predicate || predicate
  | predicate AND predicate
  | predicate && predicate
  | NOT predicate
  | predicate_atom
  | ( predicate )

predicate_atom:
    column_name comparison_operator predicate_value
  | column_name IS NULL
  | column_name IS NOT NULL
  | column_name IS TRUE
  | column_name IS FALSE
  | column_name IS UNKNOWN
  | column_name IS NOT TRUE
  | column_name IS NOT FALSE
  | column_name IS NOT UNKNOWN
  | column_name BETWEEN predicate_value AND predicate_value
  | column_name NOT BETWEEN predicate_value AND predicate_value
  | column_name IN ( in_value_list )
  | column_name NOT IN ( in_value_list )
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
predicate_atom(A) ::= qualified_identifier(C) IS(I) predicate_truth_value(V). {
    A = mylite_sql_parser_make_is_boolean_predicate(state, C, I, V);
}

predicate_atom(A) ::= qualified_identifier(C) IS(I) NOT(N) predicate_truth_value(V). {
    A = mylite_sql_parser_make_is_boolean_predicate(state, C, I, N, V);
}

predicate_truth_value(A) ::= TRUE(T).
predicate_truth_value(A) ::= FALSE(T).
predicate_truth_value(A) ::= UNKNOWN(T).
```

The actual helper signature may differ, but the AST must preserve:

- the tested descriptor column reference;
- the target truth value: `TRUE`, `FALSE`, or `UNKNOWN`;
- whether `NOT` belongs to the `IS NOT` predicate rather than the outer keyword
  `NOT`;
- source spans for diagnostics and tests.

`UNKNOWN` should be tokenized as a MyLite parser keyword for this grammar while
remaining a nonreserved identifier outside the `IS UNKNOWN` predicate shape and
outside the full scalar literal surface.

## Semantics

For the current integer/`NULL` descriptor storage envelope:

| Predicate | Row matches when |
| --- | --- |
| `col IS TRUE` | stored value is non-`NULL` and not zero |
| `col IS FALSE` | stored value is non-`NULL` and zero |
| `col IS UNKNOWN` | stored value is `NULL` |
| `col IS NOT TRUE` | stored value is `NULL` or zero |
| `col IS NOT FALSE` | stored value is `NULL` or nonzero |
| `col IS NOT UNKNOWN` | stored value is non-`NULL` |

`TRUE` and `FALSE` are truth-test targets here, not independent expression
operands. This phase does not change the existing rule that `WHERE TRUE`,
`WHERE FALSE`, and bare descriptor-column truth tests remain unsupported.

`UNKNOWN` means SQL `NULL` for this predicate surface. It is not a new stored
literal or projected scalar value.

## Name Resolution

Truth-test predicates use the existing descriptor-backed predicate resolver:

- missing default schema for unqualified table targets reports the existing
  MySQL-compatible no-database diagnostic;
- schema-qualified table targets resolve against the named schema;
- unknown schemas and tables report the existing MySQL-compatible diagnostics;
- reserved `_mylite_*` schema/table names are rejected before SQLite SQL is
  generated;
- unsupported object kinds must be rejected once non-base-table descriptors
  exist;
- tested columns are resolved from MyLite descriptors, not SQLite metadata;
- unknown tested columns report the same deterministic diagnostic as other
  `WHERE` predicate columns;
- current descriptor catalog identifier matching keeps the existing
  case-sensitivity and collation behavior.

## Generated SQLite Shape

The planner must generate SQLite SQL only from descriptor metadata and stable
physical table/column names. Every generated identifier must be quoted. No user
SQL literal is interpolated.

Truth-test predicates require no bound value parameters. The generated term for
one quoted physical column can use these standard SQLite shapes:

| Predicate | SQLite predicate shape |
| --- | --- |
| `col IS TRUE` | `("col" IS NOT NULL AND "col" <> 0)` |
| `col IS FALSE` | `("col" IS NOT NULL AND "col" = 0)` |
| `col IS UNKNOWN` | `"col" IS NULL` |
| `col IS NOT TRUE` | `("col" IS NULL OR "col" = 0)` |
| `col IS NOT FALSE` | `("col" IS NULL OR "col" <> 0)` |
| `col IS NOT UNKNOWN` | `"col" IS NOT NULL` |

These shapes keep filtering in SQLite and avoid row materialization in MyLite.
They are valid for existing descriptor integer storage where values are stored
as SQLite integers or `NULL`.

The feature must not rely on optional SQLite syntax extensions or add SQLite
fork patches.

## Result And Diagnostics

Successful truth-test predicates:

- use existing row-result metadata for `SELECT`;
- use existing non-row result conventions for successful `DELETE` and `UPDATE`;
- preserve MySQL-compatible affected-row behavior for the already supported
  DML subset;
- report `warning_count == 0`.

Diagnostics required for this phase:

- syntax errors for unsupported grammar such as literal-left truth tests,
  expression-left truth tests, missing truth values, or invalid truth targets;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` target names;
- unsupported object kind once applicable;
- unknown tested column;
- unsupported table-qualified `DELETE`/`UPDATE` predicate columns;
- unsupported `HAVING` truth predicates;
- unsupported symbolic `!`;
- unsupported broader expression operands;
- physical SQLite prepare/step/finalize failures;
- allocation failures.

Where MyLite already has deterministic diagnostics for an unsupported shape,
this phase may preserve those diagnostics even when MySQL accepts the broader
form. The compatibility documentation must make that narrower scope explicit.

## Test Plan

Add MySQL-runtime-verified expectations covering:

- `IS TRUE`, `IS FALSE`, and `IS UNKNOWN` over nonzero, zero, and `NULL`
  descriptor values;
- `IS NOT TRUE`, `IS NOT FALSE`, and `IS NOT UNKNOWN`;
- prefix keyword `NOT` applied to `IS` predicates;
- precedence with `NOT`, `AND`, `&&`, `OR`, `||`, and parentheses;
- `INT`, `INTEGER`, `BIGINT`, `INT UNSIGNED`, and `BIGINT UNSIGNED` columns
  within MyLite's current signed-64 physical range;
- source-qualified and alias-qualified `SELECT` predicate columns;
- aggregate source filter reuse, grouped aggregate source reuse,
  `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT`;
- `UPDATE` and `DELETE` with and without `ORDER BY` / `LIMIT` where applicable;
- changed-row affected counts and warning counts;
- reopen persistence and independent file-backed handles;
- `.mylite` preamble preservation;
- unknown tested columns;
- unsupported literal-left, expression-left, parameter, function, subquery, row
  constructor, table-qualified DML predicate, invalid truth target, and symbolic
  `!` forms;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog,
  DDL/DML lifecycle, predicate, aggregate, and registration tests still pass.

Prefer extending the existing parser and `runtime_where_and_predicates` tests
unless the implementation becomes clearer with a new narrow test binary.

## Compatibility Documentation

Update only the exact supported subset:

- `COMPATIBILITY.md`
- `docs/compatibility/operators.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`
- `docs/compatibility/sql-table-dml.md` only if wording needs to mention the
  expanded baseline `WHERE` subset for DML.

Do not overclaim full `IS` expression support, bare boolean predicates,
literal-left truth tests, scalar truth metadata, `HAVING` truth predicates,
`WHERE column`, aliases for `DELETE` / `UPDATE`, joins, subqueries, casts,
string truth conversion, protocol metadata, triggers, privileges, or
general expression evaluation.
