# Baseline expression COLLATE predicates and order keys

## Scope

This slice admits explicit postfix `COLLATE` clauses on the string-expression
operands that most often appear in the MySQL parser corpus:

- descriptor-column and supported row-scalar left operands in `WHERE`
  comparison predicates;
- descriptor-column and supported row-scalar `LIKE` operands and patterns;
- descriptor-column and supported row-scalar `ORDER BY` keys for ordinary
  single-table `SELECT`;
- scalar string literals and supported `CONVERT(... USING charset)` operands
  in those contexts.

The runtime target is MySQL 8.4.9 behavior for the shared limited Unicode 0900
collation subset and ASCII data under legacy collations that MyLite already
models as scalar metadata, plus the additional frequent
`utf8mb4_0900_as_cs`, `utf8mb4_0900_as_ci`, `latin1_general_ci`,
`latin1_general_cs`, `latin1_german1_ci`, and `latin1_german2_ci` names.

## Sources

- MySQL 8.4 Reference Manual, "Using COLLATE in SQL Statements":
  https://dev.mysql.com/doc/refman/8.4/en/charset-collate.html
- MySQL 8.4 Reference Manual, "Character Set and Collation Compatibility":
  https://dev.mysql.com/doc/refman/8.4/en/charset-collation-compatibility.html
- MySQL 8.4 Reference Manual, "Collation Coercibility in Expressions":
  https://dev.mysql.com/doc/refman/8.4/en/charset-collation-coercibility.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_expression_collate_predicates_expectations.sh`.

## Semantics

MySQL treats an explicit `COLLATE` clause as the strongest collation source for
comparison resolution. For this baseline MyLite supports that behavior for the
current row-scalar envelope:

- unknown collation names return MySQL-style `1273 / HY000`;
- a known collation whose character set does not match the operand character
  set returns MySQL-style `1253 / 42000`;
- case-insensitive 0900 collations use MyLite's shared UTF-8 service with the
  selected accent-sensitivity level;
- binary collations use SQLite `BINARY`;
- case-sensitive collations use MyLite's registered `utf8mb4_0900_as_cs`
  SQLite collation with Unicode case weights and lower-before-upper ordering;
- explicit collation on the left or right side overrides the default string-key
  collation that MyLite appends for uncollated string expressions.

The implementation is intentionally narrower than UCA 9.0. It covers Unicode
case folding, common compatibility expansions, common Latin-1 canonical accent
equivalence, 0900 case/accent sensitivity, deterministic invalid-byte ordering,
and NO PAD trailing spaces. It does not implement complete Unicode weight
strings, contractions, locale-specific ordering, general PAD SPACE execution,
or general coercibility resolution between two incompatible explicit
collations beyond the validation needed for this operand envelope. `LIKE`
retains its separately documented ASCII behavior.

## Grammar

The existing general expression grammar already has a postfix `COLLATE` node.
The predicate and order-key grammars admit the node in the narrow contexts they
currently own:

```lemon
predicate_collatable_expression ::= qualified_identifier.
predicate_collatable_expression ::= predicate_scalar_literal.
predicate_collatable_expression ::= cast_convert_expression.
predicate_collatable_expression ::= CONCAT '(' function_argument_list ')'.
predicate_collate_expression ::=
    predicate_collatable_expression COLLATE option_name.

predicate_comparison_value ::= predicate_collate_expression.
predicate_like_pattern ::= predicate_collate_expression.
predicate_atom ::= predicate_collate_expression comparison_operator predicate_comparison_value.
predicate_atom ::= predicate_collate_expression LIKE predicate_like_pattern escape_opt.

select_order_key ::= predicate_collate_expression.
```

## Runtime design

MyLite represents the admitted `COLLATE` node as a row-scalar expression with
one child expression and a validated MySQL collation name. SQL generation wraps
the child expression and appends a SQLite collation name selected from the
MySQL collation metadata. This keeps the compatibility behavior in MyLite while
using SQLite's existing expression collation machinery.

No SQLite fork hook is needed for this slice.

## Tests

Focused tests cover:

- parser acceptance for right-side, left-side, `LIKE`, and `ORDER BY`
  `COLLATE` forms;
- runtime equality and `LIKE` behavior for `utf8mb4_0900_ai_ci` and
  `utf8mb4_0900_as_cs`;
- Unicode case/accent sensitivity, precomposed/decomposed common Latin-1
  accents, accent position, grouping, and unique-key behavior for supported
  0900 collations;
- runtime `latin1` converted literal comparison against a `latin1` column;
- MySQL-shaped diagnostics for invalid charset/collation combinations and
  unknown collation names;
- MySQL 8.4.9 expectation output for the same behaviors.

## Deferred behavior

This slice does not add expression-level `COLLATE` to grouping, distinct,
aggregate arguments outside the documented `GROUP_CONCAT(DISTINCT)` path,
window definitions, DML assignments, arbitrary expression trees, `REGEXP`
pattern collation semantics, complete UCA 9.0 collation parity, or full
coercibility conflict diagnostics.
