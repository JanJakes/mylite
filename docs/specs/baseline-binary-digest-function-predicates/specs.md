# Baseline Binary And Digest Function Predicates

## Summary

This slice extends already implemented MySQL-compatible binary, compression,
random-byte, and digest functions from row-backed projection into descriptor
`WHERE` predicate expressions. The covered functions are:

- `MD5()`, `SHA()`, `SHA1()`, and `SHA2()`
- `COMPRESS()`, `UNCOMPRESS()`, and `UNCOMPRESSED_LENGTH()`
- `RANDOM_BYTES()`

The function semantics, result bytes, warnings, errors, and metadata remain
owned by the existing baseline function slices. This slice changes where those
planned row-scalar function expressions may appear.

References:

- MySQL 8.4 Reference Manual, "Encryption and Compression Functions":
  <https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html>
- Runtime probes against MySQL `8.4.9` in the local `mylite-mysql-849`
  comparison container.

## MySQL 8.4.9 Behavior

The MySQL runtime accepts these function calls as ordinary expressions in
`WHERE` predicates. Observed probes:

```sql
CREATE TABLE digest_t(id INT, v VARCHAR(10), vb VARBINARY(10));
INSERT INTO digest_t VALUES (1,'abc',X'616263'),(2,'def',X'646566'),(3,NULL,NULL);

SELECT GROUP_CONCAT(id ORDER BY id)
FROM digest_t
WHERE MD5(v) = MD5('abc');
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM digest_t
WHERE SHA2(v,256) BETWEEN SHA2('abc',256) AND SHA2('def',256);
-- 1,2

SELECT GROUP_CONCAT(id ORDER BY id)
FROM digest_t
WHERE SHA2(v,NULL) IS NULL;
-- 1,2,3

CREATE TABLE comp_t(id INT, v VARCHAR(10), vb VARBINARY(10));
INSERT INTO comp_t VALUES (1,'abc',X'610062'),(2,'',X''),(3,NULL,NULL);

SELECT GROUP_CONCAT(id ORDER BY id)
FROM comp_t
WHERE UNCOMPRESS(COMPRESS(v)) = 'abc';
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM comp_t
WHERE UNCOMPRESSED_LENGTH(COMPRESS(vb)) BETWEEN 1 AND 3;
-- 1

SELECT GROUP_CONCAT(id ORDER BY id)
FROM comp_t
WHERE COMPRESS(v) IS NULL;
-- 3

SELECT COUNT(*)
FROM comp_t
WHERE RANDOM_BYTES(id) IS NOT NULL;
-- 3
```

`NULL` propagation and volatile evaluation follow the existing baseline
function semantics. `RANDOM_BYTES(id)` is evaluated per row.

## Supported Scope

MyLite supports descriptor-backed `WHERE` predicates where the left side is one
of the covered row-scalar function expressions and the predicate is:

- comparison against an admitted scalar or row-scalar value expression;
- `IS NULL` or `IS NOT NULL`;
- `BETWEEN` or `NOT BETWEEN` with admitted scalar or row-scalar bounds;
- nonvolatile row-scalar `IN` / `NOT IN` literal/function lists through
  [baseline row-scalar IN predicates](../baseline-row-scalar-in-predicates/specs.md).

The expression operands remain the same as in the existing function slices:
supported descriptor columns, supported scalar literals, nested supported
row-scalar function expressions, and the documented `SHA2()` hash-length
literals.

## Intentionally Unsupported

This slice does not add:

- arbitrary expression arguments beyond the current row-scalar expression
  planner;
- `ORDER BY`, `GROUP BY`, `HAVING`, window frame, generated-column, default, or
  broad DML assignment contexts outside the compatible row-scalar update subset
  documented by
  [baseline binary, digest, and compression UPDATE contexts](../baseline-binary-digest-compression-update-contexts/specs.md);
- new AES, password, or statement digest functions;
- new SQLite fork hooks.

## Grammar

The grammar admits the existing row-scalar binary/digest function expressions
in predicate subjects, comparison values, and range bounds:

```lemon
predicate_row_scalar_expression(A) ::= row_scalar_string_predicate_expression(B).
predicate_comparison_value(A) ::= row_scalar_string_predicate_expression(B).
predicate_range_value(A) ::= row_scalar_string_predicate_expression(B).

row_scalar_string_predicate_expression(A) ::= MD5(T) LPAREN expression(B) RPAREN(R).
row_scalar_string_predicate_expression(A) ::= SHA2(T) LPAREN expression(B) COMMA
    expression(C) RPAREN(R).
row_scalar_string_predicate_expression(A) ::= COMPRESS(T) LPAREN expression(B) RPAREN(R).
row_scalar_string_predicate_expression(A) ::= UNCOMPRESS(T) LPAREN expression(B) RPAREN(R).
row_scalar_string_predicate_expression(A) ::= UNCOMPRESSED_LENGTH(T) LPAREN expression(B)
    RPAREN(R).
row_scalar_string_predicate_expression(A) ::= RANDOM_BYTES(T) LPAREN expression(B) RPAREN(R).
```

## Architecture

The implementation is a planner-surface expansion. The functions already lower
to MyLite-owned SQLite scalar callbacks via public `sqlite3_create_function_v2()`
registration. The predicate planner now admits the covered AST function nodes
through the shared row-scalar predicate path. Binary-valued predicates bind
literal comparison values as blobs, while digest and text-valued functions bind
ordinary text values.

No catalog, storage, ABI, dependency, or file-format change is required.

## Tests

Tests cover:

- MySQL-runtime-verified digest comparison, digest `BETWEEN`, and digest
  `IS NULL` predicates.
- MySQL-runtime-verified compression/decompression comparison,
  `UNCOMPRESSED_LENGTH()` `BETWEEN`, `COMPRESS()` `IS NULL`, and
  row-volatile `RANDOM_BYTES(id) IS NOT NULL` predicates.
- Row-scalar `IN` / `NOT IN` coverage lives in
  [baseline row-scalar IN predicates](../baseline-row-scalar-in-predicates/specs.md).
- MyLite C runtime assertions for the same supported predicate contexts.
