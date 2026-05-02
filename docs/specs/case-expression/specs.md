# CASE expressions

## Scope

This feature specifies SQL `CASE` expressions for MyLite's shared scalar
expression system. It covers the expression operator forms, not stored-program
`CASE` statements.

In scope:

- simple `CASE` expressions:
  `CASE expr WHEN compare_expr THEN result_expr [WHEN ...] [ELSE result_expr] END`
- searched `CASE` expressions:
  `CASE WHEN condition THEN result_expr [WHEN ...] [ELSE result_expr] END`
- parser and AST support in the shared expression grammar
- binder support for every currently supported scalar expression call site:
  - no-table `SELECT`
  - one-table `SELECT` projection expressions
  - one-table `SELECT` `WHERE` predicates
  - one-table `SELECT` hidden and visible `ORDER BY` expressions
  - single-table `UPDATE` assignment, `WHERE`, and `ORDER BY` expressions
  - single-table `DELETE` `WHERE` and `ORDER BY` expressions
- result metadata inference for projected CASE expressions and hidden order
  keys
- short-circuit-sensitive evaluation, warning collection, and strict DML error
  promotion for evaluated CASE operands
- tests whose expected rows, metadata, warnings, errors, and side effects are
  verified against MySQL 8.4.9

Out of scope:

- stored-program `CASE` statements, which use statement lists and `END CASE`
- `INSERT ... VALUES`, `INSERT ... SET`, generated default expressions, and
  generated columns; these remain deferred to match the current scalar-function
  checkpoint
- joins, grouped queries, `HAVING`, `ON`, CTEs, subqueries, views,
  information-schema filters, and multi-table DML
- aggregate/window functions, user variables, parameters, assignment
  expressions, casts, explicit `COLLATE`, `BINARY expr`, row constructors,
  JSON operators, regular expressions, and unsupported function families inside
  CASE operands except where later tasks make those expression shapes available
- exhaustive MySQL type aggregation for every temporal, JSON, geometry, enum,
  set, bit, binary-string, and collation combination
- optimizer rewrites or constant folding that could change MySQL-visible
  warning or error timing

Parser acceptance alone is not support. CASE expressions should be marked
supported only after the runtime, metadata, warnings, diagnostics, and tests
land for the expression contexts above.

## Sources

- MySQL 8.4 Reference Manual, Flow Control Functions:
  https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html
- MySQL 8.4 Reference Manual, Expressions:
  https://dev.mysql.com/doc/refman/8.4/en/expressions.html
- MySQL 8.4 Reference Manual, Type Conversion in Expression Evaluation:
  https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html
- MySQL 8.4 Reference Manual, Comparison Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html
- Existing MyLite specs:
  - `docs/specs/expression-operator-foundation/specs.md`
  - `docs/specs/scalar-built-in-functions/specs.md`
  - `docs/specs/select-table-core/specs.md`
  - `docs/specs/where-clause/specs.md`
  - `docs/specs/order-limit-offset/specs.md`
  - `docs/specs/result-metadata-expression-labels/specs.md`
  - `docs/specs/update-single-table/specs.md`
  - `docs/specs/delete-single-table/specs.md`

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849`, using:

- `docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --force --batch --raw --show-warnings`
- `docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv`

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

Runtime probes used MySQL 8.4.9 with the default SQL mode:

```text
ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,
ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION
```

### Test fixture

Representative probes used:

```sql
DROP DATABASE IF EXISTS mylite_case_expr_probe;
CREATE DATABASE mylite_case_expr_probe;
USE mylite_case_expr_probe;
SET NAMES utf8mb4;

CREATE TABLE t (
  id INT PRIMARY KEY,
  n INT,
  s VARCHAR(20),
  z VARCHAR(20),
  nullable INT NULL
);

INSERT INTO t VALUES
  (1, 1, 'alpha', '2', NULL),
  (2, 2, 'Beta', '2a', 5),
  (3, NULL, 'gamma', 'a', 0),
  (4, 0, NULL, '10', NULL);
