# Baseline WordPress Meta Cast Predicates

## Scope

This slice extends descriptor-backed `WHERE` predicates for the WordPress meta
query shapes that compare row-backed `CAST()` results:

```sql
CAST(meta_key AS BINARY) REGEXP BINARY 'AAA_FOO_.*'
CAST(meta_key AS BINARY) NOT REGEXP BINARY 'AAA_FOO_.*'
CAST(meta_value AS SIGNED) BETWEEN '1' AND '3'
CAST(meta_value AS DECIMAL(10,2)) = '.300'
CAST(meta_value AS DECIMAL(10,2)) LIKE '%.3%'
CAST(meta_value AS DECIMAL(10,10)) BETWEEN '0.23409845' AND '.31'
```

The implementation is limited to descriptor-backed `SELECT` predicates,
including the existing joined-source and grouped-aggregate source envelopes
used by WordPress. It does not add general expression projection, row-backed
pattern operands, parameter operands, or full expression-level cast semantics.

## Compatibility Authority

- MySQL 8.4 Reference Manual, cast functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
- MySQL 8.4 Reference Manual, regular expressions:
  <https://dev.mysql.com/doc/refman/8.4/en/regexp.html>
- MySQL 8.4 Reference Manual, comparison operators:
  <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- Runtime expectations are verified against MySQL 8.4.9 with
  `packages/libmylite/tests/mysql_baseline_wordpress_meta_cast_predicates_expectations.sh`.

## Syntax

The slice admits these additional predicate atoms:

```lemon
predicate_atom ::= cast_convert_expression REGEXP STRING.
predicate_atom ::= cast_convert_expression RLIKE STRING.
predicate_atom ::= cast_convert_expression REGEXP BINARY STRING.
predicate_atom ::= cast_convert_expression RLIKE BINARY STRING.
predicate_atom ::= cast_convert_expression NOT REGEXP STRING.
predicate_atom ::= cast_convert_expression NOT RLIKE STRING.
predicate_atom ::= cast_convert_expression NOT REGEXP BINARY STRING.
predicate_atom ::= cast_convert_expression NOT RLIKE BINARY STRING.
predicate_atom ::= cast_convert_expression LIKE predicate_like_pattern.
predicate_atom ::= cast_convert_expression NOT LIKE predicate_like_pattern.
predicate_atom ::= cast_convert_expression BETWEEN predicate_range_value AND predicate_range_value.
predicate_atom ::= cast_convert_expression NOT BETWEEN predicate_range_value AND predicate_range_value.
```

`REGEXP` and `RLIKE` remain synonyms. `NOT REGEXP`, `NOT RLIKE`, and
`NOT BETWEEN` are represented as a normal predicate wrapped in logical `NOT`,
matching existing MyLite predicate planning.

## Semantics

### Binary Regex Predicates

`CAST(expr AS BINARY)` converts the row value to binary string semantics. For
the admitted `REGEXP` / `RLIKE` predicate subset, MyLite treats a binary cast
left operand as case-sensitive ASCII regex matching. The `REGEXP BINARY
'pattern'` and `RLIKE BINARY 'pattern'` spellings are admitted for WordPress
syntax compatibility; this slice does not add standalone pattern-side binary
semantics. Nonbinary descriptor regex predicates keep the existing
case-insensitive ASCII subset.

Pattern operands must be string literals in MyLite's baseline ASCII regular
expression subset. `NULL` row values do not match. Invalid or unsupported
patterns return the same MyLite/MySQL-compatible regex diagnostics used by the
existing descriptor-column `REGEXP` path.

### Signed Cast BETWEEN

`CAST(expr AS SIGNED) BETWEEN low AND high` applies MyLite's existing row-scalar
signed conversion for the left operand and compares it against inclusive
signed-64 literal bounds. Quoted integer strings are accepted because WordPress
emits string bounds. `NOT BETWEEN` is the logical negation of the same predicate.

### Decimal Cast Comparisons

`CAST(expr AS DECIMAL(M,D))` comparison and `BETWEEN` predicates compare the
row-backed value and literal operands numerically for the admitted WordPress
meta-query shapes. String literals such as `'.300'` and `'.31'` are accepted as
decimal predicate operands and are planned as numeric comparisons.

`CAST(expr AS DECIMAL(M,D)) LIKE pattern` formats the row-backed value with the
declared decimal scale before applying the existing MyLite `LIKE` pattern
subset. This covers WordPress queries such as `LIKE '%.3%'` and `NOT LIKE
'%.3%'` without adding general expression-level `LIKE` support.

This slice intentionally does not implement full fixed-precision decimal
predicate arithmetic. It is limited to finite base-10 literal forms that
WordPress emits for meta queries. Unsupported decimal predicate literals produce
a deterministic unsupported-feature diagnostic rather than falling back to text
ordering.

## Diagnostics

- Unsupported left operands continue to use the row-scalar `CAST()` predicate
  diagnostics.
- Unsupported regex patterns use the existing regex predicate diagnostics.
- Unsupported signed or decimal literal bounds return MyLite unsupported
  diagnostics that identify the literal subset.

## Non-Goals

- Full expression-level `LIKE`, `REGEXP`, `RLIKE`, or `BETWEEN`.
- Row-backed regex patterns.
- ICU/full-Unicode regular expression semantics.
- General binary-string storage predicates outside the admitted cast shape.
- Full exact-decimal arithmetic, warning propagation, or protocol metadata for
  row-backed decimal predicates.
- DML assignment support or expression indexes.
