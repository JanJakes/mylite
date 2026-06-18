# Baseline Row Bitwise Expressions

## Summary

This slice extends MyLite's numeric bitwise operator support from no-source
scalar expressions to single-table row-scalar expressions:

```sql
SELECT bitwise_expression[, ...] FROM table
SELECT ... FROM table WHERE (bitwise_expression) comparison value
SELECT ... FROM table ORDER BY bitwise_expression
```

The supported operators are unary `~` and binary `&`, `|`, `^`, `<<`, and
`>>`. Operands use the current integer-like row expression domain: integer
descriptor columns, decimal integer literals with optional sign, booleans,
`NULL`, nested bitwise expressions, supported integer arithmetic expressions,
integer-returning numeric functions, supported string length functions,
`UNIX_TIMESTAMP()`, and numeric temporal extractor functions. In predicate
contexts, the comparison value side uses the same predicate value envelope as
other row-scalar comparison predicates.

The feature deliberately remains numeric-only. It does not implement binary
string bit operations, string/decimal/float/hex/bit row operand coercion,
bitwise children inside row arithmetic parents, DML assignment expressions,
generated/default expressions, expression metadata, parameters, user variables,
or broader multi-table expression semantics beyond the existing row-scalar
source-index machinery.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Bit functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - `SELECT` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_row_bitwise_expressions_expectations.sh`.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- numeric row-backed bitwise operators produce unsigned 64-bit decimal text for
  non-`NULL` results;
- signed integer column and literal operands are converted to the unsigned
  64-bit numeric bit-operation domain;
- `TRUE` and `FALSE` behave as `1` and `0`;
- `NULL` operands produce `NULL`;
- shift counts greater than or equal to 64, including negative signed values
  after unsigned conversion, return `0`;
- bitwise expressions work as projection items, parenthesized comparison
  predicate subjects, parenthesized null-safe comparison predicate subjects,
  and direct `ORDER BY` keys for the documented integer-domain operand set;
- MySQL accepts broader operands such as strings, decimals, floats, hex
  literals, bit literals, and binary strings. Those remain deferred.

## Ownership Boundaries

- Public API: unchanged. Supported queries return values through the existing
  result API and text-result conventions.
- Parser/AST: generic expression parsing already contains the bitwise operators
  for select-list projection expressions. On syntax-error retry, MyLite replaces
  verified parenthesized bitwise predicate subjects and direct `ORDER BY`
  bitwise keys with placeholders, parses the statement through the existing
  grammar, then parses the original token range through the generic expression
  grammar and clones that AST back into the statement.
- Analyzer/planner: MyLite recognizes row bitwise roots and lowers them to a
  planned row-scalar expression kind for projection, comparison predicate, and
  `ORDER BY` planning. Non-bitwise leaves are planned through existing
  integer-like argument handling.
- SQLite execution: row evaluation is delegated to SQLite using MyLite
  registered scalar functions. SQLite still performs table scanning and result
  production; MyLite does not materialize row result sets in memory for these
  expressions.
- Catalog/storage: no descriptor mutation, file-format change, or catalog
  schema change.
- SQLite fork: no targeted fork hook is needed. The implementation uses public
  SQLite scalar-function registration.

## Syntax

The admitted projection expression shape is:

```text
row_bitwise_expression:
    ~ row_bitwise_operand
  | row_bitwise_operand ^ row_bitwise_operand
  | row_bitwise_operand << row_bitwise_operand
  | row_bitwise_operand >> row_bitwise_operand
  | row_bitwise_operand & row_bitwise_operand
  | row_bitwise_operand | row_bitwise_operand

row_bitwise_operand:
    qualified_identifier
  | integer_literal
  | TRUE
  | FALSE
  | NULL
  | + integer_literal
  | - integer_literal
  | ( row_bitwise_expression )
```

Supported contexts:

```sql
SELECT row_bitwise_expression FROM table
SELECT row_bitwise_expression AS alias FROM table
SELECT ... FROM table WHERE (row_bitwise_expression) comparison_operator predicate_value
SELECT ... FROM table WHERE (row_bitwise_expression) <=> NULL
SELECT ... FROM table ORDER BY row_bitwise_expression [ASC|DESC]
```

### MyLite Lemon Snippet

No new Lemon grammar is introduced. The select-list context already uses the
generic expression grammar, and the retry path admits the predicate/order forms
that the current predicate and order-key grammar does not parse directly:

```text
row_bitwise_predicate_retry ::=
    ( row_bitwise_expression ) comparison_operator predicate_value
  | ( row_bitwise_expression ) <=> NULL

row_bitwise_order_retry ::=
    ORDER BY row_bitwise_expression [ ASC | DESC ]
```

The retry implementation leaves the committed Lemon grammar unchanged. It is
deliberately narrower than MySQL's full expression grammar while admitting the
documented integer-domain row bitwise roots.

## Runtime Semantics

Planning rules:

1. Admit a row bitwise expression only when its root is unary `~` or one of the
   five supported binary bitwise operators, whether the expression appears in a
   projection item, parenthesized comparison predicate subject, or direct
   `ORDER BY` key.
2. Plan nested bitwise children recursively.
3. Plan non-bitwise leaves through the existing integer-like row argument
   helper.
4. Reject unsupported leaves with a clear compatibility diagnostic rather than
   falling through to SQLite syntax.

SQLite execution rules:

1. The generated SQL calls MyLite scalar UDFs:
   `_mylite_bitwise_not`, `_mylite_bitwise_and`, `_mylite_bitwise_or`,
   `_mylite_bitwise_xor`, `_mylite_bitwise_lshift`, and
   `_mylite_bitwise_rshift`.
2. UDF arguments with SQLite `NULL` type produce `NULL`.
3. Integer arguments are interpreted by their unsigned 64-bit two's-complement
   representation.
4. Text arguments are accepted only to consume nested UDF results; they must be
   canonical unsigned decimal values in the `uint64_t` range.
5. Shift counts greater than or equal to 64 return `0`.
6. Non-`NULL` results that fit `INT64_MAX` are returned as SQLite integer
   values so later contexts can preserve numeric affinity where applicable.
   Larger unsigned values are returned as canonical unsigned decimal text so
   values such as `~0` match MySQL result text.

The implementation intentionally does not add a SQLite fork hook. The UDF
surface is sufficient for this feature because SQLite can still own scan,
row evaluation, and result production; MyLite only supplies the MySQL-specific
operation semantics.

## Tests

The C runtime test covers:

- projection of all six operators over row columns and literals;
- parenthesized comparison predicates, null-safe comparison predicates, and
  direct `ORDER BY` keys over row bitwise expressions;
- unsigned formatting for `~0`, `~-1`, negative operands, and high shifts;
- nested row bitwise expressions;
- `NULL` propagation;
- catalog-generation and file-preamble safety for read-only row expressions;
- independent connection handles; and
- unsupported direct string row operands.

The MySQL expectation script verifies the expected rows and the accepted-but-
deferred broader MySQL context and operand surface against MySQL 8.4.9.

## Known Gaps

- Binary-string bit operations are not implemented.
- Row string/decimal/float/hex/bit operand coercion is not implemented.
- Unparenthesized direct bitwise predicate subjects such as
  `WHERE id & mask >= 3` remain deferred.
- Direct bitwise predicate values on the right side of comparisons are not yet
  implemented unless admitted through the existing row-scalar predicate value
  envelope.
- Bitwise expressions under arithmetic, logical, and assignment parents remain
  deferred outside the projection context listed above.
- Exact expression metadata is not implemented.
- Warning-order parity for unsupported warning-producing row leaves is deferred
  because this slice admits only integer-domain operands.
