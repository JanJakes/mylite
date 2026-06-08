# Baseline MySQL hex predicate literals

## Scope

This slice admits MySQL hexadecimal literals in the descriptor-backed predicate
positions MyLite already supports for integer-family comparisons:

- `qualified_column = 0x...`
- `qualified_column <=> 0x...`
- `qualified_column <> 0x...` and `!=`
- `<`, `<=`, `>`, `>=`
- `BETWEEN 0x... AND 0x...`
- `IN (0x... [, ...])`

The runtime scope is numeric comparison against currently supported
integer-family descriptors. Hex literals keep the existing MyLite literal AST
kind and use the existing descriptor conversion helpers for numeric predicate
planning.

## MySQL behavior

Primary references:

- https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html

MySQL 8.4 hexadecimal literals use `X'...'`, `x'...'`, or `0x...`. Hex digits
are case-insensitive. The `0x` prefix is case-sensitive; `0X...` is not the
same literal form. `X'...'` requires an even number of hex digits. `0x...` with
an odd number of digits behaves as though a leading zero was added. By default,
hexadecimal literals are binary strings, but in numeric contexts MySQL treats
them as unsigned 64-bit integer values.

Observed MySQL 8.4.9 probe:

```sql
DROP DATABASE IF EXISTS mylite_hex_predicate_probe;
CREATE DATABASE mylite_hex_predicate_probe;
USE mylite_hex_predicate_probe;
CREATE TABLE numbers (id INT NOT NULL, i INT, iu INT UNSIGNED, n INT NULL);
INSERT INTO numbers VALUES
  (1, 1, 1, NULL),
  (2, 65, 65, NULL),
  (3, 2730, 2730, NULL),
  (4, NULL, NULL, NULL);
SELECT 'eq', GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i = 0x1;
SELECT 'in', GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i IN (0x1, 0x41, 0xaaa);
SELECT 'between', GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE i BETWEEN 0x1 AND 0x41;
SELECT 'null-safe', GROUP_CONCAT(id ORDER BY id) FROM numbers WHERE n <=> 0x0;
SELECT 'odd', 0xaaa+0, HEX(0xaaa), LENGTH(0xaaa);
DROP DATABASE mylite_hex_predicate_probe;
```

Results:

```text
eq        1
in        1,2,3
between   1,2
null-safe NULL
odd       2730  0AAA  2
```

## MyLite behavior

MyLite already lexes `0x...`, `X'...'`, and `x'...'` as `HEX_LITERAL` and
already uses `MYLITE_SQL_AST_LITERAL_HEX` for scalar projection, row DML values,
and compatible descriptor conversions. This slice extends only predicate
grammar admission and uses the existing predicate conversion path.

The accepted grammar is:

```lemon
predicate_in_value ::= HEX_LITERAL.
predicate_range_value ::= HEX_LITERAL.
predicate_comparison_value ::= HEX_LITERAL.
predicate_scalar_literal ::= HEX_LITERAL.
```

`predicate_scalar_literal` admission covers literal-left and scalar-literal
truth/comparison forms such as `0x1 = 1` where the current scalar predicate
executor already supports the value domain.

## Non-goals

This slice does not add:

- binary-string descriptor comparisons for `BINARY`, `VARBINARY`, or BLOB
  family columns;
- character-string predicate conversion from hex literals;
- `_binary` introducer semantics for predicate comparisons;
- full unsigned 64-bit comparison coverage beyond MyLite's current descriptor
  conversion envelope;
- bit-literal predicate expansion beyond the existing admitted subset;
- bitwise operator semantics.

Those require broader type-comparison and collation work.

## Tests

Tests cover:

- parser acceptance for comparison, `BETWEEN`, `IN`, and scalar-literal
  predicate positions;
- runtime equality, `IN`, and `BETWEEN` over supported integer descriptors;
- replacement of the prior negative `WHERE i IN (0x1)` expectation with the
  MySQL-verified positive behavior;
- corpus score improvement for the large `WHERE ujis=0x...` update bucket.