```

### Result semantics

Simple CASE evaluates the base expression, then compares it to each `WHEN`
comparison expression in source order. The first true equality comparison
selects the matching `THEN` result. Searched CASE evaluates `WHEN` conditions
in source order. The first true condition selects the matching `THEN` result.
If no `WHEN` matches, the `ELSE` result is selected. Without `ELSE`, the result
is `NULL`.

Representative results:

| SQL | Result |
| --- | --- |
| `CASE n WHEN 1 THEN 'one' WHEN 2 THEN 'two' ELSE 'other' END` | `one`, `two`, `other`, `other` over fixture rows |
| `CASE WHEN n > 1 THEN 'gt1' WHEN nullable IS NULL THEN 'nullable' ELSE 'fallback' END` | `nullable`, `gt1`, `fallback`, `nullable` |
| `CASE WHEN nullable THEN 'true' WHEN nullable IS NULL THEN 'is-null' ELSE 'false' END` | `is-null`, `true`, `false`, `is-null` |
| `CASE NULL WHEN NULL THEN 'match' ELSE 'else' END` | `else` |
| `CASE 1 WHEN '1' THEN 'string-match' ELSE 'miss' END` | `string-match` |
| `CASE 'a' WHEN 'A' THEN 'ci-match' ELSE 'miss' END` | `ci-match` under the verified default collation |
| `CASE WHEN 0 THEN 'no' END` | `NULL` |

Simple CASE uses ordinary comparison semantics, not null-safe equality.
`NULL` therefore does not match `NULL`. Numeric and string coercion, warnings,
and collation behavior should reuse the Task 16 comparison machinery. Under
the verified default collation, ASCII string comparison is case-insensitive;
explicit binary/collation forms remain deferred until those expression forms
are supported.

### Evaluation order, warnings, and binding

CASE evaluation is short-circuit-sensitive. MyLite must evaluate only the
conditions or comparison operands needed to find the first match, and only the
selected result expression. It must still bind and validate all child
expressions before execution; an unknown column or unknown function in an
unselected branch is still an error.

Representative warning probes:

| SQL | Result | Warnings |
| --- | --- | --- |
| `CASE WHEN 1 THEN 10 ELSE 1/0 END` | `10` | none |
| `CASE WHEN 0 THEN 1/0 ELSE 20 END` | `20` | none |
| `CASE WHEN 1/0 THEN 10 ELSE 20 END` | `20` | one 1365 `Division by 0` warning |
| `CASE WHEN 0 THEN 10 ELSE 1/0 END` | `NULL` | one 1365 `Division by 0` warning |
| `CASE 1 WHEN 1 THEN 10 WHEN 1/0 THEN 20 ELSE 30 END` | `10` | none |
| `CASE 1 WHEN 0 THEN 10 WHEN 1/0 THEN 20 ELSE 30 END` | `30` | one 1365 `Division by 0` warning |
| `CASE 1/0 WHEN 0 THEN 10 ELSE 20 END` | `20` | one 1365 `Division by 0` warning |

Binding probes:

| SQL | MySQL behavior |
| --- | --- |
| `SELECT CASE WHEN 1 THEN 1 ELSE missing_col END FROM t LIMIT 1` | error 1054 / `42S22`, unknown column in `field list` |
| `SELECT CASE WHEN 1 THEN 1 ELSE NO_SUCH_FUNCTION() END` | error 1305 / `42000`, function does not exist |

The implementation must not use eager evaluation of every `THEN`/`ELSE`
expression for metadata or planning. Metadata inference must inspect expression
descriptors without producing runtime warnings.

### Statement contexts

The same expression semantics apply anywhere MyLite already supports scalar
expressions. Representative MySQL results:

| SQL | Result or side effect |
| --- | --- |
| `SELECT CASE WHEN 2 > 1 THEN 'rowless' ELSE 'miss' END` | `rowless` |
| `SELECT id, CASE WHEN n IS NULL THEN 'nil' ELSE CONCAT('n=', n) END FROM t ORDER BY id` | `n=1`, `n=2`, `nil`, `n=0` |
| `SELECT id FROM t WHERE CASE WHEN n = 1 THEN 1 WHEN s IS NULL THEN 1 ELSE 0 END ORDER BY id` | ids `1`, `4` |
| `SELECT id FROM t ORDER BY CASE WHEN s IS NULL THEN 1 ELSE 0 END, CASE WHEN n IS NULL THEN 99 ELSE n END, id` | ids `1`, `2`, `3`, `4` |

Mutation probes used:

```sql
CREATE TABLE order_dml_case (id INT PRIMARY KEY, v INT, marker VARCHAR(10));
INSERT INTO order_dml_case VALUES (1, 30, 'a'), (2, 10, 'b'), (3, 20, 'c');

UPDATE order_dml_case
SET marker = CASE WHEN id = 2 THEN 'hit' ELSE 'miss' END
WHERE CASE WHEN v >= 10 THEN 1 ELSE 0 END
ORDER BY CASE WHEN id = 2 THEN 0 ELSE 1 END, id
LIMIT 1;

