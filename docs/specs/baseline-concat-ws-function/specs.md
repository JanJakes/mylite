# Baseline CONCAT_WS Function

## Summary

This phase adds a narrow MySQL-compatible `CONCAT_WS()` string function slice:

```sql
CONCAT_WS(separator, str1[, str2 ...])
```

The supported contexts match the current row-scalar string-function envelope:
no-source scalar `SELECT`, `SELECT ... FROM DUAL`, `DO`, and single-table
row-scalar `SELECT` projection. Arguments may use the documented supported
row-scalar value-function subset, including nested `CONCAT()` / `CONCAT_WS()`
and string helper calls. It does not add general expression predicates,
expression ordering, DML assignment expressions, generated columns, defaults,
or arbitrary expression planning outside that supported value subset.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-left-right-functions/specs.md`
  - `docs/specs/baseline-string-search-functions/specs.md`
- Official MySQL 8.4 Reference Manual, string functions and operators:
  - <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_concat_ws_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `CONCAT_WS()` requires at least two arguments. Zero or one argument fails with
  `1582 / 42000`, `Incorrect parameter count in the call to native function
  'CONCAT_WS'`.
- Function-name whitespace such as `CONCAT_WS (',', 'a', 'b')` is accepted in
  default SQL mode.
- The first argument is the separator. If the separator is `NULL`, the result
  is `NULL`.
- `NULL` values after the separator are skipped.
- Empty strings after the separator are not skipped and still participate in
  separator placement.
- If all arguments after the separator are `NULL`, the result is the empty
  string.
- A single non-`NULL` value after the separator is returned without a separator.
- Numeric and boolean arguments are converted to visible string form.
- `CHAR` trailing spaces are stripped before the function sees the value in the
  current default SQL mode.
- Existing row-scalar descriptor value display shapes are used for decimal,
  temporal, and text descriptors.
- Successful supported calls produce `@@warning_count = 0`; a preceding `DO`
  followed by `ROW_COUNT()` reports `0`, while scalar `SELECT` makes
  `ROW_COUNT()` report `-1`.

MySQL also accepts broader behavior such as binary-string typing, expression
predicates, expression ordering, DML assignment expressions, nonliteral general
expressions beyond MyLite's supported row-scalar value subset, user variables,
and full expression metadata. Those forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits `CONCAT_WS()` with a generic function argument list
  and a wrong-argument-count marker for zero-argument calls, preserving source
  spans for result labels and diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors, validates the minimum argument count, and rejects unsupported
  expression shapes before generated SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: supported calls lower to a MyLite-owned scalar SQLite
  helper, `_mylite_concat_ws`, over quoted descriptor columns and bound literal
  parameters. This keeps row iteration inside SQLite while MyLite owns the
  MySQL-specific `NULL` skipping rule.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT concat_ws_item[, concat_ws_item ...]
SELECT concat_ws_item[, concat_ws_item ...] FROM DUAL
```

`DO` form:

```sql
DO concat_ws_expr[, concat_ws_expr ...]
```

Single-table row-backed forms, with at least one select item containing
`CONCAT_WS()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
concat_ws_expr:
    CONCAT_WS ( concat_ws_arg , concat_ws_arg [, concat_ws_arg ...] )

concat_ws_arg:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | scalar_subquery                 -- no-source / DUAL only, existing subset
  | descriptor_column_reference     -- table-backed SELECT only
  | supported_row_scalar_value_function
  | ( concat_ws_arg )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families match the current `CONCAT()` row-scalar subset:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The following remain outside this phase:

- `WHERE CONCAT_WS(...) ...`, `HAVING CONCAT_WS(...) ...`, expression
  `ORDER BY`, grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = CONCAT_WS('-', a, b)`;
- binary string result typing, binary-string descriptor operands, binary casts,
  hexadecimal string operands, parameters, user variables, correlated
  subqueries, CTEs, stored functions, and arbitrary expressions outside the
  supported row-scalar value subset;
- full character-set, collation, coercibility, or protocol metadata parity.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CONCAT_WS(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= CONCAT_WS(T) LPAREN RPAREN(R).
```

The first production builds a `concat_ws_function` AST node. Runtime validates
that at least two arguments are present. The second production builds a
wrong-argument-count marker so runtime can emit the native-function
parameter-count diagnostic for `CONCAT_WS()`.

## Runtime Semantics

Evaluation follows the observed MySQL 8.4.9 behavior for the admitted values:

1. Evaluate the separator and value arguments.
2. If the separator is `NULL`, return SQL `NULL`.
3. Convert non-`NULL` admitted arguments to their existing visible text form.
4. Skip `NULL` arguments after the separator.
5. Concatenate non-`NULL` values in argument order, inserting the separator
   between adjacent non-`NULL` values.
6. If there are no non-`NULL` values after the separator, return the empty
   string.

Generated SQLite uses:

```sql
_mylite_concat_ws(separator_expr, value_expr[, value_expr ...])
```

Literal and session scalar values are bound parameters. Descriptor columns are
quoted stable physical column references. The helper assembles only the result
cell for the current row and does not materialize full result sets in MyLite.

## Diagnostics

- `1582 / 42000` for zero or one total argument.
- Existing unknown-column diagnostics for unresolved descriptor arguments.
- Existing row-scalar unsupported diagnostics for unsupported argument shapes,
  unsupported descriptor types, predicates, ordering
  expressions, DML assignment values, parameters, user variables, and broader
  expression forms.
- Existing allocation and physical SQLite failure diagnostics for internal
  allocation or execution failures.

## Tests

Fast C tests cover:

- no-source, `DUAL`, and `DO` supported calls;
- separator `NULL`, skipped later `NULL` values, all-later-`NULL` empty result,
  empty-string value preservation, one value after the separator, numeric and
  boolean conversion, labels, row count, and warning count;
- row-scalar table-backed projection over `VARCHAR`, `CHAR`, `TEXT`, integer,
  `DECIMAL`, `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` descriptors;
- `WHERE`, `ORDER BY`, and `LIMIT` envelope reuse around row-scalar projection;
- reopen persistence and `.mylite` preamble preservation for the read-only
  query path;
- wrong argument counts, unknown columns, supported nested value functions,
  unsupported predicates, unsupported DML assignment use, and unsupported binary
  operands.

MySQL expectation probes cover the user-visible result values, diagnostics,
labels, row count, and warning count for the admitted subset.

## Compatibility Documentation

`COMPATIBILITY.md`, `docs/compatibility/functions-string.md`, and
`docs/compatibility/type-system-literals-conversion.md` must describe only this
limited `CONCAT_WS()` surface. They must not claim full expression support,
binary result typing, general expression metadata, predicates, ordering
expressions, DML assignment expressions, or full character-set/collation
semantics.
