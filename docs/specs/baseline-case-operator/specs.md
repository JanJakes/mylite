# Baseline CASE Operator

## Summary

This phase admits a deliberately small MySQL `CASE` operator surface in the
existing no-source and `FROM DUAL` scalar `SELECT` path:

```sql
SELECT CASE WHEN condition THEN result [WHEN condition THEN result ...] [ELSE result] END
SELECT CASE value WHEN compare_value THEN result [WHEN compare_value THEN result ...] [ELSE result] END
SELECT CASE ... END FROM DUAL
```

The slice supports searched `CASE` and simple `CASE` over MyLite's current
signed-64 scalar expression domain: decimal integer, `TRUE`, `FALSE`, `NULL`,
supported scalar control-flow/comparison helpers, unary and binary signed-64
arithmetic currently admitted by scalar projection, scalar comparisons, keyword
logical operators, scalar `IS`, parenthesized expressions, and nested `CASE`.

This is still not a general expression engine. It does not admit table-backed
`CASE`, descriptor-column expression projection, `WHERE CASE ...`, DML
assignment `CASE`, strings, decimals, floats, hex, bit literals, temporal
values, casts, collations, parameters, variables or session/system scalar
reads inside `CASE`, subqueries, CTEs, row constructors, expression
metadata, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Flow control functions:
    <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Functions and operators overview:
    <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Expressions:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_case_operator_expectations.sh`.

The MySQL manual defines two `CASE` operator forms. The searched form returns
the result associated with the first true condition. The simple form compares a
case value to each `WHEN` value and returns the result associated with the
first equality comparison that is true. If no branch matches, `ELSE` is used;
without `ELSE`, the result is `NULL`.

Runtime probes against MySQL 8.4.9 establish these expectations for this
slice:

- searched conditions treat nonzero and non-`NULL` as true;
- zero and `NULL` searched conditions do not match;
- simple `CASE` uses ordinary equality semantics, so `CASE NULL WHEN NULL`
  does not match;
- `TRUE` and `FALSE` result values render as `1` and `0`;
- selected signed integer results render in canonical decimal text;
- missing `ELSE` returns SQL `NULL` when no branch matches;
- searched and simple `CASE` choose the first matching branch;
- nested `CASE` is accepted;
- scalar arithmetic, comparison, logical, and scalar `IS` expressions can be
  used as admitted conditions, case values, comparison values, and result
  values;
- unevaluated result branches and unevaluated later `WHEN` comparisons do not
  produce division-by-zero warnings;
- evaluated condition, case-value, and comparison expressions produce their
  normal staged scalar warnings;
- a successful scalar `CASE` select makes a following `ROW_COUNT()` return
  `-1`;
- inside the same select list, `@@warning_count` and `ROW_COUNT()` observe the
  previous-statement diagnostics snapshot, not warnings staged by earlier
  select items;
- default column labels use the source text of the `CASE` expression unless an
  explicit alias is provided;
- the stored-program `END CASE` terminator is not part of this expression
  slice and is a syntax error in scalar `SELECT`.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context: scalar `CASE` `SELECT` statements use existing
  row-returning `SELECT` result conventions, including one result row, zero
  affected rows, previous row-count state `-1` after success, and warning
  storage through the existing diagnostics area.
- Lexer/parser/AST: the parser admits `CASE` as an expression. The AST must
  represent searched and simple `CASE` explicitly, with a `WHEN` list and an
  optional `ELSE` clause. It must not rely on `NULL` positional children because
  `mylite_sql_ast_node_append_child()` intentionally ignores `NULL` children.
- Analyzer/runtime: the scalar projection analyzer accepts `CASE` only in the
  existing no-source and `FROM DUAL` scalar select path. Runtime evaluation is
  MyLite-owned and evaluates only the branch expressions MySQL would evaluate.
- Catalog: not involved. This feature must not read or mutate schemas, table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: appends columns and one row through existing public result
  helpers. Result text for selected integer/boolean/`NULL` values follows the
  current scalar projection conventions.
- Storage/VFS/file format: no file-format or VFS changes. Scalar `CASE` must
  not touch user-table storage or the `.mylite` preamble.
- SQLite physical row storage: not involved. This feature should not generate
  SQLite SQL and should not add a SQLite fork patch.

## Syntax

The independent MyLite subset is:

```ebnf
case_expression:
    CASE searched_case_when_list case_else_opt END
  | CASE expression simple_case_when_list case_else_opt END

searched_case_when_list:
    searched_case_when
  | searched_case_when_list searched_case_when

searched_case_when:
    WHEN expression THEN expression

simple_case_when_list:
    simple_case_when
  | simple_case_when_list simple_case_when

simple_case_when:
    WHEN expression THEN expression

