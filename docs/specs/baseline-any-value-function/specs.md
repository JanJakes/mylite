# Baseline ANY_VALUE Function

## Goal

Add a narrow MySQL compatibility slice for:

```sql
ANY_VALUE(expr)
```

`ANY_VALUE()` is primarily useful in grouped queries that intentionally choose a
representative non-grouped value. This baseline keeps the implementation small:
no-source scalar use unwraps to the existing scalar expression engine, row-backed
scalar use unwraps to the existing row-scalar planner, and grouped use accepts
one descriptor column argument in selected grouped aggregate slots.

## Sources

- Official MySQL 8.4 miscellaneous function documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html#function_any-value>
- Official MySQL 8.4 `GROUP BY` handling documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh`.

The MyLite grammar, specification, and implementation are independently
authored from official documentation and observed MySQL 8.4.9 behavior. Do not
copy MySQL grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against the local MySQL 8.4.9 runtime establish these
expectations for this slice:

- The default 8.4.9 SQL mode includes `ONLY_FULL_GROUP_BY`.
- `SELECT ANY_VALUE(1), ANY_VALUE(NULL), ANY_VALUE('abc')` returns the argument
  values and produces no warnings.
- Whitespace between the function name and `(` is accepted.
- `ANY_VALUE` is accepted as an unquoted table name in tested DDL contexts.
- `SELECT ANY_VALUE(v) FROM t ORDER BY v` behaves as a row-scalar projection and
  returns one value per matching input row.
- `SELECT ANY_VALUE(v) FROM t WHERE false` returns no rows.
- `SELECT g, ANY_VALUE(v), ANY_VALUE(s), MAX(v), COUNT(*) FROM t GROUP BY g`
  is accepted under `ONLY_FULL_GROUP_BY`; observed representative values are not
  guaranteed when a group contains different candidate values.
- `ANY_VALUE()` and `ANY_VALUE(1,2)` return error 1582 / SQLSTATE `42000` with
  the native-function parameter-count diagnostic.
- `ANY_VALUE(*)` and `ANY_VALUE(DISTINCT v)` are syntax errors in the observed
  contexts.
- `ANY_VALUE(missing)` over a table returns error 1054 / SQLSTATE `42S22`.

## Supported Surface

MyLite supports this exact subset:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar expressions where
  the argument is already supported by the existing scalar expression engine;
- single-table row-scalar projection where the argument is already supported by
  the existing descriptor-driven row-scalar planner;
- grouped `SELECT` over the current descriptor-backed `GROUP BY` envelope where
  selected descriptor group columns are followed by aggregate-like result
  expressions;
- grouped `ANY_VALUE(column)` with one unqualified or source-qualified
  descriptor column argument;
- grouped `ANY_VALUE(column)` over descriptor columns that the existing selected-row
  readback path can return safely;
- optional aliases on selected `ANY_VALUE()` results;
- selected aggregate aliases in the existing grouped `HAVING ... IS NULL` /
  `IS NOT NULL` and grouped `ORDER BY alias [ASC|DESC]` paths;
- successful in-range expressions produce `warning_count == 0`;
- result labels follow the existing public result conventions: the original
  expression text when no alias is supplied, or the explicit alias.

For grouped queries, `ANY_VALUE(column)` means "choose one value from the group".
When a group contains different candidate values, MyLite does not claim which
row contributes the result. Tests that need deterministic values must make all
candidate values for that group identical, use `NULL`-only groups, or assert
only properties that do not depend on the representative row.

## Deferred Surface

This slice intentionally does not support:

- mixed ungrouped aggregate queries such as `SELECT ANY_VALUE(col), MAX(col)
  FROM t`;
- expression arguments in grouped `ANY_VALUE()`;
- `ANY_VALUE(*)`, `ANY_VALUE(DISTINCT ...)`, or multi-argument forms;
- use as a window function;
- `ANY_VALUE()` in DML assignments, defaults, generated columns, check
  constraints, indexes, or arbitrary predicate expression positions;
- full functional-dependency inference beyond the current grouped projection
  rules;
- optimizer influence over which grouped row supplies the representative value;
- protocol-grade type metadata beyond the existing scalar, row-scalar, and
  grouped result descriptor conventions.

## Grammar

The parser admits the independently authored one-argument function form:

```lemon
expression ::= ANY_VALUE LPAREN expression RPAREN.
```

Wrong-arity calls are represented with an explicit AST error node so execution
can return MySQL-compatible native-function parameter-count diagnostics:

```lemon
expression ::= ANY_VALUE LPAREN RPAREN.
expression ::= ANY_VALUE LPAREN expression COMMA function_argument_list RPAREN.
```

Identifier grammar admits the name as an ordinary identifier where MyLite's
current identifier grammar permits nonreserved function names:

```lemon
identifier ::= ANY_VALUE.
```

The runtime, not the grammar, enforces whether the argument is valid for scalar,
row-scalar, or grouped use.

## Runtime Semantics

### Scalar and Row-Scalar Use

No-source scalar and `FROM DUAL` `ANY_VALUE(expr)` unwrap to `expr` before
evaluation. This preserves existing scalar behavior for literals, supported
functions, session variables, arithmetic, casts, scalar subqueries, warnings,
and result values.

Table-backed row-scalar `ANY_VALUE(expr)` unwraps to `expr` before row-scalar
planning. Descriptor column resolution, source qualification, predicates,
ordering, limits, generated SQLite SQL, parameter binding, and result readback
remain owned by the existing row-scalar planner.

Wrong-arity calls return the same native-function parameter-count diagnostic as
MySQL. Unsupported child expressions return the existing scalar or row-scalar
unsupported diagnostics for the unwrapped child.

### Grouped Use

Grouped `ANY_VALUE(column)` is planned as an aggregate-like result item for the
purpose of MyLite's grouped projection validation. It suppresses
`ONLY_FULL_GROUP_BY` rejection for its own descriptor column argument only.

The grouped planner resolves the argument from MyLite descriptors, not SQLite
metadata. Unknown or ambiguous names use existing deterministic column
diagnostics for the relevant clause. The function does not create, update, or
delete catalog descriptors.

Physical SQL lowers grouped `ANY_VALUE(column)` to the resolved physical
descriptor column in the grouped `SELECT` list. This deliberately uses SQLite's
standard grouped bare-column behavior as an internal representative-value
mechanism over MyLite-owned physical tables. The generated identifier is always
quoted and, for joined grouped sources, source-qualified through the existing
stable internal source aliases.

Result conversion reads the selected SQLite value through
`append_selected_sqlite_row_value_with_descriptor()`, preserving existing MyLite
integer, floating, text, blob, and `NULL` readback behavior.

### Diagnostics

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- wrong arity: native-function parameter-count diagnostics for `ANY_VALUE`;
- unknown table or schema: existing source-resolution diagnostics;
- unknown argument column: existing unknown-column diagnostics for the active
  clause;
- grouped argument is not a descriptor column: `ANY_VALUE(column) supports only
  descriptor columns`;
- grouped `HAVING` or `ORDER BY` unsupported form: existing grouped
  `HAVING`/`ORDER BY` unsupported diagnostics;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Architecture

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: unchanged. The function does not need new per-statement
  state.
- Lexer/parser/AST: add a nonreserved function token, one function AST node
  kind, and one argument-count error node kind. Do not introduce general
  function-call parsing.
- Analyzer/planner/runtime: scalar and row-scalar paths unwrap to existing
  planners; grouped planning treats `ANY_VALUE(column)` as a selected
  aggregate-like item while resolving the argument from MyLite descriptors.
- Catalog: untouched. The function does not read or mutate descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: scalar and row-scalar result metadata stays on the existing
  child-expression path; grouped result values use descriptor-aware readback.
- Storage/VFS/file format: untouched. The function does not modify the SQLite
  payload or the MyLite preamble.
- SQLite: use MyLite wrapper/translation over ordinary SQLite grouped
  selection. No SQLite fork patch or public SQLite extension callback is
  required.

## Performance

Scalar and row-scalar `ANY_VALUE()` adds no material runtime cost because the
planner unwraps the function to the existing child expression. Grouped
`ANY_VALUE(column)` stays close to the current grouped aggregate path: SQLite
performs source scanning, predicate filtering, grouping, ordering, and limiting.
MyLite does descriptor resolution and result conversion, but it does not
materialize groups in MyLite memory or perform grouped representative-value
selection itself.

## Tests

Add fast C coverage under `packages/libmylite/tests/`:

- parser accepts supported scalar, row-scalar, grouped, mixed-case, aliased,
  whitespace, and identifier-name contexts;
- parser produces explicit wrong-arity AST nodes for zero and multiple
  arguments;
- runtime covers no-source and `FROM DUAL` scalar values, warnings, labels, and
  `ROW_COUNT()` preservation;
- runtime covers table-backed row-scalar descriptor column unwrap, `WHERE`,
  `ORDER BY`, empty filters, and aliases;
- grouped runtime covers integer, nullable integer, and nonbinary string
  descriptor arguments with deterministic same-value and all-`NULL` groups;
- grouped runtime covers composition with `MAX()` and `COUNT(*)`, selected
  alias `HAVING ... IS NOT NULL`, selected alias `ORDER BY ... DESC`, and
  `LIMIT`;
- diagnostics cover wrong arity, syntax rejections for `*`/`DISTINCT`, unknown
  argument columns, unsupported grouped expression arguments, and unsupported
  mixed ungrouped aggregate use;
- `ANY_VALUE` remains usable as a table name in tested DDL contexts;
- no catalog, file-format, VFS, or public ABI changes are introduced.

Verification before marking done:

1. `packages/libmylite/tests/mysql_baseline_any_value_function_expectations.sh`
2. `cmake --build --preset dev`
3. Focused CTest entries for parser and grouped/runtime function coverage.
4. `cmake --workflow --preset check`
