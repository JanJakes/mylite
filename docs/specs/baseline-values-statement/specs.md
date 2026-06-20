# Baseline VALUES Statement

## Summary

This phase adds a narrow standalone MySQL `VALUES` statement:

```sql
VALUES ROW(value[, ...]) [, ROW(value[, ...]) ...] [ORDER BY order_designator ...] [LIMIT ...]
```

It returns an in-memory result set whose columns are named `column_0`,
`column_1`, and so on. The first slice supports literal scalar row values only:
signed decimal integers in MyLite's current scalar range, string literals,
`NULL`, `TRUE`, and `FALSE`. It does not implement `VALUES` as a query
expression inside `UNION`, derived tables, joins, CTEs, DML sources, row
comparisons, or arbitrary expression positions.

The implementation should stay in MyLite's parser/runtime/result layers. It
does not read or write catalog descriptors, does not touch SQLite physical row
storage, and does not require a SQLite fork hook.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline scalar expression projection:
  `docs/specs/baseline-scalar-expression-projection/specs.md`
- Baseline table statement:
  `docs/specs/baseline-table-statement/specs.md`
- Baseline union select lifecycle:
  `docs/specs/baseline-union-select-lifecycle/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `VALUES`:
  <https://dev.mysql.com/doc/refman/8.4/en/values.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_values_statement_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `VALUES ROW(...)` is a standalone result-set DML statement and does not
  require a selected default schema.
- Result columns are implicitly labeled `column_0`, `column_1`, and so on,
  starting at zero.
- `ROW()` without values fails with `3942 / HY000`.
- Every row constructor in one statement must have the same number of values;
  the first mismatched later row fails with `1136 / 21S01`.
- `DEFAULT` inside a standalone `VALUES` row fails with `3943 / HY000`.
- A successful `VALUES` statement sets `ROW_COUNT()` to `-1` and produces zero
  warnings for supported in-range literals.
- MySQL accepts `LIMIT row_count`, `LIMIT offset, row_count`, and
  `LIMIT row_count OFFSET offset`. `LIMIT 0` returns no rows. Signed, string,
  decimal, and `NULL` limit forms are syntax errors. Runtime accepts unsigned
  row counts larger than signed 64-bit up to MySQL's unsigned limit range, but
  MyLite defers that wider numeric envelope and supports the current signed
  64-bit limit literal subset used by existing `SELECT` and `TABLE` paths.
- MySQL accepts `ORDER BY column_N`, quoted/case-varied `column_N`, ordinals,
  row-constant `NULL` and ordinary string literal keys, multiple order keys,
  and optional `ASC` / `DESC`; unknown `column_N` names fail with
  `1054 / 42S22`. On the observed MySQL 8.4.9 runtime, standalone
  `VALUES ... ORDER BY column_N`, `ORDER BY NULL`, and `ORDER BY 'text'`
  validate keys but preserve constructor order for the probed cases. MyLite's
  first slice matches this observed standalone behavior rather than claiming
  full sorting semantics.
- `VALUES ROW(1) AS t`, `VALUES (1)`, and `VALUES ROW(1) WHERE TRUE` are syntax
  errors.
- MySQL accepts arbitrary scalar expressions such as `1 + 2` and `DATABASE()`
  inside row constructors, but that expression surface is deferred here.

## Scope

Supported:

- top-level standalone `VALUES` statement only;
- one or more `ROW(...)` constructors;
- one or more values per row;
- equal column count across all row constructors;
- row values limited to:
  - decimal integer literals with optional unary sign in the current signed
    64-bit scalar range;
  - string literals decoded by MyLite's existing string literal rules,
    including `NO_BACKSLASH_ESCAPES`;
  - `NULL`;
  - `TRUE` and `FALSE`, returned as `1` and `0`;
- result column labels `column_0`, `column_1`, ...;
- public result metadata sufficient for existing result APIs and tests;
- `ROW_COUNT() == -1`, `affected_rows == 0`, and zero warnings for supported
  in-range successful statements;
- optional `ORDER BY` that validates admitted implicit output column names,
  quoted identifiers, positive ordinal designators, and row-constant `NULL` or
  ordinary string literal keys, accepts optional `ASC`/`DESC`, and preserves
  constructor order for this standalone slice;
- optional `LIMIT row_count`, `LIMIT offset, row_count`, and
  `LIMIT row_count OFFSET offset` over the current unsigned decimal integer
  literal syntax whose value fits the existing signed 64-bit limit conversion
  envelope.

Deferred:

- `VALUES` inside `UNION`, `INTERSECT`, `EXCEPT`, parenthesized query
  expressions, derived tables, joins, CTEs, subqueries, `INSERT ... VALUES ROW`
  source reuse, `INSERT ... SELECT`, `REPLACE`, `CREATE TABLE ... SELECT`,
  predicates, row comparisons, scalar expressions, stored routines, prepared
  statement parameter positions, or arbitrary expression positions;
- expression row values beyond the literal subset listed above, including
  arithmetic, functions, user variables, system variables, casts, subqueries,
  date/time functions, JSON construction, and column references;
- `DEFAULT` in standalone `VALUES`;
- binary/hex/bit literals, decimal/floating literals, temporal literals,
  introducers, collations, identifiers as values, and `VALUES()` duplicate-key
  function semantics;
- visible sorting semantics for standalone `VALUES ... ORDER BY`; the admitted
  first slice validates order designators and row-constant order keys, then
  preserves constructor order;
- multiple result-set protocol metadata fidelity beyond current MyLite result
  conventions;
- arbitrary SQLite SQL pass-through, SQLite temporary tables, or SQLite fork
  changes.

## Ownership Boundaries

- Public API: no ABI change. Callers use `mylite_execute()` and the existing
  result and diagnostic accessors.
- Statement context: owns diagnostics reset, `ROW_COUNT()` state, warning
  count, and result-set finalization. `VALUES` is a result-set statement and
  therefore updates `ROW_COUNT()` like `SELECT` and `TABLE`.
- Lexer/parser/AST: owns top-level syntax admission, `VALUES` row-constructor
  AST shape, source spans, arity diagnostics where syntax cannot continue, and
  keeping standalone `VALUES` out of nested query-expression grammar until
  those uses are specified.
- Analyzer/runtime: owns row count validation, literal conversion, limit
  conversion, order-designator validation, result allocation, and deterministic
  diagnostics for unsupported row values.
- Catalog: no involvement. Standalone `VALUES` must not inspect or mutate
  schema, table, column, index, constraint, descriptor-cache, generation, or
  SQLite schema-generation state.
- Result builder: owns output column labels, row values, metadata, cleanup on
  partial failure, and public result object conventions.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged. The statement has no physical storage side effects.
- SQLite physical storage: not used for this slice. Because all supported input
  values are SQL literals, the runtime can build the result directly without
  generated SQLite SQL, prepared statements, or fork hooks.

## Supported SQL Grammar

Supported top-level subset:

```sql
VALUES ROW(value_list) [, ROW(value_list) ...]
VALUES ROW(value_list) [, ROW(value_list) ...] ORDER BY order_designator [ASC | DESC] [, ...]
VALUES ROW(value_list) [, ROW(value_list) ...] LIMIT row_count
VALUES ROW(value_list) [, ROW(value_list) ...] LIMIT offset, row_count
VALUES ROW(value_list) [, ROW(value_list) ...] LIMIT row_count OFFSET offset
```

`value_list` contains one or more supported scalar literal values. The row
constructor keyword is mandatory for standalone `VALUES`.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar shape and is independently
authored for this project:

```lemon
statement(A) ::=
    VALUES(V) values_row_constructor_list(R) values_order_clause_opt(O)
    limit_clause_opt(L). {
    A = mylite_sql_parser_make_values_statement(state, V, R, O, L);
}