case_else_opt:
    empty
  | ELSE expression
```

This grammar is intentionally expression-local. It does not add stored-program
`CASE ... END CASE`, statement lists, or procedural flow control.

The intended MyLite Lemon shape is:

```lemon
expression(A) ::= CASE(T) searched_case_when_list(W) case_else_opt(E) END(R). {
    A = mylite_sql_parser_make_searched_case_expression(state, T, W, E, R);
}
expression(A) ::= CASE(T) expression(V) simple_case_when_list(W) case_else_opt(E) END(R). {
    A = mylite_sql_parser_make_simple_case_expression(state, T, V, W, E, R);
}

searched_case_when_list(A) ::= searched_case_when(B). {
    A = mylite_sql_parser_make_case_when_list(state, B);
}
searched_case_when_list(A) ::= searched_case_when_list(B) searched_case_when(C). {
    A = mylite_sql_parser_append_case_when(state, B, C);
}
searched_case_when(A) ::= WHEN(W) expression(C) THEN expression(R). {
    A = mylite_sql_parser_make_case_when_clause(state, W, C, R);
}

simple_case_when_list(A) ::= simple_case_when(B). {
    A = mylite_sql_parser_make_case_when_list(state, B);
}
simple_case_when_list(A) ::= simple_case_when_list(B) simple_case_when(C). {
    A = mylite_sql_parser_append_case_when(state, B, C);
}
simple_case_when(A) ::= WHEN(W) expression(C) THEN expression(R). {
    A = mylite_sql_parser_make_case_when_clause(state, W, C, R);
}

case_else_opt(A) ::= . { A = NULL; }
case_else_opt(A) ::= ELSE(E) expression(B). {
    A = mylite_sql_parser_make_case_else_clause(state, E, B);
}
```

Implementation may share the two `WHEN` list nonterminals when Lemon conflict
behavior remains clear. `CASE`, `WHEN`, `THEN`, `ELSE`, and `END` must be
mapped for parser use without broadening unrelated statement syntax.

## Semantics

### Searched CASE

For `CASE WHEN condition THEN result ... ELSE fallback END`:

1. Evaluate each `condition` from left to right.
2. A condition is true when it evaluates to a non-`NULL`, nonzero signed-64
   integer value.
3. Evaluate and return only the `result` associated with the first true
   condition.
4. If no condition is true, evaluate and return `fallback` when `ELSE` exists.
5. If no condition is true and no `ELSE` exists, return SQL `NULL`.

### Simple CASE

For `CASE value WHEN compare_value THEN result ... ELSE fallback END`:

1. Evaluate `value` once.
2. Evaluate each `compare_value` from left to right until a match is found.
3. A match uses ordinary equality semantics for the admitted signed-64/`NULL`
   domain: non-`NULL` integer values match when equal; any comparison involving
   `NULL` does not match.
4. Evaluate and return only the matching `result`.
5. If no comparison matches, evaluate and return `fallback` when `ELSE` exists.
6. If no comparison matches and no `ELSE` exists, return SQL `NULL`.

### Admitted Expression Domain

Every case value, searched condition, comparison value, result, and `ELSE`
expression must be within this slice's scalar domain:

- signed-64 decimal integer literal with optional unary `+` or `-`;
- `TRUE`, `FALSE`, and `NULL`;
- supported scalar `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, and
  `ISNULL()` calls;
- supported scalar `CASE`;
- supported parenthesized expressions;
- supported signed-64 scalar `+`, binary `-`, `*`, `%`, infix `MOD`,
  `MOD(left, right)`, and infix `DIV`;
- supported signed-64 scalar comparisons;
- supported keyword scalar logical `NOT`, `AND`, `XOR`, and `OR`;
- supported scalar `IS [NOT] NULL` / `TRUE` / `FALSE` / `UNKNOWN`.

The parser may accept broader expression shapes to preserve normal syntax
diagnostics, but runtime validation rejects unsupported forms deterministically
before returning a value. Unsupported forms are rejected even when they appear
in an unevaluated branch; this is a MyLite scope-control decision for the
baseline slice. A no-source or `FROM DUAL` select list may still mix separate
session scalar select items next to `CASE` items; those session scalar reads are
not part of the admitted `CASE` child-expression domain yet.

### Warnings And Diagnostics

Evaluated branch expressions preserve existing scalar warning behavior:

- Evaluated `DIV` or modulo-by-zero children stage one warning per evaluated
  child expression and feed `NULL` into the parent expression.
- Unevaluated result branches, later searched conditions after a true
  condition, and later simple `WHEN` comparisons after a match do not stage
  warnings.
- Unsupported literals, identifiers, table-backed expressions, parameters,
  subqueries, string values, decimal values, float values, hex values, bit
  values, and arithmetic overflows raise deterministic errors.