DELETE FROM order_dml_case
WHERE CASE WHEN v >= 10 THEN 1 ELSE 0 END
ORDER BY CASE WHEN marker = 'hit' THEN 0 ELSE 1 END, id
LIMIT 1;
```

The update changed only row `2` to `marker='hit'`. The delete then removed row
`2`, leaving rows `1` and `3`.

Strict DML probes confirmed that unselected warning-producing branches do not
affect the statement, while selected warning-producing branches fail under the
verified default mode:

| SQL | MySQL behavior |
| --- | --- |
| `UPDATE d SET v = CASE WHEN 1 THEN v + 1 ELSE MOD(7,0) END WHERE id = 1` | succeeds and increments `v` |
| `UPDATE d SET v = CASE WHEN 0 THEN v + 1 ELSE MOD(7,0) END WHERE id = 1` | error 1365 / `22012`, row unchanged |
| `DELETE FROM d WHERE CASE WHEN id = 1 THEN 0 ELSE MOD(7,0) END` | succeeds and deletes no rows |
| `DELETE FROM d WHERE CASE WHEN id = 1 THEN MOD(7,0) ELSE 0 END` | error 1365 / `22012`, row unchanged |

### Result metadata

CASE result metadata is derived from all `THEN` results and the optional
`ELSE` result. Literal `NULL` result operands do not determine the aggregate
type, but they do make the expression nullable. An omitted `ELSE` acts as an
implicit nullable result. If every result expression is literal `NULL`, the
metadata type is `NULL`.

Verified `mysql --column-type-info -vvv` examples:

| Expression alias | Type | Length | Decimals | Collation | Flags |
| --- | --- | ---: | ---: | --- | --- |
| `CASE WHEN 1 THEN 'yes' ELSE 'no' END AS case_string` | `VAR_STRING` | `12` | `31` | `utf8mb4_0900_ai_ci` | `NOT_NULL` |
| `CASE WHEN 1 THEN 1 ELSE 2 END AS case_int` | `LONGLONG` | `2` | `0` | `binary` | `NOT_NULL BINARY NUM` |
| `CASE WHEN 1 THEN 1.25 ELSE 2 END AS case_decimal` | `NEWDECIMAL` | `5` | `2` | `binary` | `NOT_NULL BINARY NUM` |
| `CASE WHEN 1 THEN NULL ELSE 'fallback' END AS case_null_string` | `VAR_STRING` | `32` | `31` | `utf8mb4_0900_ai_ci` | nullable |
| `CASE WHEN 1 THEN 1 ELSE NULL END AS case_int_null` | `LONGLONG` | `2` | `0` | `binary` | nullable `BINARY NUM` |
| `CASE WHEN 0 THEN 1 END AS omitted_int` | `LONGLONG` | `2` | `0` | `binary` | nullable `BINARY NUM` |
| `CASE WHEN 1 THEN NULL END AS only_null` | `NULL` | `0` | `0` | `binary` | nullable |
| `CASE WHEN n > 0 THEN s ELSE 'none' END AS case_table_string` | `VAR_STRING` | `80` | `31` | `utf8mb4_0900_ai_ci` | nullable |
| `CASE n WHEN 1 THEN n ELSE nullable END AS case_table_int` | `LONG` | `11` | `0` | `binary` | nullable `BINARY NUM` |

Initial MyLite implementation should reuse existing descriptor inference and
extend it with a CASE result aggregation helper. It may stay conservative for
currently deferred type families, but it should match the verified scalar
integer, decimal, string, `NULL`, charset, decimals, and nullability cases
covered by implementation tests.

Default output labels preserve the source expression text when no alias is
provided. Verified labels include:

```text
CASE WHEN 1 THEN 2 END
CASE 1 WHEN 1 THEN 'one' ELSE 'other' END
```

MyLite should follow the existing expression-label policy from Task 23, using
the parser's statement-owned source slices where available.

## MyLite grammar and AST notes

CASE belongs in the shared expression grammar, not in individual statement
grammars. The following Lemon-style snippets describe the intended MyLite
grammar shape and are independently authored:

```lemon
primary_expression ::= case_expression.
primary_expression ::= LPAREN expression RPAREN.
primary_expression ::= literal.
primary_expression ::= qualified_identifier.
primary_expression ::= scalar_function_call.

case_expression ::= CASE expression simple_case_when_list opt_case_else END.
case_expression ::= CASE searched_case_when_list opt_case_else END.

simple_case_when_list ::= simple_case_when_list simple_case_when.
simple_case_when_list ::= simple_case_when.
simple_case_when ::= WHEN expression THEN expression.

searched_case_when_list ::= searched_case_when_list searched_case_when.
searched_case_when_list ::= searched_case_when.
searched_case_when ::= WHEN expression THEN expression.