values_row_constructor_list(A) ::= values_row_constructor(R). {
    A = mylite_sql_parser_make_node_list(state, R);
}
values_row_constructor_list(A) ::= values_row_constructor_list(L) COMMA values_row_constructor(R). {
    A = mylite_sql_parser_append_node(state, L, R);
}

values_row_constructor(A) ::= ROW(T) LPAREN values_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_values_row_constructor(state, T, V, R);
}
values_row_constructor(A) ::= ROW(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_values_empty_row_constructor(state, T, R);
}

values_value_list(A) ::= values_value(V). {
    A = mylite_sql_parser_make_node_list(state, V);
}
values_value_list(A) ::= values_value_list(L) COMMA values_value(V). {
    A = mylite_sql_parser_append_node(state, L, V);
}

values_value(A) ::= scalar_literal(V). { A = V; }
values_value(A) ::= PLUS(P) INTEGER_LITERAL(V). {
    A = mylite_sql_parser_make_unary_expression(state, P, V);
}
values_value(A) ::= MINUS(M) INTEGER_LITERAL(V). {
    A = mylite_sql_parser_make_unary_expression(state, M, V);
}

values_order_key(A) ::= qualified_identifier(K). { A = K; }
values_order_key(A) ::= INTEGER_LITERAL(T). {
    A = mylite_sql_parser_make_integer_literal(state, T);
}
values_order_key(A) ::= NULL(T). {
    A = mylite_sql_parser_make_null_literal(state, T);
}
values_order_key(A) ::= STRING(T). {
    A = mylite_sql_parser_make_string_literal(state, T);
}
```

The parser may reuse existing literal AST nodes and existing `limit_clause`
nodes where doing so preserves source spans and diagnostics. Expression row
values are deliberately not admitted by this first grammar snippet.

## Semantics

1. The parser builds a standalone `MYLITE_SQL_AST_VALUES_STATEMENT` with row
   constructors, optional order clause, and optional limit clause.
2. Runtime validates that the first row constructor has at least one value. If
   not, it reports `3942 / HY000` with MySQL's standalone-`VALUES` empty-row
   diagnostic.
3. Runtime validates that every later row has the same value count as the first
   row. The first mismatched row reports `1136 / 21S01`.
4. Runtime rejects `DEFAULT` values with `3943 / HY000`.
5. Runtime converts admitted literals using MyLite-owned scalar conversion:
   integers remain exact signed integer text, strings decode through the
   existing string literal decoder, `NULL` remains SQL `NULL`, `TRUE` becomes
   `1`, and `FALSE` becomes `0`.
6. Output columns are named `column_0` through `column_<n-1>`.
7. Optional order designators are validated against the implicit output
   columns. `column_N` is case-insensitive and valid only when `N` has no
   leading zeroes except `0` itself and `N < column_count`. Positive integer
   ordinals are accepted when they are within the one-based output column
   range; `0` and out-of-range ordinals produce unknown-column diagnostics
   matching MySQL's observed behavior.
8. For this standalone slice, admitted `ORDER BY` does not reorder rows. It
   validates identifier and ordinal designators, accepts row-constant `NULL`
   and ordinary string literal keys as no-op keys, and preserves constructor
   order to match the recorded MySQL 8.4.9 behavior.
9. Optional `LIMIT` and `OFFSET` are applied after order validation to the
   constructor-order row sequence. `LIMIT 0` returns no rows. Offsets beyond
   the row count return no rows.
10. Successful execution returns a row result set, `ROW_COUNT() == -1`,
    `affected_rows == 0`, and zero warnings.

## Diagnostics

MySQL-compatible diagnostics for the supported and explicitly rejected subset:

- empty standalone row constructor: `3942 / HY000`
  `Each row of a VALUES clause must have at least one column, unless when used
  as source in an INSERT statement.`;
- mismatched row value count: `1136 / 21S01`
  `Column count doesn't match value count at row N`;
