# Baseline Binary, Digest, And Compression UPDATE Contexts

## Goal

This slice admits existing row-scalar binary, digest, compression, and
random-byte functions in compatible non-key single-table `UPDATE` assignment
values and the matching `INSERT ... ON DUPLICATE KEY UPDATE` assignment path.
It does not add new function semantics, operand domains, key-target assignment
support, generated columns, defaults, grouping, ordering, or arbitrary
expression evaluation.

Covered functions:

- `MD5()`, `SHA()`, `SHA1()`, and `SHA2()`;
- `COMPRESS()`, `UNCOMPRESS()`, and `UNCOMPRESSED_LENGTH()`;
- `RANDOM_BYTES()`.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>

The exact expected rows, changed-row counts, warning counts, and error counts
are captured in the existing MySQL comparison scripts:

- `packages/libmylite/tests/mysql_baseline_digest_functions_expectations.sh`
- `packages/libmylite/tests/mysql_baseline_compression_random_functions_expectations.sh`

## Semantics

MyLite may plan a covered function as a row-scalar assignment value when the
existing assignment rules accept the target column. The target must be a
compatible non-key column, must not be `AUTO_INCREMENT`, and must use the
existing target storage conversion path.

Function results, `NULL` behavior, warnings, diagnostics, binary/text result
bytes, random-byte length validation, and metadata remain delegated to the
existing baseline function implementations. This slice only changes where the
existing row-scalar planned expressions are admitted.

For nondeterministic `RANDOM_BYTES(row_value)` assignments, tests assert
changed-row counts and result byte lengths/non-`NULL` behavior rather than raw
random bytes.

## Syntax

The parser retry layer may replace a direct covered function call in a
single-table `UPDATE` assignment value with a row-scalar placeholder when the
argument list contains a row operand. The same placeholder admission applies to
the existing compatible duplicate-key update assignment path.

Intended admission shape:

```lemon
update_value(A) ::= row_scalar_binary_digest_compression_function_call(B). {
    A = B;
}
```

For duplicate-key assignments, the retry layer uses an integer placeholder
accepted by the existing `insert_value` grammar and replaces that placeholder
with the retained original AST after parsing.

## SQLite Integration

This slice uses MyLite-side parsing/planning and existing SQLite scalar UDFs.
It does not require a SQLite fork hook, file-format change, or new dependency.

## Test Plan

- Extend the digest MySQL expectation script with single-table `UPDATE` and
  duplicate-key assignments for `MD5()`, `SHA()`, `SHA1()`, and `SHA2()`.
- Extend the compression/random MySQL expectation script with single-table
  `UPDATE` and duplicate-key assignments for `COMPRESS()`, `UNCOMPRESS()`,
  `UNCOMPRESSED_LENGTH()`, and `RANDOM_BYTES()`.
- Add matching C runtime coverage to the existing digest and
  compression/random test targets.
- Run shell syntax checks, the two MySQL expectation scripts, the two runtime
  CTest targets, diff checks, and the full `cmake --workflow --preset check`
  gate.

## Compatibility Impact

The covered detailed rows should no longer list compatible non-key
single-table `UPDATE` or duplicate-key assignments as missing. They remain
yellow where independent gaps still exist, including unsupported AES/password
functions, broad expression operands, ordering/grouping/default/generated-column
contexts, exact expression metadata, replication warnings, and broader binary
or collation behavior.