opt_case_else ::= .
opt_case_else ::= ELSE expression.
```

The actual parser can use different production names, but it must require at
least one `WHEN` clause and must keep simple CASE distinct from searched CASE.
This distinction matters because simple CASE evaluates one base expression and
then performs equality comparisons, while searched CASE evaluates each `WHEN`
expression as a truth predicate.

Recommended AST shape:

- `MYLITE_SQL_AST_CASE_EXPRESSION`
  - mode: simple or searched
  - optional simple base expression child
  - ordered list of `WHEN` arms
  - optional `ELSE` expression
- `MYLITE_SQL_AST_CASE_WHEN_LIST`
- `MYLITE_SQL_AST_CASE_WHEN`
  - comparison/condition expression
  - result expression

Every node should carry source spans for diagnostics and default labels.
Expression binding should visit every child expression even though runtime
evaluation short-circuits.

## Runtime design

Evaluation should be implemented in the shared expression evaluator so every
supported statement context gets the same behavior.

Simple CASE evaluation:

1. Evaluate the base expression once for the current row or rowless context.
2. For each `WHEN` arm in source order:
   - evaluate the arm's comparison expression
   - compare the base value to the comparison value using Task 16 equality
     semantics, including NULL, numeric conversion, collation, and warnings
   - if the comparison is true, evaluate and return that arm's result
3. If no arm matches and `ELSE` exists, evaluate and return `ELSE`.
4. If no arm matches and `ELSE` is absent, return `NULL`.

Searched CASE evaluation:

1. For each `WHEN` arm in source order:
   - evaluate the condition expression
   - convert it to predicate truth using Task 16 / Task 17 truthiness rules
   - if true, evaluate and return that arm's result
2. If no condition is true and `ELSE` exists, evaluate and return `ELSE`.
3. If no condition is true and `ELSE` is absent, return `NULL`.

Unchosen result expressions must not be evaluated. Later `WHEN` conditions or
comparison expressions must not be evaluated after a match. This is observable
through warnings and DML errors.

Warning and error integration should reuse existing expression diagnostics:

- rowless and read-only `SELECT` should return result values plus warning
  records for evaluated warning-producing operands
- `WHERE` and `ORDER BY` in read-only `SELECT` should preserve the current
  Task 17/18 warning lifecycle
- single-table `UPDATE` and `DELETE` should promote evaluated expression
  warnings to errors under the current default strict-mode behavior, matching
  the scalar-function checkpoint
- unsupported child expression shapes should fail during binding with the
  same clause-specific diagnostics used by current projection, `WHERE`,
  `ORDER BY`, assignment, and DML binders

No storage format changes are required. CASE expressions are evaluated from
AST nodes over current row values, literals, supported functions, and supported
operators.

## Implementation plan

1. Add CASE expression parser support in the shared expression grammar and
   keep stored-program CASE statement syntax separate.
2. Add AST node kinds for CASE expressions and WHEN arms, including simple vs
   searched mode.
3. Extend expression support checks and bind walkers so CASE children are
   validated in no-table `SELECT`, one-table `SELECT`, `WHERE`, `ORDER BY`,
   `UPDATE`, and `DELETE` paths.
4. Implement CASE evaluation in the shared expression evaluator with explicit
   short-circuit control.
5. Reuse the Task 16 equality and truthiness helpers for simple and searched
   matching.
6. Add CASE result metadata inference that aggregates `THEN` and `ELSE`
   result descriptors without evaluating expressions.
7. Add runtime tests for rows, metadata, warnings, errors, branch evaluation,
   unsupported child binding, aliases/default labels, and all in-scope
   statement contexts.
8. Update `COMPATIBILITY.md` when implementation and tests land, changing CASE
   expression support from designed to implemented.

## MySQL-runtime-verified implementation tests

The implementation phase should add tests with MySQL-verified expectations for
at least the following cases.

### Parser and binding

| SQL | Expected behavior |
| --- | --- |
| `SELECT CASE WHEN 1 THEN 2 END` | parses; returns `2`; default label is the expression text |
| `SELECT CASE 1 WHEN 1 THEN 'one' ELSE 'other' END` | parses; returns `one` |
| `SELECT CASE ELSE 1 END` | syntax error |
| `SELECT CASE WHEN 1 THEN 1 ELSE missing_col END FROM t LIMIT 1` | binding error 1054 / `42S22` |
| `SELECT CASE WHEN 1 THEN 1 ELSE NO_SUCH_FUNCTION() END` | binding error 1305 / `42000` when function resolution is available |
| `SELECT CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END ELSE 4 END` | nested CASE returns `3` |

### Rowless and table-backed SELECT

| SQL | Expected behavior |
| --- | --- |
| `SELECT CASE WHEN 2 > 1 THEN 'rowless' ELSE 'miss' END` | `rowless` |
| `SELECT id, CASE WHEN n IS NULL THEN 'nil' ELSE CONCAT('n=', n) END FROM t ORDER BY id` | `n=1`, `n=2`, `nil`, `n=0` |
| `SELECT id FROM t WHERE CASE WHEN n = 1 THEN 1 WHEN s IS NULL THEN 1 ELSE 0 END ORDER BY id` | ids `1`, `4` |
| `SELECT id FROM t ORDER BY CASE WHEN s IS NULL THEN 1 ELSE 0 END, CASE WHEN n IS NULL THEN 99 ELSE n END, id` | ids `1`, `2`, `3`, `4` |

### Evaluation order and warnings

| SQL | Expected behavior |
| --- | --- |
| `SELECT CASE WHEN 1 THEN 10 ELSE 1/0 END` | `10`, no warnings |
| `SELECT CASE WHEN 0 THEN 1/0 ELSE 20 END` | `20`, no warnings |
| `SELECT CASE WHEN 1/0 THEN 10 ELSE 20 END` | `20`, one warning 1365 |
| `SELECT CASE WHEN 0 THEN 10 ELSE 1/0 END` | `NULL`, one warning 1365 |
| `SELECT CASE 1 WHEN 1 THEN 10 WHEN 1/0 THEN 20 ELSE 30 END` | `10`, no warnings |
| `SELECT CASE 1 WHEN 0 THEN 10 WHEN 1/0 THEN 20 ELSE 30 END` | `30`, one warning 1365 |
| `SELECT CASE 1/0 WHEN 0 THEN 10 ELSE 20 END` | `20`, one warning 1365 |

### Metadata

Assert symbolic descriptor fields for the verified metadata examples above:

- string CASE returns `VAR_STRING`, `utf8mb4_0900_ai_ci`, decimals `31`
- integer CASE returns `LONGLONG` or table-column integer type, binary
  collation, numeric flags
- decimal CASE returns `NEWDECIMAL` with the verified scale
- literal `NULL` and omitted `ELSE` make the expression nullable
- all-`NULL` result expressions produce the `NULL` metadata type
- CASE expressions have empty origin schema/table/column metadata

### UPDATE and DELETE

| SQL | Expected behavior |
| --- | --- |
| `UPDATE d SET v = CASE WHEN 1 THEN v + 1 ELSE MOD(7,0) END WHERE id = 1` | succeeds; increments `v` |
| `UPDATE d SET v = CASE WHEN 0 THEN v + 1 ELSE MOD(7,0) END WHERE id = 1` | error 1365 / `22012`; row unchanged |
| `UPDATE d SET marker = CASE WHEN id = 2 THEN 'hit' ELSE 'miss' END WHERE CASE WHEN v >= 10 THEN 1 ELSE 0 END ORDER BY CASE WHEN id = 2 THEN 0 ELSE 1 END, id LIMIT 1` | updates only row `2` |
| `DELETE FROM d WHERE CASE WHEN id = 1 THEN 0 ELSE MOD(7,0) END` | succeeds; deletes no rows |
| `DELETE FROM d WHERE CASE WHEN id = 1 THEN MOD(7,0) ELSE 0 END` | error 1365 / `22012`; row unchanged |
| `DELETE FROM d WHERE CASE WHEN v >= 10 THEN 1 ELSE 0 END ORDER BY CASE WHEN marker = 'hit' THEN 0 ELSE 1 END, id LIMIT 1` | deletes the row ordered first by the CASE key |

## Deferred behavior and compatibility risks

- `INSERT ... VALUES` and `INSERT ... SET` CASE expressions are deferred until
  those expression paths are widened beyond the current scalar-function
  checkpoint.
- Exact MySQL metadata aggregation for all mixed temporal, JSON, geometry,
  enum, set, bit, binary, and collation combinations is deferred.
- Exact diagnostics for every malformed CASE expression should improve as the
  parser and diagnostic layer get more MySQL-specific error reporting.
- Explicit collation, binary-string comparison, and non-ASCII collation
  equivalence depend on future collation work.
- CASE expressions containing future side-effecting functions must preserve
  MySQL evaluation order. This spec already requires branch short-circuiting so
  those functions can plug into the same evaluator later.
- Stored-program CASE statements remain a separate compatibility row and must
  not be marked supported by this feature.