- `DEFAULT` in standalone row constructor: `3943 / HY000`
  `A VALUES clause cannot use DEFAULT values, unless used as a source in an
  INSERT statement.`;
- unknown order designator: `1054 / 42S22`
  `Unknown column '...' in 'order clause'`;
- syntax errors for aliases, missing `ROW`, `WHERE`, signed/unsupported limit
  forms, and unsupported grammar: existing MyLite parse-error conventions with
  MySQL-compatible code where currently available;
- unsupported value literal or expression: deterministic MyLite parse or
  runtime diagnostic until the corresponding scalar expression slice is
  specified;
- allocation failure: existing MyLite out-of-memory diagnostic and cleanup;
- public API misuse: unchanged existing public API behavior.

## Performance and SQLite Handling

Standalone `VALUES` is an explicit SQL-text row constructor. The runtime may
materialize the statement's own literal rows directly into the result object.
That does not duplicate table data or bypass an optimizer path, because no
table access is involved in this slice.

Do not lower this statement to SQLite `VALUES` SQL. MyLite owns the admitted
literal conversion, result labels, diagnostics, `ROW_COUNT()`, and unsupported
surface control. No SQLite fork patch is needed.

## Tests

MySQL-runtime expectation script:

- `packages/libmylite/tests/mysql_baseline_values_statement_expectations.sh`
  verifies MySQL 8.4.9 results, labels, warnings, row-count state, limits, and
  diagnostics used by this spec.

Fast C tests must cover:

- one-row and multi-row `VALUES`;
- implicit labels `column_0`, `column_1`, ...;
- integer, signed integer, string, `NULL`, `TRUE`, and `FALSE` values;
- `ROW_COUNT() == -1`, zero warnings, no affected rows, and no catalog/storage
  side effects;
- empty row, mismatched row counts, and `DEFAULT` diagnostics;
- optional `LIMIT 0`, exact row counts, row counts larger than the constructor
  count, offset forms, and unsupported signed/string/decimal/`NULL` limits;
- optional `ORDER BY` designator validation, `ASC`/`DESC`, row-constant `NULL`
  and ordinary string literal keys, unknown column names, out-of-range
  ordinals, and constructor-order preservation;
- no-default-schema execution;
- unsupported syntax for aliases, missing `ROW`, `WHERE`, expression values,
  function values, identifiers, parameters, and nested query-expression use;
- result cleanup on failure and zero-initialized cleanup for new statement or
  result helpers;
- existing parser, result metadata, statement context, scalar projection,
  `TABLE`, and `UNION` tests still pass.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-query-expressions.md`
only for the exact supported standalone subset. Do not claim full table value
constructors, row constructors in expressions, `VALUES` inside set operations,
derived tables, joins, DML sources, expression row values, or visible
standalone `ORDER BY` sorting semantics.