- Successful in-range `CASE` projection has `warning_count == 0` unless an
  evaluated supported child expression stages warnings.
- A scalar `CASE` select updates the previous diagnostics snapshot like the
  existing scalar select path.

### Labels And Metadata

The default column label is the exact `CASE ... END` expression span, after the
same comment-stripping behavior already used by the lexer/parser. Explicit
identifier, quoted-identifier, and string-literal aliases use the existing
select-item alias path. This slice does not add expression metadata such as
precise MySQL type, charset, collation, length, flags, or origin metadata.

## Runtime And Storage Design

`CASE` stays in the MyLite scalar evaluator. It should not be translated to
SQLite SQL because the supported path has no table source and because MySQL
branch evaluation and diagnostics sequencing must remain MyLite-owned.

The evaluator should:

1. Validate the whole `CASE` subtree against the admitted scalar domain.
2. Evaluate searched conditions or the simple case value and `WHEN` comparison
   values left-to-right.
3. Evaluate only the selected result or selected `ELSE` expression.
4. Return a `session_scalar_cell` using the existing scalar result text and
   staged-warning conventions.
5. Avoid recursion depth tied to C call stack where practical by following the
   current small explicit-stack style used by scalar control-flow functions.

No catalog, descriptor, VFS, preamble, or SQLite schema-generation state may be
changed by this feature.

## Diagnostics

Expected diagnostics include:

- Syntax errors for malformed `CASE` forms, including missing `WHEN`, missing
  condition, missing `THEN`, missing result, missing `END`, and stored-program
  `END CASE` terminator in scalar `SELECT`.
- Unsupported scalar projection diagnostic for table-backed `CASE`, DML
  assignment `CASE`, unsupported branch expressions, unsupported literals,
  subqueries, row constructors, parameters, and identifiers outside supported
  literal/control-flow/scalar-expression forms.
- Existing arithmetic overflow diagnostic for evaluated or validation-relevant
  signed-64 scalar arithmetic overflow.
- Existing division-by-zero warnings for evaluated supported `DIV`, modulo,
  and `MOD()` children.
- Allocation failures use existing `MYLITE_NOMEM` handling.
- Public API misuse behavior is unchanged.

## Performance

This phase is O(number of `CASE` arms plus evaluated expression cost) for scalar
no-source/`DUAL` selects. It does not scan tables, materialize rowsets, build
temporary SQLite tables, add indexes, or fork SQLite. The only dynamic memory
needed beyond existing result construction is small AST/runtime stack growth
proportional to nested expression depth and `WHEN` arm count.

## Tests

Add:

- `packages/libmylite/tests/mysql_baseline_case_operator_expectations.sh`
  with MySQL 8.4.9 verified expectations.
- Parser tests for searched and simple `CASE`, multiple `WHEN` arms, optional
  `ELSE`, missing `ELSE`, nested `CASE`, aliases/default labels through spans,
  precedence with arithmetic/comparison/logical/scalar `IS`, and malformed
  syntax.
- A new C runtime test binary `runtime_case_operator_test.c`, registered as
  `libmylite.runtime.case_operator`.

Runtime coverage should include:

- searched true, false, `NULL`, no-`ELSE`, first-match, and nested forms;
- simple first-match, later-match, no-match, `NULL` comparison no-match, and
  nested forms;
- result values `NULL`, `TRUE`, `FALSE`, signed boundaries, and normalized
  integers;
- admitted arithmetic, comparison, logical, scalar `IS`, and control-flow child
  expressions;
- branch short-circuiting and warning count behavior for selected and skipped
  warning-producing expressions;
- default labels, explicit aliases, `FROM DUAL`, mixed scalar select lists,
  affected rows, warning count, absence of table result side effects, and
  following diagnostics snapshot behavior;
- unsupported table-backed `CASE`, DML assignment `CASE`, strings, decimals,
  floats, hex, bit literals, identifiers, parameters, subqueries, row
  constructors, and malformed syntax;
- file-backed preamble preservation, unchanged catalog/schema-generation
  counters, independent handles, and zero-initialized cleanup for new runtime
  objects.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`: mark `CASE` as limited and mention the no-source/`DUAL`
  scalar projection subset in the `SELECT` and projection-list rows.
- `docs/compatibility/operators.md`: mark `CASE` as limited.
- `docs/compatibility/functions-control-flow.md`: add a `CASE` row even though
  MySQL lists it as an operator in the flow-control section.
- `docs/compatibility/sql-query-expressions.md`: include `CASE` in no-source
  and `FROM DUAL` scalar projection wording.

Do not claim table-backed expression projection, DML assignment expressions,
predicate `CASE`, full result type aggregation, string/collation behavior,
subqueries, or expression metadata.
